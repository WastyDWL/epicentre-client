#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <goldsrc/cvar.h>

#include "hud/HudBase.h"
#include "hud/HudBaseHelper.h"
#include "hud/radar/integration/Performance.h"
#include "hud/radar/render/RadarDraw.h"
#include "Cs2Art.h"
#include "Cs2Avatars.h"
#include "Cs2Text.h"

// CS2-style bottom HUD: armor and health left of a centre emblem, ammo right of it, money in the
// bottom-left corner. While it is on (cl_hud_cs2 1) the stock health/armor, ammo and money are
// hidden for the stock draw pass through HideFlags().
class HudCs2 : public HudBase, public HudBaseHelper
{
    RadarDrawContext draw_;
    Cs2Text text_;   // digits: StratumNo2
    Cs2Text label_;  // names and captions: Bahnschrift (Cyrillic)
    Cs2Art art_;
    Cs2Avatars avatars_;
    goldsrc::cvar_t* enabled_cvar_{};
    goldsrc::cvar_t* crosshair_cvar_{};
    goldsrc::cvar_t* cards_cvar_{};

    // cards.txt from tools/import-cs2-hud.py --kill-cards
    struct CardStyle
    {
        char name[16];
        int tex_w, tex_h, strip_w, strip_h, line;
    };
    std::vector<CardStyle> card_styles_;
    bool card_styles_loaded_ = false;
    const CardStyle* FindCardStyle(const char* name);
    // false when the style's art is missing, so the drawn emblem stands in
    bool DrawKillStrip(float cx, float line_y, float h, bool terrorist);

public:
    HudCs2(nitroapi::NitroApiInterface* nitro_api, radar::integration::Performance& performance);

    void Init() override;
    void VidInit() override;
    void Draw(float time) override;
    void Think(float time) override;

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
    void DrawScoreboard(float w, float h, float s); // HudCs2Scoreboard.cpp

    // HudCs2Players.cpp: the players in the two teams, the top bar's avatars and the spectator panel
    struct PlayerView
    {
        int index;
        uint64_t steam_id;
        std::string name;
        int team;
        bool dead;
        int frags;
        int deaths;
        int health; // 0 when the client is not told
        bool bomb;
    };
    std::vector<PlayerView> CollectPlayers();
    // left_edge / right_edge: where the T block ends and the CT block starts
    void DrawTopCounters(int alive_t, int alive_ct, float left_edge, float right_edge, float top, float height, float s);
    void DrawSpectatorPanel(float w, float h, float s);
    unsigned int AvatarOf(uint64_t steam_id);
    // corner colours: top-left, top-right, bottom-right, bottom-left
    void DrawGradient(float x0, float y0, float x1, float y1, ncl_math::Color tl, ncl_math::Color tr, ncl_math::Color br, ncl_math::Color bl);

    // Draws one art piece when the HUD folder has it; false lets the caller draw its fallback.
    bool Icon(const char* name, float x, float y, float w, float h, ncl_math::Color tint);
    int LocalTeam();
};
