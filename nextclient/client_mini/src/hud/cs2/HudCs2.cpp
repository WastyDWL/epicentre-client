#include "HudCs2.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include <ncl_gl/context.h>

#include "Cs2HudState.h"
#include "main.h"

namespace
{
    using goldsrc::cstrike::HIDEHUD_ALL;
    using goldsrc::cstrike::HIDEHUD_HEALTH;
    using goldsrc::cstrike::HIDEHUD_MONEY;
    using goldsrc::cstrike::HIDEHUD_TIMER;
    using goldsrc::cstrike::HIDEHUD_WEAPONS;

    // CS2's HUD ink: a warm off-white, red for low health, the team tints of the emblem
    constexpr ncl_math::Color kInk{236, 229, 205, 255};
    constexpr ncl_math::Color kInkDim{236, 229, 205, 150};
    constexpr ncl_math::Color kLow{235, 72, 60, 255};
    constexpr ncl_math::Color kMoney{232, 214, 150, 255};
    constexpr ncl_math::Color kTeamT{222, 155, 53, 255};
    constexpr ncl_math::Color kTeamCT{110, 150, 214, 255};
    constexpr ncl_math::Color kNeutral{170, 170, 170, 255};

    constexpr int kLowHealth = 25;

    // distances from the bottom edge at 1080p: the divider line (emblem centre) and the bars
    constexpr float kLineY = 34.0f;
    constexpr float kBarY = 44.0f;
    // how far above the divider the health / armor / ammo / money blocks are centred
    constexpr float kRaise = 26.0f;

    ncl_math::Color WithAlpha(ncl_math::Color c, int a)
    {
        return {c.r, c.g, c.b, static_cast<uint8_t>(std::clamp(a, 0, 255))};
    }

    int Px(float v)
    {
        return std::max(8, static_cast<int>(v + 0.5f));
    }
} // namespace

HudCs2::HudCs2(nitroapi::NitroApiInterface* nitro_api, radar::integration::Performance& performance) :
    HudBaseHelper(nitro_api),
    draw_(performance),
    text_(draw_, true),
    label_(draw_, false)
{
}

void HudCs2::Init()
{
    enabled_cvar_ = cl_enginefunc()->pfnRegisterVariable("cl_hud_cs2", "1", goldsrc::FCVAR_ARCHIVE);
    crosshair_cvar_ = cl_enginefunc()->pfnGetCvarPointer("crosshair");
    // kill cards over the emblem: 1 CS2 style, 2 classic aces, 0 the drawn emblem without cards
    cards_cvar_ = cl_enginefunc()->pfnRegisterVariable("cl_hud_cs2_cards", "1", goldsrc::FCVAR_ARCHIVE);
    // the buy menu: 1 GameUI's CS2-style page, 0 client.dll's own
    buy_cvar_ = cl_enginefunc()->pfnRegisterVariable("cl_buy_cs2", "1", goldsrc::FCVAR_ARCHIVE);
}

void HudCs2::VidInit()
{
    // a video restart may bring a new GL context: the programs and fonts are re-made on demand
    draw_.ResetGl(false);
    text_.Reset();
    label_.Reset();
}

void HudCs2::Think(float time)
{
    // decides, at the next +showscores, whether Tab opens this board or client.dll's
    Cs2State().scoreboard_own = IsEnabled();
    Cs2State().buy_own = buy_cvar_ && buy_cvar_->value != 0.0f;
    Cs2HudState_SyncBuyMenu(LocalTeam(), !m_fPlayerDead && g_iUser1 == 0);
}

bool HudCs2::IsEnabled() const
{
    return enabled_cvar_ && enabled_cvar_->value != 0.0f;
}

int HudCs2::HideFlags() const
{
    // the stock ammo numbers go through Cs2HudState_ClipStockAmmo instead of HIDEHUD_WEAPONS,
    // which would take the weapon selection and the crosshair with them
    return IsEnabled() ? (HIDEHUD_HEALTH | HIDEHUD_MONEY | HIDEHUD_TIMER) : 0;
}

