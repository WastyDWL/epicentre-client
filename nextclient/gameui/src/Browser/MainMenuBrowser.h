#pragma once

#include <GameUi.h>
#include <vgui_controls/HTML.h>
#include <vgui_controls/EditablePanel.h>

class CBobPreviewPanel;

class CMainMenuBrowser : public vgui2::EditablePanel {
public:
	DECLARE_CLASS_SIMPLE(CMainMenuBrowser, EditablePanel);

	CMainMenuBrowser(vgui2::Panel *parent);
	virtual ~CMainMenuBrowser();

	void OpenHomePage();
	void OpenURL(const char *URL);
	void SetFullscreen(bool status);

	// The weapon preview of the options page, drawn over the page in the rectangle it keeps
	// free (browser pixels); w <= 0 hides it. move/demo: PreviewMove/PreviewDemo values.
	void SetGamePreview(int x, int y, int w, int h, int move, int demo);

	// IViewportPanel overrides
	const char* GetName() override {
		return "MainMenuBrowser";
	}

	// VGUI functions:
	vgui2::VPANEL GetVPanel() override final {
		return BaseClass::GetVPanel();
	}

	bool IsVisible() override final {
		return BaseClass::IsVisible();
	}

	void SetParent(vgui2::VPANEL parent) override final {
		BaseClass::SetParent(parent);
	}

private:
	bool m_bRenderFullscreen = false;
	vgui2::HTML* m_pHtml;
	CBobPreviewPanel* m_pPreview = nullptr;

	void SyncPreview();

	virtual void PerformLayout();
	virtual void OnThink();

	void InitializeBrowser();
	std::vector<uint8_t> ReadFile(const std::string& fileName);

	MESSAGE_FUNC_INT_INT( OnScreenSizeChanged, "OnScreenSizeChanged", oldWide, oldTall );
};