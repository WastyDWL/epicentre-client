// The CS2-style scoreboard of HudCs2: opened by +showscores while the CS2 HUD is on.
#include "HudCs2.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include <glad/glad.h>

#include "Cs2HudState.h"

namespace
{
    constexpr ncl_math::Color kInk{232, 228, 214, 255};
    constexpr ncl_math::Color kInkDim{232, 228, 214, 120};
    constexpr ncl_math::Color kInkDead{232, 228, 214, 80};
    constexpr ncl_math::Color kTeamT{224, 166, 74, 255};
    constexpr ncl_math::Color kTeamCT{120, 160, 220, 255};
    constexpr ncl_math::Color kMoney{168, 206, 140, 255};
    constexpr ncl_math::Color kBomb{230, 118, 48, 255};
    constexpr ncl_math::Color kPanel{12, 14, 19, 242};

    struct Row
    {
        int index;
        std::string name;
        int frags;
        int deaths;
        int ping;
        int money; // -1 unknown
        bool dead;
        bool bomb;
        bool local;
        bool bot;
    };

    ncl_math::Color Alpha(ncl_math::Color c, int a)
    {
        return {c.r, c.g, c.b, std::clamp(a, 0, 255)};
    }

    ncl_math::Color Mix(ncl_math::Color a, ncl_math::Color b, float t)
    {
        auto m = [t](int x, int y) { return static_cast<int>(x + (y - x) * t); };
        return {m(a.r, b.r), m(a.g, b.g), m(a.b, b.b), m(a.a, b.a)};
    }

    int Px(float v)
    {
        return std::max(12, static_cast<int>(v + 0.5f));
    }

