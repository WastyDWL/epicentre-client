#pragma once

#include <cstdint>
#include <unordered_map>

// Steam avatars (64x64) of the players on the scoreboard, as GL textures. Steam has an avatar of a
// stranger only after RequestUserInformation, so a texture appears a moment after the board opens;
// bots (SteamID 0) and players without Steam stay on the drawn initial.
class Cs2Avatars
{
public:
    // 0 while there is no texture (yet); `now` is the client time, which paces the Steam queries
    unsigned int Texture(uint64_t steam_id, float now);

    // Textures belong to a GL context: forget them when it changes.
    void Reset() { entries_.clear(); }

private:
    struct Entry
    {
        unsigned int texture = 0;
        bool requested = false;
        float next_try = 0.0f;
    };
    std::unordered_map<uint64_t, Entry> entries_;
};
