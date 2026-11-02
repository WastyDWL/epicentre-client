// HudCs2's player views: the top bar's team avatars (CS2) and the panel of the player being spectated.
#include "HudCs2.h"

#include <algorithm>
#include <cstdio>

#include "Cs2HudState.h"
#include "main.h"

namespace
{
    constexpr ncl_math::Color kInk{236, 229, 205, 255};
    constexpr ncl_math::Color kInkDim{236, 229, 205, 140};
    constexpr ncl_math::Color kTeamT{222, 155, 53, 255};
    constexpr ncl_math::Color kTeamCT{110, 150, 214, 255};
    constexpr ncl_math::Color kLow{235, 72, 60, 255};

    // observer modes (pm_shared.h) in which one player is followed
    constexpr int kObsChaseLocked = 1;
    constexpr int kObsChaseFree = 2;
    constexpr int kObsInEye = 4;

    ncl_math::Color Alpha(ncl_math::Color c, int a)
    {
        return {c.r, c.g, c.b, std::clamp(a, 0, 255)};
    }

    int Px(float v)
    {
        return std::max(10, static_cast<int>(v + 0.5f));
    }

    std::string Initial(const std::string& name)
    {
        if (name.empty())
            return "?";
        unsigned char c = name[0];
        size_t len = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : 4;
        std::string out = name.substr(0, std::min(len, name.size()));
        if (len == 1 && c >= 'a' && c <= 'z')
            out[0] = static_cast<char>(c - 32);
        return out;
    }
} // namespace

std::vector<HudCs2::PlayerView> HudCs2::CollectPlayers()
{
    GetAllPlayersInfo();
    std::vector<PlayerView> out;
    for (int i = 1; i <= goldsrc::cstrike::MAX_PLAYERS; i++)
    {
        goldsrc::hud_player_info_t info{};
        cl_enginefunc()->pfnGetPlayerInfo(i, &info);
        if (!info.name || !info.name[0] || info.spectator)
            continue;
        const goldsrc::cstrike::extra_player_info_t& extra = cl()->g_PlayerExtraInfo[i];
        if (extra.teamnumber != goldsrc::cstrike::TEAM_TERRORIST && extra.teamnumber != goldsrc::cstrike::TEAM_CT)
            continue;
        int health = extra.sb_health > 0 ? extra.sb_health : extra.health;
        out.push_back({i, info.m_nSteamID, info.name, extra.teamnumber, extra.dead, extra.frags, extra.deaths, health, extra.has_c4 != 0});
    }
    return out;
}

unsigned int HudCs2::AvatarOf(uint64_t steam_id)
{
    return avatars_.Texture(steam_id, static_cast<float>(cl_enginefunc()->GetClientTime()));
}

bool HudCs2::DrawTopAvatars(const std::vector<PlayerView>& players, float left_edge, float right_edge, float top, float height, float s)
{
    if (players.empty())
        return false;

    float size = height - 10 * s, gap = 3 * s, y = top + 2 * s;
    int ipx = Px(size * 0.55f);
    float left_x = left_edge, right_x = right_edge;

    for (const PlayerView& p : players)
    {
        bool t = p.team == goldsrc::cstrike::TEAM_TERRORIST;
        ncl_math::Color tint = t ? kTeamT : kTeamCT;
        float x = t ? (left_x -= size + gap) : right_x;
        if (!t)
            right_x += size + gap;

        // living players bright with the team bar under them; the dead greyed, without the bar
        draw_.DrawRect({x - 1 * s, y - 1 * s}, {x + size + 1 * s, y + size + 1 * s}, {0, 0, 0, 170});
        if (unsigned int tex = AvatarOf(p.steam_id))
        {
            draw_.FlushPrims();
            Cs2Art::DrawTexture(tex, x, y, size, size, p.dead ? ncl_math::Color{90, 90, 90, 150} : ncl_math::Color{255, 255, 255, 255});
        }
        else
        {
            draw_.DrawRect({x, y}, {x + size, y + size}, p.dead ? ncl_math::Color{50, 50, 54, 200} : Alpha(tint, 150));
            label_.Draw(Initial(p.name).c_str(), {x + size * 0.5f, y + (size - ipx) * 0.5f - 1 * s}, ipx, true,
                        p.dead ? Alpha(kInk, 80) : kInk, Cs2Text::Align::Center);
        }
        if (!p.dead)
            draw_.DrawRect({x, y + size + 2 * s}, {x + size, y + size + 5 * s}, tint);
    }
    return true;
}