bool HudCs2::ShouldDrawCrosshair()
{
    if (!(HideFlags() & HIDEHUD_WEAPONS) || g_CurrentWeaponId <= 0)
        return false;
    int hide = *gHUD()->m_iHideHUDDisplay;
    if (hide & (HIDEHUD_ALL | HIDEHUD_WEAPONS | goldsrc::cstrike::HIDEHUD_CROSSHAIR))
        return false;
    if (m_fPlayerDead || !(m_iWeaponBits & (1 << goldsrc::cstrike::WEAPON_SUIT)) || cl_enginefunc()->IsSpectateOnly())
        return false;
    if (crosshair_cvar_ && crosshair_cvar_->value == 0.0f)
        return false;
    // sniper scopes draw their own reticle
    int fov = gHUD()->m_iFOV ? *gHUD()->m_iFOV : 90;
    return fov == 0 || fov > 40;
}

bool HudCs2::PrepareGraphics()
{
    void* context = ncl_gl::CurrentContext();
    if (!context)
        return false;
    if (!draw_.is_context(context, nullptr))
    {
        draw_.ResetGl(false);
        text_.Reset();
        label_.Reset();
        art_.Reset();
        avatars_.Reset();
        draw_.Prepare(context, nullptr);
    }
    return draw_.CheckPrimsAvailable();
}

void HudCs2::Draw(float time)
{
    if (!IsEnabled() || (m_iHideHUDDisplay & HIDEHUD_ALL))
        return;
    if (!PrepareGraphics())
        return;
    // in-eye spectating looks alive to the HUD (it carries the followed player's state): the observer
    // mode decides, and the spectator panel takes over
    bool alive = !m_fPlayerDead && (m_iWeaponBits & (1 << goldsrc::cstrike::WEAPON_SUIT)) && !cl_enginefunc()->IsSpectateOnly() &&
                 g_iUser1 == 0;

    int iw, ih;
    GetScreenResolution(iw, ih);
    float w = static_cast<float>(iw), h = static_cast<float>(ih);
    float s = h / 1080.0f;
    float cx = w * 0.5f;

    if (!text_.Available(Px(54 * s), true))
        return;

    draw_.BeginPrims();
    if (!(m_iHideHUDDisplay & goldsrc::cstrike::HIDEHUD_TIMER))
        DrawTopBar(cx, s);
    if (alive)
    {
        DrawBackdrop(w, h, s);
        DrawEmblem(cx, h, s);
        if (!(m_iHideHUDDisplay & HIDEHUD_HEALTH))
            DrawHealthArmor(cx, h, s);
        if (!(m_iHideHUDDisplay & HIDEHUD_WEAPONS))
            DrawAmmo(cx, h, s);
        if (!(m_iHideHUDDisplay & HIDEHUD_MONEY))
            DrawMoney(h, s);
    }
    else
    {
        DrawSpectatorPanel(w, h, s);
    }
    draw_.EndPrims();
    draw_.EndFrame();

    Cs2HudState_SyncStockScoreboard();
    if (Cs2State().scoreboard_held)
        DrawScoreboard(w, h, s);
}

void HudCs2::DrawBackdrop(float w, float h, float s)
{
    // soft dark band behind the bottom row, darker towards the screen edge
    constexpr int kSteps = 10;
    float band = 120 * s;
    for (int i = 0; i < kSteps; i++)
    {
        float y0 = h - band + band * i / kSteps;
        float y1 = h - band + band * (i + 1) / kSteps;
        draw_.DrawRect({0, y0}, {w, y1}, {0, 0, 0, static_cast<uint8_t>(10 + 9 * i)});
    }
}

bool HudCs2::Icon(const char* name, float x, float y, float w, float h, ncl_math::Color tint)
{
    if (!art_.Has(name))
        return false;
    // the art is a fixed-function quad: commit the queued primitives under it first
    draw_.FlushPrims();
    art_.Draw(name, x, y, w, h, tint);
    return true;
}

int HudCs2::LocalTeam()
{
    goldsrc::cl_entity_t* local = cl_enginefunc()->GetLocalPlayer();
    return local && IsValidClientIndex(local->index) ? cl()->g_PlayerExtraInfo[local->index].teamnumber : 0;
}

