#include "MainMenuBrowser.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>
#include <fstream>
#include <string>
#include <format>
#include <vgui/ISurfaceNext.h>
#include <nitro_utils/string_utils.h>
#include <steam/steam_api.h>
#include <data_encoding/aes.h>
#include <crosshair/crosshair.h>
#include <cvars/cvar_defaults.h>
#include <goldsrc/client_api.h>

#include "BobPreviewPanel.h"

auto transparent = Color(0, 0, 0, 0);

CMainMenuBrowser::CMainMenuBrowser(vgui2::Panel* parent) 
	: BaseClass(parent, "MainMenuBrowser") {
	SetVisible(false);
	SetBgColor(transparent);

	OpenHomePage();
}

void CMainMenuBrowser::InitializeBrowser() {
	m_pHtml = new vgui2::HTML(this, "MainMenuBrowserHTML");
	// "Main Menu" tells chromehtml.dll this is the main-menu browser (back-to-menu button on sites).
	m_pHtml->InitializeBrowser("Epicentre Main Menu Browser", true);

    std::vector<std::string_view> lang_views;
    nitro_utils::split(SteamApps()->GetAvailableGameLanguages(), ",", lang_views);
    std::vector<std::string> languages(lang_views.begin(), lang_views.end());

	m_pHtml->SetLangSettings(SteamApps()->GetCurrentGameLanguage(), languages);
	m_pHtml->SetSteamId(SteamUser()->GetSteamID().ConvertToUint64());
	m_pHtml->SetContextMenuEnabled(false);

	SetVisible(true);
}

CMainMenuBrowser::~CMainMenuBrowser() {
}

void CMainMenuBrowser::OpenURL(const char *URL) {
	m_pHtml->OpenURL(URL, nullptr);
}

void CMainMenuBrowser::OnThink() {
	BaseClass::OnThink();

	SetBgColor(transparent);

	if (m_pPreview && m_pPreview->IsVisible())
		SyncPreview();
}

void CMainMenuBrowser::SetGamePreview(int x, int y, int w, int h, int move, int demo) {
	if (w <= 0 || h <= 0) {
		if (m_pPreview)
			m_pPreview->SetVisible(false);
		return;
	}

	if (!m_pPreview)
		m_pPreview = new CBobPreviewPanel(this, "EpicentrePreview");  // after m_pHtml: drawn over the page

	m_pPreview->SetBounds(x, y, w, h);
	m_pPreview->SetMoveMode(static_cast<PreviewMove>(std::clamp(move, 0, static_cast<int>(PreviewMove::kIdle))));
	m_pPreview->SetDemo(static_cast<PreviewDemo>(std::clamp(demo, 0, static_cast<int>(PreviewDemo::kWeaponSwitch))));
	SyncPreview();
	m_pPreview->SetVisible(true);
}

