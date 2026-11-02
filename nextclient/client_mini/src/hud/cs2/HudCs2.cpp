#include "HudCs2.h"

#include <algorithm>
#include <cstdio>

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
    bool alive = !m_fPlayerDead && (m_iWeaponBits & (1 << goldsrc::cstrike::WEAPON_SUIT)) && !cl_enginefunc()->IsSpectateOnly();

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

void HudCs2::DrawEmblem(float cx, float h, float s)
{
    float y = h - kLineY * s;
    float line_y = y;

    // the CS2 divider art: hairline with the team emblem in the middle
    int tn = LocalTeam();
    if (Icon(tn == goldsrc::cstrike::TEAM_TERRORIST ? "hud_bot_tr" : "hud_bot", cx - 320 * s, y - 80 * s, 640 * s, 160 * s, {255, 255, 255, 235}))
        return;

    // the divider: two hairlines fading out from the emblem
    constexpr int kSegments = 8;
    float span = 400 * s, gap = 36 * s;
    for (int i = 0; i < kSegments; i++)
    {
        float a0 = gap + span * i / kSegments, a1 = gap + span * (i + 1) / kSegments;
        uint8_t alpha = static_cast<uint8_t>(110 - 12 * i);
        draw_.DrawRect({cx - a1, line_y}, {cx - a0, line_y + std::max(1.0f, 1.5f * s)}, WithAlpha(kInk, alpha));
        draw_.DrawRect({cx + a0, line_y}, {cx + a1, line_y + std::max(1.0f, 1.5f * s)}, WithAlpha(kInk, alpha));
    }

    ncl_math::Color team = tn == goldsrc::cstrike::TEAM_TERRORIST ? kTeamT : tn == goldsrc::cstrike::TEAM_CT ? kTeamCT : kNeutral;

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

    int big = Px(54 * s);
    float top = h - 96 * s;
    float right = cx - 64 * s;

    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d", health);
    float num_w = text_.Measure(buf, big, true);
    text_.Draw(buf, {right, top}, big, true, ink, Cs2Text::Align::Right);

    // health bar under the number
    float bar_w = std::max(num_w, 92 * s);
    float bar_y = h - kBarY * s;
    draw_.DrawRect({right - bar_w, bar_y}, {right, bar_y + 3 * s}, {255, 255, 255, 40});
    draw_.DrawRect({right - bar_w, bar_y}, {right - bar_w + bar_w * std::min(health, 100) / 100.0f, bar_y + 3 * s}, ink);

    // cross icon
    float ix = right - bar_w - 26 * s, iy = top + big * 0.55f, arm = 11 * s, thick = 3.5f * s;
    if (!Icon("ico_health", ix - 14 * s, iy - 14 * s, 28 * s, 28 * s, WithAlpha(ink, 235)))
    {
        draw_.DrawRect({ix - arm, iy - thick}, {ix + arm, iy + thick}, WithAlpha(ink, 230));
        draw_.DrawRect({ix - thick, iy - arm}, {ix + thick, iy + arm}, WithAlpha(ink, 230));
    }

    // armor: shield and value, further left
    int small = Px(32 * s);
    float armor_right = ix - 46 * s;
    std::snprintf(buf, sizeof(buf), "%d", std::clamp(st.armor, 0, 999));
    float armor_top = top + (big - small) * 0.72f;
    float armor_w = text_.Draw(buf, {armor_right, armor_top}, small, true, st.armor > 0 ? kInk : kInkDim, Cs2Text::Align::Right);

    float sx = armor_right - armor_w - 18 * s, sy = armor_top + small * 0.5f;
    float sw = 9 * s, sh = 7 * s;
    ncl_math::Color shield = WithAlpha(st.armor > 0 ? kInk : kInkDim, 220);
    if (Icon(st.helmet ? "ico_armorhelm" : "ico_armor", sx - 16 * s, sy - 16 * s, 32 * s, 32 * s, shield))
        return;
    draw_.DrawRect({sx - sw, sy - sh - 2 * s}, {sx + sw, sy + 2 * s}, shield);
    draw_.DrawTriangle({sx - sw, sy + 2 * s}, {sx + sw, sy + 2 * s}, {sx, sy + 13 * s}, shield);
    if (st.helmet)
        draw_.DrawRect({sx - sw * 0.6f, sy - sh - 6 * s}, {sx + sw * 0.6f, sy - sh - 3.5f * s}, shield);
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

    int big = Px(54 * s), small = Px(30 * s);
    float top = h - 96 * s;
    float left = cx + 64 * s;
    char buf[16];

    float x = left;
    if (clip >= 0)
    {
        std::snprintf(buf, sizeof(buf), "%d", clip);
        x += text_.Draw(buf, {x, top}, big, true, clip == 0 ? kLow : kInk);
        if (reserve >= 0)
        {
            std::snprintf(buf, sizeof(buf), "%d", reserve);
            x += 12 * s;
            x += text_.Draw(buf, {x, top + (big - small) * 0.72f}, small, true, reserve == 0 ? kLow : kInkDim);
        }
    }
    else
    {
        // grenades, C4: just the count
        std::snprintf(buf, sizeof(buf), "%d", reserve);
        x += text_.Draw(buf, {x, top}, big, true, kInk);
    }

    // bullets icon
    float bx = x + 16 * s, by = top + big * 0.36f;
    if (!Icon("ico_bullet", bx - 2 * s, top + big * 0.55f - 15 * s, 30 * s, 30 * s, WithAlpha(kInkDim, 230)))
    {
        for (int i = 0; i < 3; i++)
        {
            float yy = by + i * 7 * s;
            draw_.DrawRect({bx, yy}, {bx + 16 * s, yy + 3.5f * s}, WithAlpha(kInkDim, 200));
        }
    }

    // a thin bar under the ammo, mirroring the health bar
    float bar_y = h - kBarY * s;
    draw_.DrawRect({left, bar_y}, {left + std::max(x - left, 92 * s), bar_y + 3 * s}, {255, 255, 255, 40});
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

    // team logos outside the score boxes
    float logo = 40 * s, ly = top + (height - logo) * 0.5f;
    Icon("logo_tr", cx - half - side - logo - 6 * s, ly, logo, logo, {255, 255, 255, 240});
    Icon("logo_ct", cx + half + side + 6 * s, ly, logo, logo, {255, 255, 255, 240});

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
    text_.Draw(buf, {28 * s, h - 72 * s}, Px(40 * s), true, kMoney);
}