const HudCs2::CardStyle* HudCs2::FindCardStyle(const char* name)
{
    if (!card_styles_loaded_)
    {
        card_styles_loaded_ = true;
        if (FILE* f = std::fopen("cstrike/resource/epicentre_hud/cards/cards.txt", "r"))
        {
            char line[128];
            while (std::fgets(line, sizeof(line), f))
            {
                CardStyle c{};
                if (line[0] != '#' && std::sscanf(line, "%15s %d %d %d %d %d", c.name, &c.tex_w, &c.tex_h, &c.strip_w, &c.strip_h, &c.line) == 6)
                    card_styles_.push_back(c);
            }
            std::fclose(f);
        }
    }
    for (const CardStyle& c : card_styles_)
    {
        if (std::strcmp(c.name, name) == 0)
            return &c;
    }
    return nullptr;
}

bool HudCs2::DrawKillStrip(float cx, float line_y, float h, bool terrorist)
{
    int style_id = cards_cvar_ ? static_cast<int>(cards_cvar_->value) : 1;
    const char* style_name = style_id == 1 ? "cs2" : style_id == 2 ? "classic" : nullptr;
    const CardStyle* style = style_name ? FindCardStyle(style_name) : nullptr;
    if (!style || style->tex_w <= 0 || style->tex_h <= 0)
        return false;

    // the overlays covered a 16:9 screen, so the strip keeps that stretch at this height
    float kx = h * 16.0f / 9.0f / style->tex_w, ky = h / style->tex_h;
    const Cs2HudState& st = Cs2State();
    int kills = std::clamp(st.round_kills, 0, 5);

    auto frame = [&](int n, float alpha, float scale) {
        char name[64];
        std::snprintf(name, sizeof(name), "cards/%s_%s_%d", style->name, terrorist ? "tt" : "ct", n + 1);
        float dw = style->strip_w * kx * scale, dh = style->strip_h * ky * scale;
        return Icon(name, cx - dw * 0.5f, line_y - style->line * ky * scale, dw, dh, {255, 255, 255, static_cast<uint8_t>(alpha * 255)});
    };

    // a new card pops in: the previous strip fades out under the new one, which settles from 108%
    float t = static_cast<float>(cl_enginefunc()->GetClientTime()) - st.last_kill_time;
    constexpr float kPop = 0.28f;
    if (kills > 0 && t >= 0.0f && t < kPop)
    {
        float k = t / kPop;
        float ease = 1.0f - (1.0f - k) * (1.0f - k);
        frame(kills - 1, 1.0f - ease, 1.0f);
        return frame(kills, ease, 1.0f + 0.08f * (1.0f - ease));
    }
    return frame(kills, 1.0f, 1.0f);
}

void HudCs2::DrawEmblem(float cx, float h, float s)
{
    float y = h - kLineY * s;
    float line_y = y;

    int tn = LocalTeam();
    ncl_math::Color team = tn == goldsrc::cstrike::TEAM_TERRORIST ? kTeamT : tn == goldsrc::cstrike::TEAM_CT ? kTeamCT : kNeutral;

    // the Kill Overlay Manager strip of the chosen style: divider, emblem and this round's kill cards
    if (DrawKillStrip(cx, line_y, h, tn == goldsrc::cstrike::TEAM_TERRORIST))
        return;

    // the divider: one hairline each side, fading out from the emblem (the same for both teams;
    // the pack's two divider pictures differ in size and emblem)
    float span = 400 * s, gap = 30 * s, thick = std::max(1.0f, 1.5f * s);
    ncl_math::Color near_ink = WithAlpha(kInk, 150), far_ink = WithAlpha(kInk, 0);
    DrawGradient(cx - gap - span, line_y, cx - gap, line_y + thick, far_ink, near_ink, near_ink, far_ink);
    DrawGradient(cx + gap, line_y, cx + gap + span, line_y + thick, near_ink, far_ink, far_ink, near_ink);

    // the emblem: the team's round logo, as on the scoreboard
    float logo = 50 * s;
    draw_.DrawDisc({cx, y}, logo * 0.5f, {10, 10, 12, 200});
    const char* art = tn == goldsrc::cstrike::TEAM_TERRORIST ? "logo_tr" : tn == goldsrc::cstrike::TEAM_CT ? "logo_ct" : nullptr;
    if (art && Icon(art, cx - logo * 0.5f, y - logo * 0.5f, logo, logo, {255, 255, 255, 245}))
        return;

    draw_.DrawDisc({cx, y}, 22 * s, {12, 12, 14, 200});
    draw_.DrawRing({cx, y}, 24 * s, 21 * s, WithAlpha(kInk, 220));
    draw_.DrawRing({cx, y}, 15 * s, 12.5f * s, WithAlpha(team, 255));
    draw_.DrawDisc({cx, y}, 6 * s, WithAlpha(team, 255));
}