// the page writes the cvars right away, so the preview just follows them
void CMainMenuBrowser::SyncPreview() {
	auto f = [](const char* name) { return engine->pfnGetCvarFloat(name); };

	view_bob::BobParams bob;
	bob.style = static_cast<int>(f(cvars::kBobStyle.name));
	bob.bob = f(cvars::kBob.name);
	bob.bob_cycle = f(cvars::kBobCycle.name);
	bob.bob_up = f(cvars::kBobUp.name);
	bob.amt_vert = f(cvars::kBobAmtVert.name);
	bob.amt_lat = f(cvars::kBobAmtLat.name);
	bob.lower_amt = f(cvars::kBobLowerAmt.name);
	bob.camera_bob = f(cvars::kBobCamera.name) != 0.0f;
	m_pPreview->SetBobParams(bob);

	ViewTuningParams tuning;
	tuning.offset_x = f(cvars::kViewmodelOffsetX.name);
	tuning.offset_y = f(cvars::kViewmodelOffsetY.name);
	tuning.offset_z = f(cvars::kViewmodelOffsetZ.name);
	tuning.disable_shift = f(cvars::kViewmodelDisableShift.name) != 0.0f;
	tuning.viewmodel_fov = f(cvars::kViewmodelFov.name);
	tuning.lag_style = static_cast<int>(f(cvars::kViewmodelLagStyle.name));
	tuning.lag_scale = f(cvars::kViewmodelLagScale.name);
	tuning.lag_speed = f(cvars::kViewmodelLagSpeed.name);
	tuning.roll_angle = f(cvars::kRollAngle.name);
	tuning.roll_speed = f(cvars::kRollSpeed.name);
	tuning.camera_move_scale = f(cvars::kCameraMovementScale.name);
	tuning.camera_move_interp = f(cvars::kCameraMovementInterp.name);
	m_pPreview->SetViewTuning(tuning);

	CrosshairParams crosshair;
	crosshair.type = static_cast<int>(f(cvars::kCrosshairType.name));
	int r = 50, g = 250, b = 50;
	if (const char* color = engine->pfnGetCvarString(cvars::kCrosshairColor.name))
		sscanf(color, "%d %d %d", &r, &g, &b);
	crosshair.color = {std::clamp(r, 0, 255), std::clamp(g, 0, 255), std::clamp(b, 0, 255)};
	crosshair.size_index = crosshair::SizeIndex(engine->pfnGetCvarString(cvars::kCrosshairSize.name));
	crosshair.translucent = f(cvars::kCrosshairTranslucent.name) != 0.0f;
	crosshair.dynamic = f(cvars::kDynamicCrosshair.name) != 0.0f;
	m_pPreview->SetCrosshairParams(crosshair);
}

void CMainMenuBrowser::OnScreenSizeChanged(int oldWide, int oldTall) {
	InvalidateLayout();
}

void CMainMenuBrowser::SetFullscreen(bool status) {
	m_bRenderFullscreen = status;
}

void CMainMenuBrowser::PerformLayout() {
	BaseClass::PerformLayout();

	int screenWide, screenTall;
	vgui2::surface()->GetScreenSize(screenWide, screenTall);

	if(m_bRenderFullscreen) {
		SetBounds(0, 0, screenWide, screenTall);
		m_pHtml->SetBounds(0, 0, screenWide, screenTall);
	}
	else {
		auto browserWidth = std::clamp(screenWide - 300, 450, 1152);

		SetBounds(screenWide - browserWidth, 0, browserWidth, screenTall);
		m_pHtml->SetBounds(0, 0, browserWidth, screenTall);
	}
}

std::vector<uint8_t> CMainMenuBrowser::ReadFile(const std::string& fileName) {
	std::ifstream file(fileName.c_str(), std::ios::binary);
	if(!file.is_open()) return {};

    file.seekg(0, std::ifstream::end);
    size_t length = file.tellg();
    file.seekg(0, std::ifstream::beg);

    std::vector<uint8_t> fileData(length);
	file.read(reinterpret_cast<char *>(fileData.data()), length);

	return fileData;
}

void CMainMenuBrowser::OpenHomePage() {
	// platform/mmb_home_page.html still overrides the start page; by default the client opens its
	// built-in Epicentre menu, served by chromehtml.dll from platform/epicentre/menu.
	auto rawHomePageUrl = ReadFile("platform/mmb_home_page.html");
	if(!rawHomePageUrl.size()) {
		const std::string builtinMenu = "ncl://menu/index.html";
		rawHomePageUrl.assign(builtinMenu.begin(), builtinMenu.end());
	}

	InitializeBrowser();

	auto rawErrorPageHtml = ReadFile("platform/mmb_error_page.html");
	if(rawErrorPageHtml.size() > 0) {
		std::string errorPageHtml(rawErrorPageHtml.begin(), rawErrorPageHtml.end());
		nitro_utils::trim(errorPageHtml);
		
		errorPageHtml = std::format(
			"<script>document.body.setAttribute('style', '');document.querySelector('h1').style.display='none'</script>{}", 
			errorPageHtml
		);

		m_pHtml->BrowserErrorStrings("", "", errorPageHtml.c_str(), errorPageHtml.c_str(), 
			errorPageHtml.c_str(), errorPageHtml.c_str(), errorPageHtml.c_str());
	}

	OpenURL(std::string(rawHomePageUrl.begin(), rawHomePageUrl.end()).c_str());
}