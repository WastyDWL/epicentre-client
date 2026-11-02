#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include <nitroapi/NitroApiInterface.h>

// What the CS2-style HUD shows that client.dll keeps in fields NitroApi does not model
// (armor, money, ammo counts, team scores): read from the same user messages its handlers get.
struct Cs2HudState
{
    int armor = 0;
    bool helmet = false;
    int money = 0;
    bool has_money = false;
    int weapon = 0;  // CurWeapon id of the active weapon, 0 none
    int clip = -1;   // -1: the active weapon has no clip
    std::array<int, 32> ammo{};               // AmmoX, by ammo type
    std::array<int8_t, 32> weapon_ammo_type{}; // WeaponList: primary ammo type per weapon id, -1 none
    int score_t = 0;
    int score_ct = 0;
    float round_end = 0.0f;  // client time the round clock reaches zero (RoundTime)
    bool bomb_planted = false;

    // scoreboard: money per player index (Account, sent for teammates) and whether Tab is held
    std::array<int, 33> account{};
    std::array<bool, 33> has_account{};
    bool scoreboard_held = false;
    char server_name[64]{};     // ServerName, sent on connect
    bool scoreboard_own = false; // the CS2 HUD is on: +showscores opens ours, not client.dll's
    bool taps_active = false; // client.dll's handlers were wrapped (CLDLL_Initialize came after us)

    int Reserve() const
    {
        if (weapon <= 0 || weapon >= 32)
            return -1;
        int type = weapon_ammo_type[weapon];
        return type > 0 && type < 32 ? ammo[type] : -1;
    }
};

Cs2HudState& Cs2State();

// While on, the engine 2D draws of the stock ammo corner are dropped (client.dll's table is wrapped
// in CLDLL_Initialize, so this only touches what client.dll itself draws).
// Swaps client.dll's scoreboard for the CS2 one: hides the stock panel once it is shown and marks
// ours open. Called every HUD frame.
void Cs2HudState_SyncStockScoreboard();

void Cs2HudState_ClipStockAmmo(bool on, int screen_w, int screen_h, int font_h);

// Wraps client.dll's handlers of Battery, ArmorType, Money, WeaponList and TeamScore: the
// engine keeps one handler per message, so the tap registers itself and calls the stock one.
void Cs2HudState_Subscribe(nitroapi::NitroApiInterface* nitro_api, std::vector<std::shared_ptr<nitroapi::Unsubscriber>>& unsub);