void HudCs2::DrawHealthArmor(float cx, float h, float s)
{
    int health = std::clamp(gHUD()->m_Health->m_iHealth, 0, 999);
    const Cs2HudState& st = Cs2State();
    bool low = health <= kLowHealth;
    ncl_math::Color ink = low ? kLow : kInk;

    // CS2's places, as fractions of the screen width: the armor shield at 0.297 with its value
    // inside, the health from 0.318, both centred on the divider line
    float w = cx * 2.0f;
    float line_y = h - kLineY * s;
    int big = Px(40 * s);
    // the blocks stand above the divider (CS2), clear of it
    float mid = line_y - kRaise * s;
    float digits_top = mid - big * 0.56f;

    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d", health);
    float hx = w * 0.305f;
    float num_w = text_.Draw(buf, {hx, digits_top}, big, true, ink);

    // the health bar under the number: red as in CS2 once it runs low
    float bar_w = std::max(num_w, 40 * s), bar_y = line_y - 8 * s;
    draw_.DrawRect({hx, bar_y}, {hx + bar_w, bar_y + 3 * s}, {255, 255, 255, 45});
    draw_.DrawRect({hx, bar_y}, {hx + bar_w * std::min(health, 100) / 100.0f, bar_y + 3 * s}, low ? kLow : WithAlpha(kInk, 230));

    // the armor: a shield outline with its value inside (CS2), a dome on top for the helmet
    float sx = w * 0.282f, sy = mid;
    float sw = 15 * s, top_y = sy - 16 * s, mid_y = sy + 6 * s, tip_y = sy + 20 * s;
    ncl_math::Color rim = WithAlpha(kInk, st.armor > 0 ? 235 : 120);
    draw_.DrawRect({sx - sw, top_y}, {sx + sw, mid_y}, rim);
    draw_.DrawTriangle({sx - sw, mid_y}, {sx + sw, mid_y}, {sx, tip_y}, rim);
    float in = 2.2f * s;
    ncl_math::Color face{18, 18, 20, 210};
    draw_.DrawRect({sx - sw + in, top_y + in}, {sx + sw - in, mid_y}, face);
    draw_.DrawTriangle({sx - sw + in, mid_y}, {sx + sw - in, mid_y}, {sx, tip_y - in * 1.8f}, face);
    if (st.helmet)
        draw_.DrawRect({sx - sw * 0.55f, top_y - 4 * s}, {sx + sw * 0.55f, top_y - 1.5f * s}, rim);
    std::snprintf(buf, sizeof(buf), "%d", std::clamp(st.armor, 0, 999));
    int apx = Px(16 * s);
    text_.Draw(buf, {sx, sy - apx * 0.62f}, apx, true, st.armor > 0 ? kInk : kInkDim, Cs2Text::Align::Center);
}

