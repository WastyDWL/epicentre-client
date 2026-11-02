#pragma once

#include <goldsrc/cvar.h>

#include "hud/HudBase.h"
#include "hud/HudBaseHelper.h"
#include "hud/radar/integration/Performance.h"
#include "hud/radar/render/RadarDraw.h"
#include "Cs2Art.h"
#include "Cs2Text.h"

// CS2-style bottom HUD: armor and health left of a centre emblem, ammo right of it, money in the
// bottom-left corner. While it is on (cl_hud_cs2 1) the stock health/armor, ammo and money are
// hidden for the stock draw pass through HideFlags().
class HudCs2 : public HudBase, public HudBaseHelper
{
    RadarDrawContext draw_;
    Cs2Text text_;
    Cs2Art art_;
    goldsrc::cvar_t* enabled_cvar_{};
    goldsrc::cvar_t* crosshair_cvar_{};

public:
    HudCs2(nitroapi::NitroApiInterface* nitro_api, radar::integration::Performance& performance);

    void Init() override;
    void VidInit() override;
    void Draw(float time) override;

    bool IsEnabled() const;

    // m_iHideHUDDisplay bits to add while client.dll draws its own HUD.
    int HideFlags() const;

    // Hiding the stock ammo pass also skips its crosshair call: the same checks it makes.
    bool ShouldDrawCrosshair();

private:
    bool PrepareGraphics();
    void DrawBackdrop(float w, float h, float s);
    void DrawHealthArmor(float cx, float h, float s);
    void DrawAmmo(float cx, float h, float s);
    void DrawMoney(float h, float s);
    void DrawEmblem(float cx, float h, float s);
    void DrawTopBar(float cx, float s);

    // Draws one art piece when the HUD folder has it; false lets the caller draw its fallback.
    bool Icon(const char* name, float x, float y, float w, float h, ncl_math::Color tint);
    int LocalTeam();
};
