// The scoreboard of HudCs2, after CS:GO's: one panel with a header (map, server, round clock),
// a round badge per team on the left, the player table and the round history between the teams.
#include "HudCs2.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include <glad/glad.h>

#include "Cs2HudState.h"

namespace
{
    constexpr ncl_math::Color kInk{226, 222, 210, 255};
    constexpr ncl_math::Color kInkDim{226, 222, 210, 125};
    constexpr ncl_math::Color kInkDead{226, 222, 210, 70};
    constexpr ncl_math::Color kTeamT{222, 184, 110, 255};
    constexpr ncl_math::Color kTeamCT{150, 186, 228, 255};
    constexpr ncl_math::Color kLocal{110, 190, 80, 255};
    constexpr ncl_math::Color kBomb{230, 118, 48, 255};

    struct Row
    {
        uint64_t steam_id;
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

void HudCs2::DrawGradient(float x0, float y0, float x1, float y1, ncl_math::Color tl, ncl_math::Color tr, ncl_math::Color br, ncl_math::Color bl)
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
    glColor4ub(tl.r, tl.g, tl.b, tl.a);
    glVertex2f(x0, y0);
    glColor4ub(tr.r, tr.g, tr.b, tr.a);
    glVertex2f(x1, y0);
    glColor4ub(br.r, br.g, br.b, br.a);
    glVertex2f(x1, y1);
    glColor4ub(bl.r, bl.g, bl.b, bl.a);
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
    int players = 0;

