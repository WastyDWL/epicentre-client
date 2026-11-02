#include "BuyMenuBrowser.h"

#include <cstdlib>
#include <format>
#include <vgui/ISurfaceNext.h>
#include <goldsrc/client_api.h>

#include "GameUi.h"

namespace
{
	const Color kTransparent(0, 0, 0, 0);
}

CBuyMenuBrowser::CBuyMenuBrowser(vgui2::Panel* parent)
	: BaseClass(parent, "BuyMenuBrowser") {
	// vgui draws the cursor only while a visible popup takes the mouse (like the radar Frame)
	MakePopup(false);
	SetBgColor(kTransparent);
	SetPaintBackgroundEnabled(false);
	SetMouseInputEnabled(true);
	SetKeyBoardInputEnabled(true);

	m_pHtml = new vgui2::HTML(this, "BuyMenuBrowserHTML");
	m_pHtml->InitializeBrowser("Epicentre Buy Menu Browser", true);
	m_pHtml->SetContextMenuEnabled(false);
	// loaded once: the page opens at once from then on and picks the menu up from _epic_buy
	m_pHtml->OpenURL("ncl://menu/buy.html", nullptr);

	SetVisible(false);
}

void CBuyMenuBrowser::RegisterCommands() {
	engine->pfnRegisterVariable("_epic_buy", "0", 0);
	engine->pfnRegisterVariable("_epic_money", "0", 0);
	engine->pfnRegisterVariable("_epic_team", "0", 0);
	engine->pfnAddCommand("epic_buymenu", []() { GameUI().OpenBuyMenu(std::atoi(engine->Cmd_Argv(1))); });
	engine->pfnAddCommand("epic_buymenu_close", []() { GameUI().CloseBuyMenu(); });
}

void CBuyMenuBrowser::Open(int menu) {
	engine->Cvar_SetValue("_epic_buy", static_cast<float>(menu));
	m_pHtml->RunJavascript(std::format("window.EpicBuy&&window.EpicBuy.open({})", menu).c_str());

	InvalidateLayout(true);
	SetVisible(true);
	MoveToFront();
	RequestFocus();
	m_pHtml->RequestFocus();
}

void CBuyMenuBrowser::OnClose() {
	if (!IsVisible())
		return;

	SetVisible(false);
	engine->Cvar_SetValue("_epic_buy", 0.0f);
	m_pHtml->RunJavascript("window.EpicBuy&&window.EpicBuy.close()");
	GameUI().EndPanelOverGame();
}

void CBuyMenuBrowser::OnThink() {
	BaseClass::OnThink();
	SetBgColor(kTransparent);
}

void CBuyMenuBrowser::PerformLayout() {
	BaseClass::PerformLayout();

	int wide, tall;
	vgui2::surface()->GetScreenSize(wide, tall);
	SetBounds(0, 0, wide, tall);
	m_pHtml->SetBounds(0, 0, wide, tall);
}