void HudCs2::DrawSpectatorPanel(float w, float h, float s)
{
    if (g_iUser1 != kObsChaseLocked && g_iUser1 != kObsChaseFree && g_iUser1 != kObsInEye)
        return;
    int target = g_iUser2;
    if (!IsValidClientIndex(target))
        return;

    const PlayerView* p = nullptr;
    std::vector<PlayerView> players = CollectPlayers();
    for (const PlayerView& v : players)
    {
        if (v.index == target)
            p = &v;
    }
    if (!p)
        return;


    bool t = p->team == goldsrc::cstrike::TEAM_TERRORIST;
    ncl_math::Color tint = t ? kTeamT : kTeamCT;
    bool in_eye = g_iUser1 == kObsInEye;
    float cx = w * 0.5f;

    // In-eye, CS 1.6 tells the spectator the followed player's weapon (CurWeapon) but not its health
    // or ammo, so only the weapon is shown; health comes from the scoreboard data when known.

    // the plate between them (CS2: 0.385..0.615 of the width), the team colour fading in from the left
    float x0 = w * 0.385f, x1 = w * 0.615f, y1 = h - 12 * s, y0 = y1 - 50 * s;
    DrawGradient(x0, y0, x1, y1, Alpha(tint, 95), {14, 14, 18, 215}, {10, 10, 12, 230}, Alpha(tint, 60));
    draw_.DrawRect({x0, y0}, {x1, y0 + std::max(1.0f, 1.2f * s)}, Alpha(tint, 200));

    // the avatar in the middle, standing out above the plate, framed in the team colour
    float av = 62 * s, ax = cx - av * 0.5f, ay = y1 - av;
    draw_.DrawRect({ax - 3 * s, ay - 3 * s}, {ax + av + 3 * s, ay + av + 3 * s}, tint);
    if (unsigned int tex = AvatarOf(p->steam_id))
    {
        draw_.FlushPrims();
        Cs2Art::DrawTexture(tex, ax, ay, av, av, {255, 255, 255, 255});
    }
    else
    {
        draw_.DrawRect({ax, ay}, {ax + av, ay + av}, {28, 30, 34, 255});
        int ipx = Px(av * 0.55f);
        label_.Draw(Initial(p->name).c_str(), {cx, ay + (av - ipx) * 0.5f - 2 * s}, ipx, true, tint, Cs2Text::Align::Center);
    }

    // left half: the name and the score line
    float pad = 12 * s, half = (x1 - x0) * 0.5f - av * 0.5f - pad * 2;
    int npx = Px(19 * s);
    std::string name = label_.Fit(p->name.c_str(), npx, true, half);
    label_.Draw(name.c_str(), {x0 + pad, y0 + 6 * s}, npx, true, tint);
    char buf[48];
    std::snprintf(buf, sizeof(buf), "У %d  ·  С %d", p->frags, p->deaths);
    label_.Draw(buf, {x0 + pad, y0 + 9 * s + npx}, Px(13 * s), false, kInkDim);

    // right half: the weapon (known in-eye) or the health, and C4
    static const char* const kWeapons[] = {
        "", "P228", "Glock", "Scout", "HE-граната", "XM1014", "C4", "MAC-10", "AUG", "Дымовая", "Dual Elites",
        "Five-SeveN", "UMP-45", "SG 550", "Galil", "FAMAS", "USP", "Glock-18", "AWP", "MP5", "M249", "M3", "M4A1",
        "TMP", "G3SG1", "Флешка", "Desert Eagle", "SG 552", "AK-47", "Нож", "P90"};
    const Cs2HudState& st = Cs2State();
    const char* right = nullptr;
    if (p->dead)
        right = "мёртв";
    else if (in_eye && st.weapon > 0 && st.weapon < static_cast<int>(std::size(kWeapons)))
        right = kWeapons[st.weapon];
    float line2 = y0 + 6 * s;
    if (right)
    {
        label_.Draw(right, {x1 - pad, line2}, Px(17 * s), true, p->dead ? kLow : kInk, Cs2Text::Align::Right);
        line2 += 22 * s;
    }
    float bomb_right = x1 - pad;
    if (!p->dead && p->health > 0)
    {
        std::snprintf(buf, sizeof(buf), "%d HP", p->health);
        float hw = text_.Draw(buf, {x1 - pad, line2}, Px(16 * s), true, p->health <= 25 ? kLow : kInkDim, Cs2Text::Align::Right);
        bomb_right -= hw + 8 * s;
    }
    if (p->bomb)
    {
        float bx = bomb_right - 30 * s, by = y0 + 28 * s;
        draw_.DrawRect({bx, by}, {bx + 30 * s, by + 16 * s}, {230, 118, 48, 255});
        label_.Draw("C4", {bx + 15 * s, by + 1 * s}, Px(12 * s), true, {24, 14, 6, 255}, Cs2Text::Align::Center);
    }
}