    for (int i = 1; i <= goldsrc::cstrike::MAX_PLAYERS; i++)
    {
        goldsrc::hud_player_info_t info{};
        cl_enginefunc()->pfnGetPlayerInfo(i, &info);
        if (!info.name || !info.name[0])
            continue;
        players++;

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
        Row row{info.m_nSteamID, info.name, extra.frags, extra.deaths, info.ping, -1, extra.dead, extra.has_c4 != 0, local,
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

    // ---- layout ----
    float width = std::min(1040 * s, w - 40 * s);
    float x0 = (w - width) * 0.5f, x1 = x0 + width;
    float pad = 26 * s;
    float head_h = 96 * s, cols_h = 26 * s, strip_h = 78 * s, foot_h = spectators.empty() ? 22 * s : 44 * s;
    size_t rows_ct = std::max<size_t>(teams[0].size(), 5), rows_t = std::max<size_t>(teams[1].size(), 5);
    float fixed = head_h + cols_h + strip_h + foot_h;
    float row_h = std::clamp((h * 0.86f - fixed) / static_cast<float>(rows_ct + rows_t), 20 * s, 28 * s);
    float total = fixed + (rows_ct + rows_t) * row_h;
    float y0 = std::max(12 * s, (h - total) * 0.46f);
    float y1 = y0 + total;

    float badge_cx = x0 + pad + 46 * s;          // team badge column
    float table_x0 = x0 + pad + 104 * s, table_x1 = x1 - pad;
    float col_deaths = table_x1 - 14 * s, col_kills = col_deaths - 54 * s, col_money = col_kills - 66 * s;
    float ping_x = table_x0 + 30 * s, avatar_x = table_x0 + 48 * s, name_x = avatar_x + row_h + 8 * s;
    float name_max = col_money - 110 * s - name_x;

    // ---- panel: soft shadow, glassy gradient, hairline frame ----
    draw_.BeginPrims();
    for (int i = 1; i <= 4; i++)
    {
        float o = i * 3 * s;
        draw_.DrawRect({x0 - o, y0 - o}, {x1 + o, y1 + o}, {0, 0, 0, static_cast<uint8_t>(34 - 7 * i)});
    }
    DrawGradient(x0, y0, x1, y1, {44, 46, 50, 238}, {36, 38, 42, 238}, {16, 17, 20, 244}, {20, 21, 24, 244});
    float hair = std::max(1.0f, s);
    draw_.DrawRect({x0, y0}, {x1, y0 + hair}, {255, 255, 255, 46});
    draw_.DrawRect({x0, y1 - hair}, {x1, y1}, {255, 255, 255, 18});
    draw_.DrawRect({x0, y0}, {x0 + hair, y1}, {255, 255, 255, 22});
    draw_.DrawRect({x1 - hair, y0}, {x1, y1}, {255, 255, 255, 22});

    // header: emblem, "Карта | map", server; clock and player count on the right
    std::string map = cl_enginefunc()->pfnGetLevelName ? cl_enginefunc()->pfnGetLevelName() : "";
    if (map.rfind("maps/", 0) == 0)
        map = map.substr(5);
    if (map.size() > 4 && map.compare(map.size() - 4, 4, ".bsp") == 0)
        map.resize(map.size() - 4);

    float icon = 54 * s, iy = y0 + 22 * s;
    // the server's own emblem: the team logos already sit beside each team
    if (!Icon("epicentre_logo", x0 + pad - 4 * s, iy - 4 * s, icon + 8 * s, icon + 8 * s, {255, 255, 255, 255}))
        draw_.DrawDisc({x0 + pad + icon * 0.5f, iy + icon * 0.5f}, icon * 0.45f, {255, 122, 26, 255});

    int title_px = Px(30 * s);
    float tx = x0 + pad + icon + 18 * s;
    float title_w = label_.Draw("Карта", {tx, iy - 2 * s}, title_px, true, kInk);
    draw_.DrawRect({tx + title_w + 12 * s, iy + 4 * s}, {tx + title_w + 14 * s, iy + title_px}, Alpha(kInk, 120));
    text_.Draw(map.c_str(), {tx + title_w + 26 * s, iy - 2 * s}, title_px, true, kInk);

    char buf[96];
    std::string host = label_.Fit(st.server_name[0] ? st.server_name : "Counter-Strike", Px(16 * s), false, width * 0.55f);
    std::snprintf(buf, sizeof(buf), "Сервер: %s", host.c_str());
    label_.Draw(buf, {tx, iy + title_px + 8 * s}, Px(16 * s), false, kInkDim);

    float left = st.round_end - static_cast<float>(cl_enginefunc()->GetClientTime());
    if (left > 0.0f && !st.bomb_planted)
    {
        int secs = static_cast<int>(left);
        std::snprintf(buf, sizeof(buf), "%d:%02d", secs / 60, secs % 60);
    }
    else
    {
        std::snprintf(buf, sizeof(buf), st.bomb_planted ? "C4" : "0:00");
    }
    text_.Draw(buf, {table_x1, iy - 4 * s}, Px(34 * s), true, st.bomb_planted ? kBomb : kInk, Cs2Text::Align::Right);
    std::snprintf(buf, sizeof(buf), "Игроков: %d", players);
    label_.Draw(buf, {table_x1, iy + title_px + 8 * s}, Px(16 * s), false, kInkDim, Cs2Text::Align::Right);

    // column titles
    float y = y0 + head_h;
    int cpx = Px(14 * s);
    float cy = y + (cols_h - cpx) * 0.5f;
    draw_.DrawRect({table_x0, y}, {table_x1, y + cols_h}, {255, 255, 255, 8});
    for (int b = 0; b < 4; b++) // signal bars over the ping column
    {
        float bx = table_x0 + 10 * s + b * 4 * s, bh = (b + 1) * 3.2f * s;
        draw_.DrawRect({bx, y + cols_h - 7 * s - bh}, {bx + 2.4f * s, y + cols_h - 7 * s}, kInkDim);
    }
    label_.Draw("Деньги", {col_money, cy}, cpx, false, kInkDim, Cs2Text::Align::Right);
    label_.Draw("У", {col_kills, cy}, cpx, false, kInkDim, Cs2Text::Align::Right);
    label_.Draw("С", {col_deaths, cy}, cpx, false, kInkDim, Cs2Text::Align::Right);
    y += cols_h;

    auto draw_team = [&](int t, float top, size_t slots) {
        const std::vector<Row>& team = teams[t];
        bool ct = t == 0;
        ncl_math::Color tint = ct ? kTeamCT : kTeamT;
        int score = ct ? st.score_ct : st.score_t;
        float block = slots * row_h;

        // team badge: logo, the score in the team colour, the team's name
        float logo = std::min(54 * s, block * 0.38f);
        int spx = Px(std::min(40 * s, block * 0.3f));
        int npx = Px(12 * s);
        float stack = logo + 4 * s + spx + 6 * s + npx;
        float by = top + (block - stack) * 0.5f;
        draw_.DrawDisc({badge_cx, by + logo * 0.5f}, logo * 0.62f, Alpha(tint, 22));
        if (!Icon(ct ? "logo_ct" : "logo_tr", badge_cx - logo * 0.5f, by, logo, logo, {255, 255, 255, 235}))
            draw_.DrawRing({badge_cx, by + logo * 0.5f}, logo * 0.5f, logo * 0.44f, tint);
        std::snprintf(buf, sizeof(buf), "%d", score);
        text_.Draw(buf, {badge_cx, by + logo + 4 * s}, spx, true, tint, Cs2Text::Align::Center);
        label_.Draw(ct ? "СПЕЦНАЗ" : "ТЕРРОРИСТЫ", {badge_cx, by + logo + 10 * s + spx}, npx, true, Alpha(tint, 210), Cs2Text::Align::Center);

        int fpx = Px(std::min(19 * s, row_h * 0.7f));
        int dpx = Px(std::min(20 * s, row_h * 0.74f));
        int ppx = Px(std::min(12 * s, row_h * 0.5f));
        for (size_t i = 0; i < slots; i++)
        {
            float ry = top + i * row_h;
            if (i >= team.size())
            {
                draw_.DrawRect({table_x0, ry}, {table_x1, ry + row_h - 1}, {0, 0, 0, i % 2 ? 40 : 60});
                continue;
            }
            const Row& p = team[i];
            // the row: the team colour fading out to the right, CS2-like
            DrawGradient(table_x0, ry, table_x1, ry + row_h - 1, Alpha(tint, p.dead ? 18 : 52), Alpha(tint, 6),
                         {0, 0, 0, i % 2 ? 50 : 80}, Alpha(tint, p.dead ? 12 : 34));

            if (p.local)
            {
                draw_.DrawRect({table_x0, ry}, {table_x1, ry + row_h - 1}, Alpha(kLocal, 46));
                float lw = std::max(1.0f, 1.5f * s);
                draw_.DrawRect({table_x0, ry}, {table_x1, ry + lw}, kLocal);
                draw_.DrawRect({table_x0, ry + row_h - 1 - lw}, {table_x1, ry + row_h - 1}, kLocal);
                draw_.DrawRect({table_x0, ry}, {table_x0 + lw, ry + row_h - 1}, kLocal);
                draw_.DrawRect({table_x1 - lw, ry}, {table_x1, ry + row_h - 1}, kLocal);
            }
            ncl_math::Color ink = p.dead ? kInkDead : kInk;

            // ping (or BOT) at the left edge, like CS:GO
            if (p.bot)
                std::snprintf(buf, sizeof(buf), "BOT");
            else
                std::snprintf(buf, sizeof(buf), "%d", std::max(p.ping, 0));
            label_.Draw(buf, {ping_x, ry + (row_h - ppx) * 0.5f - 1 * s}, ppx, false, kInkDim, Cs2Text::Align::Right);

            // avatar: the Steam picture in a team-coloured frame, or the initial on a team square
            float av = row_h - 6 * s, ay = ry + 3 * s, frame = std::max(1.0f, 1.2f * s);
            draw_.DrawRect({avatar_x - frame, ay - frame}, {avatar_x + av + frame, ay + av + frame}, Alpha(tint, p.dead ? 70 : 200));
            if (unsigned int tex = avatars_.Texture(p.steam_id, static_cast<float>(cl_enginefunc()->GetClientTime())))
            {
                draw_.FlushPrims();
                Cs2Art::DrawTexture(tex, avatar_x, ay, av, av, p.dead ? ncl_math::Color{120, 120, 120, 170} : ncl_math::Color{255, 255, 255, 255});
            }
            else
            {
                ncl_math::Color face = p.dead ? ncl_math::Color{70, 70, 72, 255} : Mix(tint, {0, 0, 0, 255}, 0.35f);
                draw_.DrawRect({avatar_x, ay}, {avatar_x + av, ay + av}, face);
                int ipx = Px(av * 0.62f);
                label_.Draw(Initial(p.name).c_str(), {avatar_x + av * 0.5f, ay + (av - ipx) * 0.5f - 1 * s}, ipx, true,
                            p.dead ? kInkDead : ncl_math::Color{255, 255, 255, 235}, Cs2Text::Align::Center);
            }

            float ty = ry + (row_h - fpx) * 0.5f - 1 * s;
            std::string name = label_.Fit(p.name.c_str(), fpx, false, name_max - (p.bomb ? 40 * s : 0));
            float nw = label_.Draw(name.c_str(), {name_x, ty}, fpx, false, ink);
            if (p.bomb)
            {
                float bx = name_x + nw + 10 * s, bh = row_h * 0.62f;
                draw_.DrawRect({bx, ry + (row_h - bh) * 0.5f}, {bx + 26 * s, ry + (row_h + bh) * 0.5f}, kBomb);
                int bpx = Px(bh * 0.72f);
                label_.Draw("C4", {bx + 13 * s, ry + (row_h - bpx) * 0.5f - 1 * s}, bpx, true, {24, 14, 6, 255}, Cs2Text::Align::Center);
            }

            float dy = ry + (row_h - dpx) * 0.5f;
            if (p.money >= 0)
            {
                std::snprintf(buf, sizeof(buf), "$%d", p.money);
                text_.Draw(buf, {col_money, dy}, dpx, false, ink, Cs2Text::Align::Right);
            }
            std::snprintf(buf, sizeof(buf), "%d", p.frags);
            text_.Draw(buf, {col_kills, dy}, dpx, false, ink, Cs2Text::Align::Right);
            std::snprintf(buf, sizeof(buf), "%d", p.deaths);
            text_.Draw(buf, {col_deaths, dy}, dpx, false, ink, Cs2Text::Align::Right);
        }
    };

    draw_team(0, y, rows_ct);
    y += rows_ct * row_h;

    // round history: a numbered line, CT wins above it, T wins below
    {
        float line_y = y + strip_h * 0.5f;
        float sx0 = table_x0 + 60 * s, sx1 = table_x1 - 60 * s;
        constexpr int kSlots = 30;
        float step = (sx1 - sx0) / kSlots;
        draw_.DrawRect({sx0, line_y}, {sx1, line_y + std::max(1.0f, 1.2f * s)}, {255, 255, 255, 50});
        int tick_px = Px(11 * s);
        for (int i = 1; i <= kSlots; i++)
        {
            float x = sx0 + step * (i - 0.5f);
            if (i % 5 == 0)
            {
                std::snprintf(buf, sizeof(buf), "%d", i);
                text_.Draw(buf, {x, line_y - tick_px * 0.5f}, tick_px, false, kInkDim, Cs2Text::Align::Center);
            }
            else
            {
                draw_.DrawRect({x - 1.5f * s, line_y - 0.5f * s}, {x + 1.5f * s, line_y + 1.7f * s}, {255, 255, 255, 70});
            }
        }
        size_t first = st.round_winners.size() > kSlots ? st.round_winners.size() - kSlots : 0;
        for (size_t i = first; i < st.round_winners.size(); i++)
        {
            float x = sx0 + step * (static_cast<float>(i - first) + 0.5f);
            bool ct = st.round_winners[i] == 'C';
            float yy = ct ? line_y - 14 * s : line_y + 15 * s;
            draw_.DrawDisc({x, yy}, 5 * s, ct ? kTeamCT : kTeamT);
        }
        int label_px = Px(12 * s);
        label_.Draw("Раунды", {table_x0, line_y - label_px * 0.5f - 1 * s}, label_px, false, kInkDim);
        std::snprintf(buf, sizeof(buf), "%d", st.score_ct);
        text_.Draw(buf, {table_x1, line_y - 24 * s}, Px(18 * s), true, kTeamCT, Cs2Text::Align::Right);
        std::snprintf(buf, sizeof(buf), "%d", st.score_t);
        text_.Draw(buf, {table_x1, line_y + 6 * s}, Px(18 * s), true, kTeamT, Cs2Text::Align::Right);
    }
    y += strip_h;

    draw_team(1, y, rows_t);
    y += rows_t * row_h;

    if (!spectators.empty())
    {
        int px = Px(14 * s);
        std::string line = label_.Fit(("Наблюдатели: " + spectators).c_str(), px, false, table_x1 - table_x0);
        label_.Draw(line.c_str(), {table_x0, y + 12 * s}, px, false, kInkDim);
    }

    draw_.EndPrims();
    draw_.EndFrame();
}
