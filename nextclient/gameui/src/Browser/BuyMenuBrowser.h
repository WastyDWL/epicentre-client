#pragma once

#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/HTML.h>

// The CS2-style buy menu (ncl://menu/buy.html) shown over the running game in place of client.dll's
// buy panel. client_mini opens it with "epic_buymenu <VGUIMenu id>" when the server sends the buy
// menu; the page buys with the CS 1.6 buy aliases and closes itself with "epic_buymenu_close".
// _epic_buy holds the open menu id (0 closed); client_mini keeps _epic_money / _epic_team current.
class CBuyMenuBrowser : public vgui2::EditablePanel {
public:
	DECLARE_CLASS_SIMPLE(CBuyMenuBrowser, EditablePanel);

	explicit CBuyMenuBrowser(vgui2::Panel* parent);

	void Open(int menu);

	// cvars and commands, registered from CGameUI::Start
	static void RegisterCommands();

private:
	vgui2::HTML* m_pHtml;

	void PerformLayout() override;
	void OnThink() override;

	// sent by CGameUI::ClosePanelOverGame (Esc, loading, disconnect) and by epic_buymenu_close
	MESSAGE_FUNC(OnClose, "Close");
};
