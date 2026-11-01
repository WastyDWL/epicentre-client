#include "core/a2s.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <charconv>
#include <cstring>
#include <thread>

#include "core/js_api.h"

namespace nc::a2s {

namespace {

constexpr int kTimeoutMs = 1500;

// Bounds-checked reader over one packet.
struct Reader {
    const std::vector<unsigned char>& b;
    size_t pos = 0;
    bool ok = true;

    unsigned char u8() {
        if (pos + 1 > b.size()) return ok = false, 0;
        return b[pos++];
    }
    uint32_t le32() {
        if (pos + 4 > b.size()) return ok = false, 0;
        uint32_t v = b[pos] | b[pos + 1] << 8 | b[pos + 2] << 16 | static_cast<uint32_t>(b[pos + 3]) << 24;
        pos += 4;
        return v;
    }
    std::string str() {
        size_t end = pos;
        while (end < b.size() && b[end]) ++end;
        if (end >= b.size()) return ok = false, std::string();
        std::string s(reinterpret_cast<const char*>(b.data() + pos), end - pos);
        pos = end + 1;
        return s;
    }
};

std::vector<unsigned char> header(char type) { return {0xFF, 0xFF, 0xFF, 0xFF, static_cast<unsigned char>(type)}; }

void put_le32(std::vector<unsigned char>& b, uint32_t v) {
    for (int i = 0; i < 4; ++i) b.push_back(static_cast<unsigned char>(v >> (8 * i)));
}

std::optional<ServerInfo> parse_source_info(Reader& r) {
    ServerInfo i;
    r.u8();  // protocol
    i.name = r.str();
    i.map = r.str();
    i.game_dir = r.str();
    r.str();  // game description
    r.u8(), r.u8();  // app id
    i.players = r.u8();
    i.max_players = r.u8();
    i.bots = r.u8();
    r.u8(), r.u8();  // server type, environment
    i.password = r.u8() != 0;
    i.vac = r.u8() != 0;
    if (!r.ok) return std::nullopt;
    return i;
}

std::optional<ServerInfo> parse_goldsrc_info(Reader& r) {
    ServerInfo i;
    r.str();  // address
    i.name = r.str();
    i.map = r.str();
    i.game_dir = r.str();
    r.str();  // description
    i.players = r.u8();
    i.max_players = r.u8();
    r.u8(), r.u8(), r.u8();  // protocol, server type, environment
    i.password = r.u8() != 0;
    if (r.u8() == 1) {  // mod info
        r.str(), r.str();
        r.u8();
        r.le32(), r.le32();
        r.u8(), r.u8();
    }
    i.vac = r.u8() != 0;
    i.bots = r.u8();
    if (!r.ok) return std::nullopt;
    return i;
}

std::optional<std::vector<Player>> parse_players(Reader& r) {
    int count = r.u8();
    std::vector<Player> players;
    for (int k = 0; k < count && r.ok; ++k) {
        Player p;
        r.u8();  // index
        p.name = r.str();
        p.score = static_cast<int32_t>(r.le32());
        uint32_t bits = r.le32();
        float seconds;
        std::memcpy(&seconds, &bits, 4);
        p.seconds = static_cast<int>(seconds);
        players.push_back(std::move(p));
    }
    if (!r.ok) return std::nullopt;
    return players;
}

// One request/response round trip on a UDP socket; empty on timeout.
std::vector<unsigned char> exchange(SOCKET s, const sockaddr_in& to, const std::vector<unsigned char>& req) {
    sendto(s, reinterpret_cast<const char*>(req.data()), static_cast<int>(req.size()), 0,
           reinterpret_cast<const sockaddr*>(&to), sizeof(to));
    std::vector<unsigned char> buf(4096);
    int n = recv(s, reinterpret_cast<char*>(buf.data()), static_cast<int>(buf.size()), 0);
    if (n <= 0) return {};
    buf.resize(n);
    return buf;
}

// Sends `make(challenge)`, answering one challenge round, and returns the final reply.
template <class Make>
Reply query(const std::string& address, Make make, std::optional<uint32_t> first) {
    static const bool wsa = [] {
        WSADATA d;
        return WSAStartup(MAKEWORD(2, 2), &d) == 0;
    }();
    std::optional<Address> a = parse_address(address);
    if (!wsa || !a) return {};
    sockaddr_in to{};
    to.sin_family = AF_INET;
    to.sin_port = htons(a->port);
    if (inet_pton(AF_INET, a->host.c_str(), &to.sin_addr) != 1) {
        addrinfo hints{}, *res = nullptr;
        hints.ai_family = AF_INET;
        if (getaddrinfo(a->host.c_str(), nullptr, &hints, &res) != 0 || !res) return {};
        to.sin_addr = reinterpret_cast<sockaddr_in*>(res->ai_addr)->sin_addr;
        freeaddrinfo(res);
    }
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return {};
    DWORD timeout = kTimeoutMs;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    Reply r = parse(exchange(s, to, make(first)));
    if (r.challenge) r = parse(exchange(s, to, make(*r.challenge)));
    closesocket(s);
    return r;
}

}  // namespace

std::vector<unsigned char> info_request(std::optional<uint32_t> challenge) {
    std::vector<unsigned char> b = header('T');
    const char payload[] = "Source Engine Query";
    b.insert(b.end(), payload, payload + sizeof(payload));
    if (challenge) put_le32(b, *challenge);
    return b;
}

std::vector<unsigned char> players_request(uint32_t challenge) {
    std::vector<unsigned char> b = header('U');
    put_le32(b, challenge);
    return b;
}

Reply parse(const std::vector<unsigned char>& packet) {
    Reply out;
    if (packet.size() < 5 || packet[0] != 0xFF || packet[1] != 0xFF || packet[2] != 0xFF || packet[3] != 0xFF)
        return out;  // split packets are not needed for these small replies
    Reader r{packet, 5};
    switch (packet[4]) {
        case 'A': {
            uint32_t c = r.le32();
            if (r.ok) out.challenge = c;
            break;
        }
        case 'I': out.info = parse_source_info(r); break;
        case 'm': out.info = parse_goldsrc_info(r); break;
        case 'D': out.players = parse_players(r); break;
    }
    return out;
}

std::optional<Address> parse_address(std::string_view text) {
    if (text.empty()) return std::nullopt;
    Address a;
    size_t colon = text.rfind(':');
    a.host = std::string(text.substr(0, colon));
    if (colon != std::string_view::npos) {
        int port = 0;
        auto [p, ec] = std::from_chars(text.data() + colon + 1, text.data() + text.size(), port);
        if (ec != std::errc() || p != text.data() + text.size() || port <= 0 || port > 65535) return std::nullopt;
        a.port = static_cast<uint16_t>(port);
    }
    if (a.host.empty()) return std::nullopt;
    return a;
}

std::string info_json(const ServerInfo& i, const std::string& address) {
    int humans = i.players - i.bots;
    return "{\"appId\":10,\"gameDir\":" + json_string(i.game_dir) + ",\"address\":" + json_string(address) +
           ",\"hostname\":" + json_string(i.name) + ",\"map\":" + json_string(i.map) +
           ",\"playersOnline\":" + std::to_string(humans < 0 ? 0 : humans) + ",\"botsOnline\":" +
           std::to_string(i.bots) + ",\"playersMax\":" + std::to_string(i.max_players) +
           ",\"isPasswordProtected\":" + (i.password ? "true" : "false") + ",\"isVacSecured\":" +
           (i.vac ? "true" : "false") + ",\"unixTimeLastPlayed\":0}";
}

std::string players_json(const std::vector<Player>& players) {
    std::string out = "[";
    for (size_t k = 0; k < players.size(); ++k) {
        if (k) out += ",";
        out += "{\"name\":" + json_string(players[k].name) + ",\"score\":" + std::to_string(players[k].score) +
               ",\"timePlayedSeconds\":" + std::to_string(players[k].seconds) + "}";
    }
    return out + "]";
}

void query_info(const std::string& address, std::function<void(std::optional<std::string>)> done) {
    std::thread([address, done = std::move(done)] {
        Reply r = query(address, info_request, std::nullopt);
        done(r.info ? std::optional<std::string>(info_json(*r.info, address)) : std::nullopt);
    }).detach();
}

void query_players(const std::string& address, std::function<void(std::optional<std::string>)> done) {
    std::thread([address, done = std::move(done)] {
        Reply r = query(address, [](std::optional<uint32_t> c) { return players_request(c.value_or(0xFFFFFFFF)); },
                        std::nullopt);
        done(r.players ? std::optional<std::string>(players_json(*r.players)) : std::nullopt);
    }).detach();
}

}  // namespace nc::a2s
