#pragma once

#include <string>

// window.nextclient methods GameUI owns (server lists, favorites, rules, options dialog, console
// events), registered with chromehtml.dll's NCL_BrowserApi_001. Main thread only.
void BrowserApi_Init();
void BrowserApi_Shutdown();
// Opens an options-dialog tab a trusted page asked for (needs the GameUI frame).
void BrowserApi_RunFrame();
// Forwards console output to pages as nextclient.console 'message' events.
void BrowserApi_ConsolePrint(const std::string& utf8_text);