    // first character of a UTF-8 name, upper-cased for ASCII
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

// a horizontal gradient: one fixed-function quad with per-vertex colour (strips of rects seam)
void HudCs2::DrawFade(float x0, float y0, float x1, float y1, ncl_math::Color from, ncl_math::Color to)
{
    draw_.FlushPrims();
    // the engine draws its 2D with flat shading, which would paint the quad in one colour
    GLint shade = GL_SMOOTH;
    glGetIntegerv(GL_SHADE_MODEL, &shade);
    glShadeModel(GL_SMOOTH);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBegin(GL_QUADS);
    glColor4ub(from.r, from.g, from.b, from.a);
    glVertex2f(x0, y0);
    glColor4ub(to.r, to.g, to.b, to.a);
    glVertex2f(x1, y0);
    glVertex2f(x1, y1);
    glColor4ub(from.r, from.g, from.b, from.a);
    glVertex2f(x0, y1);
    glEnd();
    glColor4ub(255, 255, 255, 255);
    glEnable(GL_TEXTURE_2D);
    glShadeModel(static_cast<GLenum>(shade));
}

void HudCs2::DrawScoreboard(float w, float h, float s)
{
    const Cs2HudState& st = Cs2State();
    GetAllPlayersInfo();

    std::vector<Row> teams[2]; // 0 CT, 1 T
    std::string spectators;
    int local_team = LocalTeam();

    for (int i = 1; i <= goldsrc::cstrike::MAX_PLAYERS; i++)
    {
        goldsrc::hud_player_info_t info{};
        cl_enginefunc()->pfnGetPlayerInfo(i, &info);
        if (!info.name || !info.name[0])
            continue;

        const goldsrc::cstrike::extra_player_info_t& extra = cl()->g_PlayerExtraInfo[i];
        int tn = extra.teamnumber;
        if (info.spectator || (tn != goldsrc::cstrike::TEAM_CT && tn != goldsrc::cstrike::TEAM_TERRORIST))
        {
            if (!spectators.empty())
                spectators += ", ";
            spectators += info.name;
            continue;
        }

        bool local = info.thisplayer != 0;
        Row row{i, info.name, extra.frags, extra.deaths, info.ping, -1, extra.dead, extra.has_c4 != 0, local,
                !local && info.m_nSteamID == 0 && info.ping <= 0};
        if (local && st.has_money)
            row.money = st.money;
        else if (st.has_account[i] && tn == local_team)
            row.money = st.account[i];
        teams[tn == goldsrc::cstrike::TEAM_CT ? 0 : 1].push_back(row);
    }

    for (auto& team : teams)
    {
        std::sort(team.begin(), team.end(), [](const Row& a, const Row& b) {
            return a.frags != b.frags ? a.frags > b.frags : a.deaths < b.deaths;
        });
    }

    // layout: the rows shrink for big teams, so the board stays on screen
    float width = std::min(900 * s, w - 40 * s);
    float x0 = (w - width) * 0.5f, x1 = x0 + width;
    float header_h = 46 * s, team_h = 72 * s, columns_h = 28 * s, gap = 14 * s, spec_h = 36 * s;
    int rows = static_cast<int>(std::max<size_t>(teams[0].size(), 1) + std::max<size_t>(teams[1].size(), 1));
    float fixed = header_h + 2 * (team_h + columns_h) + 2 * gap + (spectators.empty() ? 0 : spec_h);
    float row_h = std::clamp((h * 0.84f - fixed) / rows, 26 * s, 40 * s);
    float total = fixed + rows * row_h;
    float y = std::max(16 * s, (h - total) * 0.42f);

    draw_.BeginPrims();

    // server and map
    draw_.DrawRect({x0, y}, {x1, y + header_h}, kPanel);
    std::string map = cl_enginefunc()->pfnGetLevelName ? cl_enginefunc()->pfnGetLevelName() : "";
    if (map.rfind("maps/", 0) == 0)
        map = map.substr(5);
    if (map.size() > 4 && map.compare(map.size() - 4, 4, ".bsp") == 0)
        map.resize(map.size() - 4);
    int head_px = Px(22 * s), map_px = Px(19 * s);
    float map_w = text_.Measure(map.c_str(), map_px, true);
    std::string host = label_.Fit(st.server_name[0] ? st.server_name : "Counter-Strike", head_px, true, width - map_w - 64 * s);
    label_.Draw(host.c_str(), {x0 + 20 * s, y + (header_h - head_px) * 0.5f - 2 * s}, head_px, true, kInk);
    text_.Draw(map.c_str(), {x1 - 20 * s, y + (header_h - map_px) * 0.5f - 1 * s}, map_px, true, kInkDim, Cs2Text::Align::Right);
    y += header_h + gap;

    // columns, right to left
    float col_ping = x1 - 22 * s, col_deaths = col_ping - 80 * s, col_kills = col_deaths - 80 * s, col_money = col_kills - 110 * s;
    float avatar_x = x0 + 18 * s, name_x = avatar_x + 44 * s, name_max = col_money - 110 * s - name_x;

    for (int t = 0; t < 2; t++)
    {
        const std::vector<Row>& team = teams[t];
        bool ct = t == 0;
        ncl_math::Color tint = ct ? kTeamCT : kTeamT;
        int score = ct ? st.score_ct : st.score_t;
        int alive = static_cast<int>(std::count_if(team.begin(), team.end(), [](const Row& r) { return !r.dead; }));

        // team header: score plate in the team colour, logo, name, alive count
        draw_.DrawRect({x0, y}, {x1, y + team_h}, kPanel);
        DrawFade(x0, y, x0 + width * 0.7f, y + team_h, Alpha(tint, 70), Alpha(tint, 0));

        float plate = team_h;
        draw_.DrawRect({x0, y}, {x0 + plate * 1.25f, y + team_h}, Alpha(Mix(tint, {0, 0, 0, 255}, 0.35f), 255));
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%d", score);
        int score_px = Px(48 * s);
        text_.Draw(buf, {x0 + plate * 0.625f, y + (team_h - score_px) * 0.5f - 2 * s}, score_px, true, {255, 255, 255, 255}, Cs2Text::Align::Center);

        float logo = 50 * s, lx = x0 + plate * 1.25f + 16 * s;
        if (!Icon(ct ? "logo_ct" : "logo_tr", lx, y + (team_h - logo) * 0.5f, logo, logo, {255, 255, 255, 255}))
            draw_.DrawDisc({lx + logo * 0.5f, y + team_h * 0.5f}, logo * 0.4f, tint);

        int tpx = Px(26 * s);
        float tx = lx + logo + 14 * s;
        label_.Draw(ct ? "СПЕЦНАЗ" : "ТЕРРОРИСТЫ", {tx, y + team_h * 0.5f - tpx + 2 * s}, tpx, true, tint);
        std::snprintf(buf, sizeof(buf), "Живы: %d / %d", alive, static_cast<int>(team.size()));
        label_.Draw(buf, {tx, y + team_h * 0.5f + 4 * s}, Px(16 * s), false, kInkDim);
        y += team_h;

        // column titles
        draw_.DrawRect({x0, y}, {x1, y + columns_h}, {8, 9, 13, 245});
        draw_.DrawRect({x0, y + columns_h - 1}, {x1, y + columns_h}, Alpha(tint, 90));
        int cpx = Px(14 * s);
        float cy = y + (columns_h - cpx) * 0.5f - 1 * s;
        label_.Draw("ИГРОК", {name_x, cy}, cpx, true, kInkDim);
        label_.Draw("ДЕНЬГИ", {col_money, cy}, cpx, true, kInkDim, Cs2Text::Align::Right);
        label_.Draw("УБ", {col_kills, cy}, cpx, true, kInkDim, Cs2Text::Align::Right);
        label_.Draw("СМ", {col_deaths, cy}, cpx, true, kInkDim, Cs2Text::Align::Right);
        label_.Draw("ПИНГ", {col_ping, cy}, cpx, true, kInkDim, Cs2Text::Align::Right);
        y += columns_h;

        int npx = Px(std::min(20 * s, row_h * 0.52f));
        int dpx = Px(std::min(24 * s, row_h * 0.62f));
        float av = std::min(30 * s, row_h - 6 * s);
        for (size_t i = 0; i < team.size(); i++)
        {
            const Row& r = team[i];
            draw_.DrawRect({x0, y}, {x1, y + row_h}, i % 2 ? ncl_math::Color{15, 17, 23, 238} : ncl_math::Color{20, 23, 30, 238});
            if (r.local)
                DrawFade(x0, y, x1, y + row_h, Alpha(tint, 70), Alpha(tint, 12));
            draw_.DrawRect({x0, y + row_h - 1}, {x1, y + row_h}, {255, 255, 255, 10});

            ncl_math::Color ink = r.dead ? kInkDead : kInk;

            // avatar: team-coloured square with the name's initial, greyed when dead
            float ay = y + (row_h - av) * 0.5f;
            ncl_math::Color face = r.dead ? ncl_math::Color{60, 62, 68, 255} : Mix(tint, {0, 0, 0, 255}, 0.25f);
            draw_.DrawRect({avatar_x, ay}, {avatar_x + av, ay + av}, face);
            int ipx = Px(av * 0.6f);
            label_.Draw(Initial(r.name).c_str(), {avatar_x + av * 0.5f, ay + (av - ipx) * 0.5f - 1 * s}, ipx, true,
                        r.dead ? kInkDead : ncl_math::Color{255, 255, 255, 235}, Cs2Text::Align::Center);

            float ty = y + (row_h - npx) * 0.5f - 1 * s;
            std::string name = label_.Fit(r.name.c_str(), npx, r.local, name_max - (r.bomb ? 44 * s : 0));
            float nw = label_.Draw(name.c_str(), {name_x, ty}, npx, r.local, ink);
            if (r.bomb)
            {
                float bx = name_x + nw + 12 * s, bh = std::min(20 * s, row_h * 0.55f);
                draw_.DrawRect({bx, y + (row_h - bh) * 0.5f}, {bx + 30 * s, y + (row_h + bh) * 0.5f}, kBomb);
                int bpx = Px(bh * 0.7f);
                label_.Draw("C4", {bx + 15 * s, y + (row_h - bpx) * 0.5f - 1 * s}, bpx, true, {24, 14, 6, 255}, Cs2Text::Align::Center);
            }

            float dy = y + (row_h - dpx) * 0.5f;
            char num[24];
            if (r.money >= 0)
            {
                std::snprintf(num, sizeof(num), "$%d", r.money);
                text_.Draw(num, {col_money, dy}, dpx, true, r.dead ? Alpha(kMoney, 100) : kMoney, Cs2Text::Align::Right);
            }
            std::snprintf(num, sizeof(num), "%d", r.frags);
            text_.Draw(num, {col_kills, dy}, dpx, true, ink, Cs2Text::Align::Right);
            std::snprintf(num, sizeof(num), "%d", r.deaths);
            text_.Draw(num, {col_deaths, dy}, dpx, true, ink, Cs2Text::Align::Right);
            if (r.bot)
            {
                int bpx = Px(dpx * 0.72f);
                label_.Draw("БОТ", {col_ping, y + (row_h - bpx) * 0.5f - 1 * s}, bpx, true, kInkDim, Cs2Text::Align::Right);
            }
            else
            {
                std::snprintf(num, sizeof(num), "%d", std::max(r.ping, 0));
                text_.Draw(num, {col_ping, dy}, dpx, true, ink, Cs2Text::Align::Right);
            }
            y += row_h;
        }
        if (team.empty())
        {
            draw_.DrawRect({x0, y}, {x1, y + row_h}, {15, 17, 23, 238});
            int px = Px(16 * s);
            label_.Draw("Нет игроков", {name_x, y + (row_h - px) * 0.5f - 1 * s}, px, false, kInkDead);
            y += row_h;
        }
        y += gap;
    }

    if (!spectators.empty())
    {
        int px = Px(16 * s);
        draw_.DrawRect({x0, y}, {x1, y + spec_h}, kPanel);
        std::string line = label_.Fit(("Наблюдатели: " + spectators).c_str(), px, false, width - 40 * s);
        label_.Draw(line.c_str(), {x0 + 20 * s, y + (spec_h - px) * 0.5f - 1 * s}, px, false, kInkDim);
    }

    draw_.EndPrims();
    draw_.EndFrame();
}
