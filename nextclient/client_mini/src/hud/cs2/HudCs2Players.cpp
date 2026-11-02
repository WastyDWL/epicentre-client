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
    float cx = w * 0.5f;
    float plate_w = 560 * s, plate_h = 58 * s, py = h - 30 * s - plate_h;
    float av = 76 * s, ay = py + plate_h - av + 6 * s;

    // the plate: dark in the middle, fading towards the ends, a hairline of the team colour on top
    DrawGradient(cx - plate_w * 0.5f, py, cx, py + plate_h, {0, 0, 0, 0}, {10, 10, 12, 210}, {10, 10, 12, 220}, {0, 0, 0, 0});
    DrawGradient(cx, py, cx + plate_w * 0.5f, py + plate_h, {10, 10, 12, 210}, {0, 0, 0, 0}, {0, 0, 0, 0}, {10, 10, 12, 220});
    DrawGradient(cx - plate_w * 0.5f, py, cx, py + 2 * s, Alpha(tint, 0), Alpha(tint, 220), Alpha(tint, 220), Alpha(tint, 0));
    DrawGradient(cx, py, cx + plate_w * 0.5f, py + 2 * s, Alpha(tint, 220), Alpha(tint, 0), Alpha(tint, 0), Alpha(tint, 220));

    // the avatar in the middle, framed in the team colour
    float ax = cx - av * 0.5f;
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

    // left of the avatar: the name and the score line; right of it: health
    float inner = av * 0.5f + 18 * s;
    int npx = Px(22 * s);
    std::string name = label_.Fit(p->name.c_str(), npx, true, plate_w * 0.5f - inner - 10 * s);
    label_.Draw(name.c_str(), {cx - inner, py + 7 * s}, npx, true, tint, Cs2Text::Align::Right);
    char buf[48];
    std::snprintf(buf, sizeof(buf), "У %d  ·  С %d", p->frags, p->deaths);
    label_.Draw(buf, {cx - inner, py + 9 * s + npx}, Px(15 * s), false, kInkDim, Cs2Text::Align::Right);

    if (p->bomb)
    {
        float bx = cx + inner, by = py + 34 * s;
        draw_.DrawRect({bx, by}, {bx + 30 * s, by + 18 * s}, {230, 118, 48, 255});
        label_.Draw("C4", {bx + 15 * s, by + 2 * s}, Px(13 * s), true, {24, 14, 6, 255}, Cs2Text::Align::Center);
    }
    if (p->health > 0 && !p->dead)
    {
        int hpx = Px(36 * s);
        float hx = cx + inner + 24 * s, hy = py + (plate_h - hpx) * 0.5f - 6 * s;
        float arm = 9 * s, th = 3 * s, ix = cx + inner + 6 * s, iy = hy + hpx * 0.55f;
        ncl_math::Color ink = p->health <= 25 ? kLow : kInk;
        draw_.DrawRect({ix - arm, iy - th}, {ix + arm, iy + th}, ink);
        draw_.DrawRect({ix - th, iy - arm}, {ix + th, iy + arm}, ink);
        std::snprintf(buf, sizeof(buf), "%d", p->health);
        text_.Draw(buf, {hx, hy}, hpx, true, ink);
    }
    else if (p->dead)
    {
        label_.Draw("мёртв", {cx + inner, py + (plate_h - Px(18 * s)) * 0.5f}, Px(18 * s), true, kLow);
    }
}
