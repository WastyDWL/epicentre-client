// Valve server queries (A2S_INFO, A2S_PLAYER over UDP), for nextclient.matchmaking.getServerInfo /
// getPlayersInfo without Steam. Handles challenges and both Source ('I') and GoldSrc ('m') replies.
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nc::a2s {

struct ServerInfo {
    std::string name, map, game_dir;
    int players = 0, max_players = 0, bots = 0;
    bool password = false, vac = false;
};

struct Player {
    std::string name;
    int score = 0;
    int seconds = 0;
};

struct Reply {
    std::optional<uint32_t> challenge;  // 'A': resend the request with this challenge
    std::optional<ServerInfo> info;
    std::optional<std::vector<Player>> players;
};

struct Address {
    std::string host;
    uint16_t port = 27015;
};

std::vector<unsigned char> info_request(std::optional<uint32_t> challenge);
std::vector<unsigned char> players_request(uint32_t challenge);  // 0xFFFFFFFF asks for a challenge
Reply parse(const std::vector<unsigned char>& packet);
std::optional<Address> parse_address(std::string_view text);

// Same shapes as NextClient's matchmaking API (GameServerItemToV8Object, player details).
std::string info_json(const ServerInfo& info, const std::string& address);
std::string players_json(const std::vector<Player>& players);

// Runs a query on a worker thread; `done` gets the JSON result or nullopt (timeout/error),
// on that worker thread.
void query_info(const std::string& address, std::function<void(std::optional<std::string>)> done);
void query_players(const std::string& address, std::function<void(std::optional<std::string>)> done);

}  // namespace nc::a2s
