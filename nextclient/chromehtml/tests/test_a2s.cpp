#include <cstring>
#include <string>
#include <vector>

#include "core/a2s.h"
#include "testing.h"

using namespace nc::a2s;

namespace {

using Bytes = std::vector<unsigned char>;

void put(Bytes& b, std::initializer_list<int> v) {
    for (int x : v) b.push_back(static_cast<unsigned char>(x));
}
void put_str(Bytes& b, const std::string& s) {
    b.insert(b.end(), s.begin(), s.end());
    b.push_back(0);
}
void put_le32(Bytes& b, uint32_t v) { put(b, {int(v & 0xFF), int(v >> 8 & 0xFF), int(v >> 16 & 0xFF), int(v >> 24)}); }
void put_float(Bytes& b, float f) {
    uint32_t v;
    std::memcpy(&v, &f, 4);
    put_le32(b, v);
}

Bytes header(char type) {
    Bytes b;
    put(b, {0xFF, 0xFF, 0xFF, 0xFF, type});
    return b;
}

}  // namespace

TEST(a2s_info_request_without_and_with_challenge) {
    Bytes plain = info_request(std::nullopt);
    std::string s(plain.begin() + 5, plain.end());
    CHECK_EQ(plain[4], (unsigned char)'T');
    CHECK_EQ(s, std::string("Source Engine Query", 20));  // includes the terminating NUL
    Bytes with = info_request(0x11223344u);
    CHECK_EQ(with.size(), plain.size() + 4);
    CHECK_EQ(with[with.size() - 4], (unsigned char)0x44);
    CHECK_EQ(with.back(), (unsigned char)0x11);
}

TEST(a2s_challenge_reply_parsed) {
    Bytes b = header('A');
    put_le32(b, 0xDEADBEEF);
    auto r = parse(b);
    CHECK(r.challenge.has_value());
    CHECK_EQ(*r.challenge, 0xDEADBEEFu);
}

TEST(a2s_source_style_info) {
    Bytes b = header('I');
    put(b, {48});  // protocol
    put_str(b, "Эпицентр Арена Страйка 18+");
    put_str(b, "de_dust2");
    put_str(b, "cstrike");
    put_str(b, "Counter-Strike");
    put(b, {10, 0});      // app id 10
    put(b, {14, 32, 2});  // players, max, bots
    put(b, {'d', 'w', 1, 1});  // dedicated, windows, password, vac
    auto r = parse(b);
    CHECK(r.info.has_value());
    CHECK_EQ(r.info->name, std::string("Эпицентр Арена Страйка 18+"));
    CHECK_EQ(r.info->map, std::string("de_dust2"));
    CHECK_EQ(r.info->game_dir, std::string("cstrike"));
    CHECK_EQ(r.info->players, 14);
    CHECK_EQ(r.info->max_players, 32);
    CHECK_EQ(r.info->bots, 2);
    CHECK(r.info->password);
    CHECK(r.info->vac);
}

TEST(a2s_goldsrc_style_info) {
    Bytes b = header('m');
    put_str(b, "45.95.31.157:27115");
    put_str(b, "Old Server");
    put_str(b, "cs_mansion");
    put_str(b, "cstrike");
    put_str(b, "Counter-Strike");
    put(b, {5, 20, 47});  // players, max, protocol
    put(b, {'d', 'w', 0, 0});  // dedicated, os, password, mod=0
    put(b, {1, 0});            // vac, bots
    auto r = parse(b);
    CHECK(r.info.has_value());
    CHECK_EQ(r.info->name, std::string("Old Server"));
    CHECK_EQ(r.info->map, std::string("cs_mansion"));
    CHECK_EQ(r.info->players, 5);
    CHECK_EQ(r.info->max_players, 20);
    CHECK(r.info->vac);
}

TEST(a2s_players_parsed) {
    Bytes b = header('D');
    put(b, {2});
    put(b, {0});
    put_str(b, "Жека К");
    put_le32(b, 40);
    put_float(b, 125.5f);
    put(b, {1});
    put_str(b, "JIyHb");
    put_le32(b, static_cast<uint32_t>(-3));
    put_float(b, 61.0f);
    auto r = parse(b);
    CHECK(r.players.has_value());
    CHECK_EQ(r.players->size(), (size_t)2);
    CHECK_EQ((*r.players)[0].name, std::string("Жека К"));
    CHECK_EQ((*r.players)[0].score, 40);
    CHECK_EQ((*r.players)[0].seconds, 125);
    CHECK_EQ((*r.players)[1].score, -3);
}

TEST(a2s_truncated_or_garbage_rejected) {
    Bytes b = header('I');
    put(b, {48});
    put_str(b, "name");  // the rest is missing
    auto r = parse(b);
    CHECK(!r.info.has_value());
    CHECK(!parse(Bytes{1, 2, 3}).info.has_value());
    Bytes p = header('D');
    put(p, {5, 0});  // claims 5 players, has none
    CHECK(!parse(p).players.has_value());
}

TEST(a2s_info_json_matches_nextclient_shape) {
    ServerInfo i;
    i.name = "Эпицентр \"18+\"";
    i.map = "de_dust2";
    i.game_dir = "cstrike";
    i.players = 14;
    i.bots = 2;
    i.max_players = 32;
    i.password = false;
    i.vac = true;
    CHECK_EQ(info_json(i, "45.95.31.157:27115"),
             std::string("{\"appId\":10,\"gameDir\":\"cstrike\",\"address\":\"45.95.31.157:27115\","
                         "\"hostname\":\"Эпицентр \\\"18+\\\"\",\"map\":\"de_dust2\",\"playersOnline\":12,"
                         "\"botsOnline\":2,\"playersMax\":32,\"isPasswordProtected\":false,"
                         "\"isVacSecured\":true,\"unixTimeLastPlayed\":0}"));
}

TEST(a2s_players_json) {
    std::vector<Player> p = {{"Жека", 40, 125}};
    CHECK_EQ(players_json(p), std::string("[{\"name\":\"Жека\",\"score\":40,\"timePlayedSeconds\":125}]"));
}

TEST(a2s_address_parsing) {
    auto a = parse_address("45.95.31.157:27115");
    CHECK(a.has_value());
    CHECK_EQ(a->port, (uint16_t)27115);
    CHECK_EQ(a->host, std::string("45.95.31.157"));
    CHECK_EQ(parse_address("1.2.3.4")->port, (uint16_t)27015);
    CHECK(!parse_address("").has_value());
    CHECK(!parse_address("1.2.3.4:99999").has_value());
}
