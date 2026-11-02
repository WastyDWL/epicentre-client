#include "GameHud.h"
#include <goldsrc/client_api.h>

#include "main.h"

#include "hud/radar/integration/RadarConfig.h"
#include "hud/radar/integration/RadarDebug.h"

extern goldsrc::cl_enginefunc_t gEngfuncs;

GameHud::GameHud(
    nitroapi::NitroApiInterface* nitro_api,
    radar::integration::Performance& performance,
    taskcoro::TaskCoroImplInterface& scheduler
)
{
    health_ = std::make_shared<HudHealth>(nitro_api);
    all_hud_.push_back(health_);

    crosshair_ = std::make_shared<HudCrosshair>(nitro_api);
    all_hud_.push_back(crosshair_);

    sprite_store_ = std::make_shared<HudSpriteStore>();
    all_hud_.push_back(sprite_store_);

    damage_icons_ = std::make_shared<HudDamageIcons>(nitro_api);
    all_hud_.push_back(damage_icons_);

    damage_direction_ = std::make_shared<HudDamageDirection>(nitro_api);
    all_hud_.push_back(damage_direction_);

    radar_advanced_ = std::make_shared<HudRadarAdvanced>(nitro_api, performance, scheduler);
    radar_ = std::make_shared<HudRadar>(nitro_api, radar_advanced_->get_draw_context());
    all_hud_.push_back(radar_);
    all_hud_.push_back(radar_advanced_);

    death_notice_ = std::make_shared<HudDeathNotice>(nitro_api);
    all_hud_.push_back(death_notice_);

    ammo_ = std::make_shared<HudAmmo>(nitro_api);
    all_hud_.push_back(ammo_);

    cs2_ = std::make_shared<HudCs2>(nitro_api, performance);
    all_hud_.push_back(cs2_);
}

void GameHud::Init()
{
    FetchCvars();

    for (auto& item : all_hud_)
        item->Init();
}

void GameHud::VidInit()
{
    for (auto& item : all_hud_)
        item->VidInit();
}

void GameHud::Draw(float time)
{
    if (cs2_->ShouldDrawCrosshair())
        crosshair_->DrawCrosshair(time, g_CurrentWeaponId);

    for (auto& item : all_hud_) {
        if (item == radar_advanced_)
        {
            continue; // drawn separately via DrawRadar
        }
        if (item == health_ && cs2_->IsEnabled())
        {
            continue; // the CS2 HUD draws health itself
        }
        item->Draw(time);
    }
}

void GameHud::PrepareRadar()
{
    radar_advanced_->PrepareHudTextZone();
}

void GameHud::UpdateRadarScoreboard(bool hud_visible)
{
    radar_advanced_->UpdateScoreboard(hud_visible);
}

void GameHud::DrawRadar(float time)
{
    radar_advanced_->Draw(time);
}

void GameHud::Think(float time)
{
    for (auto& item : all_hud_)
        item->Think(time);
}

void GameHud::Reset()
{
    for (auto& item : all_hud_)
        item->Reset();
}

void GameHud::InitHUDData()
{
    for (auto& item : all_hud_)
        item->InitHUDData();
}

void GameHud::FetchCvars()
{
    cl_righthand = gEngfuncs.pfnGetCvarPointer("cl_righthand");

    RadarConfig_Init();
    RadarDebug_Init();
}

void GameHud::Shutdown()
{
    radar_advanced_->Shutdown();
}