void HudCs2::DrawAmmo(float cx, float h, float s)
{
    const Cs2HudState& st = Cs2State();
    if (st.weapon <= 0)
        return;

    int reserve = st.Reserve();
    int clip = st.clip;
    if (clip < 0 && reserve < 0)
        return; // knife

    // CS2's places: the clip from 0.632 of the width, then "| reserve" smaller and the bullets,
    // centred on the divider line
    float w = cx * 2.0f;
    float line_y = h - kLineY * s;
    int big = Px(40 * s), small = Px(24 * s);
    float mid = line_y - kRaise * s;
    float top = mid - big * 0.56f, small_top = mid - small * 0.52f;
    char buf[16];

    float x = w * 0.655f;
    if (clip >= 0)
    {
        std::snprintf(buf, sizeof(buf), "%d", clip);
        x += text_.Draw(buf, {x, top}, big, true, clip == 0 ? kLow : kInk);
        if (reserve >= 0)
        {
            x += 10 * s;
            draw_.DrawRect({x, mid - small * 0.36f}, {x + std::max(1.0f, 1.5f * s), mid + small * 0.34f}, WithAlpha(kInk, 150));
            x += 9 * s;
            std::snprintf(buf, sizeof(buf), "%d", reserve);
            x += text_.Draw(buf, {x, small_top}, small, true, reserve == 0 ? kLow : WithAlpha(kInk, 200));
        }
    }
    else
    {
        // grenades, C4: just the count
        std::snprintf(buf, sizeof(buf), "%d", reserve);
        x += text_.Draw(buf, {x, top}, big, true, kInk);
    }

    // the bullet icon
    float bx = x + 12 * s;
    if (!Icon("ico_bullet", bx, mid - 12 * s, 24 * s, 24 * s, WithAlpha(kInk, 210)))
    {
        for (int i = 0; i < 3; i++)
        {
            float yy = mid - 7 * s + i * 6 * s;
            draw_.DrawRect({bx, yy}, {bx + 14 * s, yy + 3 * s}, WithAlpha(kInk, 190));
        }
    }
}

void HudCs2::DrawTopBar(float cx, float s)
{
    const Cs2HudState& st = Cs2State();
    float top = 8 * s, height = 50 * s, half = 50 * s, side = 62 * s;

    // team scores either side of the clock
    draw_.DrawRect({cx - half - side, top}, {cx - half - 2 * s, top + height}, {0, 0, 0, 140});
    draw_.DrawRect({cx + half + 2 * s, top}, {cx + half + side, top + height}, {0, 0, 0, 140});
    draw_.DrawRect({cx - half - side, top + height - 3 * s}, {cx - half - 2 * s, top + height}, kTeamT);
    draw_.DrawRect({cx + half + 2 * s, top + height - 3 * s}, {cx + half + side, top + height}, kTeamCT);

    char buf[16];
    int score_px = Px(30 * s);
    std::snprintf(buf, sizeof(buf), "%d", st.score_t);
    text_.Draw(buf, {cx - half - side * 0.5f - 1 * s, top + 8 * s}, score_px, true, kTeamT, Cs2Text::Align::Center);
    std::snprintf(buf, sizeof(buf), "%d", st.score_ct);
    text_.Draw(buf, {cx + half + side * 0.5f + 1 * s, top + 8 * s}, score_px, true, kTeamCT, Cs2Text::Align::Center);

    // the players' avatars outside the score boxes (CS2), the team logos while nobody is in a team
    std::vector<PlayerView> players = CollectPlayers();
    int alive_t = 0, alive_ct = 0;
    for (const PlayerView& p : players)
    {
        if (!p.dead)
            (p.team == goldsrc::cstrike::TEAM_TERRORIST ? alive_t : alive_ct)++;
    }
    // each team's logo with its count of living players beside the score boxes
    DrawTopCounters(alive_t, alive_ct, cx - half - side - 6 * s, cx + half + side + 6 * s, top, height, s);

    // the clock
    draw_.DrawRect({cx - half, top}, {cx + half, top + height}, {0, 0, 0, 170});
    float left = st.round_end - static_cast<float>(cl_enginefunc()->GetClientTime());
    if (st.bomb_planted || left <= 0.0f)
    {
        // no round clock running (bomb planted, round over): a red dot in place of the time
        draw_.DrawDisc({cx, top + height * 0.5f}, 7 * s, kLow);
        return;
    }
    int secs = static_cast<int>(left);
    std::snprintf(buf, sizeof(buf), "%d:%02d", secs / 60, secs % 60);
    bool panic = secs <= 10;
    text_.Draw(buf, {cx, top + 7 * s}, Px(32 * s), true, panic ? kLow : kInk, Cs2Text::Align::Center);
}

void HudCs2::DrawMoney(float h, float s)
{
    const Cs2HudState& st = Cs2State();
    if (!st.has_money)
        return;
    char buf[24];
    std::snprintf(buf, sizeof(buf), "$%d", st.money);
    // bottom-left corner, on the divider line's height like CS2
    int px = Px(34 * s);
    text_.Draw(buf, {20 * s, h - (kLineY + kRaise) * s - px * 0.56f}, px, true, kMoney);
}
