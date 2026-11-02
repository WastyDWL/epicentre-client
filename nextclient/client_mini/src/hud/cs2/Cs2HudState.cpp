#include "Cs2HudState.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

#include <data_types/MessageReader.h>
#include <goldsrc/client_api.h>
#include <interface.h>
#include <vgui/IPanel.h>
#include <vgui/ISurfaceNext.h>

#include "main.h"

extern goldsrc::cl_enginefunc_t gEngfuncs;

namespace
{
    Cs2HudState g_State = [] {
        Cs2HudState s;
        s.weapon_ammo_type.fill(-1);
        return s;
    }();

    using MsgFn = int (*)(const char*, int, void*);
    using HookFn = decltype(goldsrc::cl_enginefunc_t::pfnHookUserMsg);

    struct Tap
    {
        const char* name;
        void (*read)(MessageReader& reader);
        MsgFn stock;
    };

    Tap g_Taps[] = {
        {"Battery", [](MessageReader& r) { g_State.armor = r.ReadShort(); }, nullptr},
        {"ArmorType", [](MessageReader& r) { g_State.helmet = r.ReadByte() != 0; }, nullptr},
        {"Money", [](MessageReader& r) {
             g_State.money = r.ReadLong();
             g_State.has_money = true;
         }, nullptr},
        {"WeaponList", [](MessageReader& r) {
             r.ReadString();
             int ammo1 = r.ReadChar();
             r.ReadByte(); // max ammo 1
             r.ReadChar(); // ammo 2
             r.ReadByte(); // max ammo 2
             r.ReadChar(); // slot
             r.ReadChar(); // position
             int id = r.ReadChar();
             if (id > 0 && id < 32)
                 g_State.weapon_ammo_type[id] = static_cast<int8_t>(ammo1);
         }, nullptr},
        {"TeamScore", [](MessageReader& r) {
             std::string team = r.ReadString();
             int score = r.ReadShort();
             if (team == "TERRORIST")
                 g_State.score_t = score;
             else if (team == "CT")
                 g_State.score_ct = score;
         }, nullptr},
        {"RoundTime", [](MessageReader& r) {
             g_State.round_end = static_cast<float>(gEngfuncs.GetClientTime()) + r.ReadShort();
             g_State.bomb_planted = false;
         }, nullptr},
        {"ServerName", [](MessageReader& r) {
             std::string name = r.ReadString();
             std::snprintf(g_State.server_name, sizeof(g_State.server_name), "%s", name.c_str());
         }, nullptr},
        {"Account", [](MessageReader& r) {
             int index = r.ReadByte();
             int money = r.ReadLong();
             if (index > 0 && index <= 32)
             {
                 g_State.account[index] = money;
                 g_State.has_account[index] = true;
             }
         }, nullptr},
    };

    template <int I>
    int Trampoline(const char* name, int size, void* data)
    {
        {
            MessageReader reader(data, size);
            g_Taps[I].read(reader);
        }
        return g_Taps[I].stock ? g_Taps[I].stock(name, size, data) : 1;
    }

    MsgFn g_Trampolines[] = {Trampoline<0>, Trampoline<1>, Trampoline<2>, Trampoline<3>, Trampoline<4>, Trampoline<5>, Trampoline<6>, Trampoline<7>};

    // +showscores / -showscores: client.dll's handlers are kept, ours decide which board opens
    using CmdFn = void (*)();
    using AddCmdFn = decltype(goldsrc::cl_enginefunc_t::pfnAddCommand);
    AddCmdFn g_EngineAddCommand = nullptr;
    int g_ScoreKey = 9; // engine key bound to +showscores: Tab until a key event says otherwise
    CmdFn g_StockShowScores = nullptr;
    CmdFn g_StockHideScores = nullptr;

    void ShowScores()
    {
        g_State.scoreboard_held = true;
        if (!g_State.scoreboard_own && g_StockShowScores)
            g_StockShowScores();
    }

    void HideScores()
    {
        g_State.scoreboard_held = false;
        if (g_StockHideScores)
            g_StockHideScores();
    }

    int AddCommandTap(const char* name, CmdFn fn)
    {
        if (name && std::strcmp(name, "+showscores") == 0)
        {
            g_StockShowScores = fn;
            return g_EngineAddCommand(name, ShowScores);
        }
        if (name && std::strcmp(name, "-showscores") == 0)
        {
            g_StockHideScores = fn;
            return g_EngineAddCommand(name, HideScores);
        }
        return g_EngineAddCommand(name, fn);
    }
    static_assert(std::size(g_Trampolines) == std::size(g_Taps));

    HookFn g_EngineHook = nullptr;

    // While the stock HUD draws, sprites and fills in the stock ammo corner are dropped: hiding the
    // whole ammo element (HIDEHUD_WEAPONS) would also take the weapon selection with it.
    struct StockClip
    {
        bool on = false;
        int min_x = 0;
        int min_y = 0;
    } g_Clip;

    bool Clipped(int x, int y)
    {
        return g_Clip.on && x >= g_Clip.min_x && y >= g_Clip.min_y;
    }

    using SprFn = decltype(goldsrc::cl_enginefunc_t::pfnSPR_Draw);
    using FillFn = decltype(goldsrc::cl_enginefunc_t::pfnFillRGBA);
    SprFn g_SprDraw, g_SprDrawHoles, g_SprDrawAdditive;
    FillFn g_FillRGBA;

    void SprDraw(int frame, int x, int y, const goldsrc::wrect_t* rc)
    {
        if (!Clipped(x, y))
            g_SprDraw(frame, x, y, rc);
    }
    void SprDrawHoles(int frame, int x, int y, const goldsrc::wrect_t* rc)
    {
        if (!Clipped(x, y))
            g_SprDrawHoles(frame, x, y, rc);
    }
    void SprDrawAdditive(int frame, int x, int y, const goldsrc::wrect_t* rc)
    {
        if (!Clipped(x, y))
            g_SprDrawAdditive(frame, x, y, rc);
    }
    void FillRGBA(int x, int y, int w, int h, int r, int g, int b, int a)
    {
        if (!Clipped(x, y))
            g_FillRGBA(x, y, w, h, r, g, b, a);
    }

    int HookUserMsgTap(const char* name, MsgFn fn)
    {
        for (size_t i = 0; i < std::size(g_Taps); i++)
        {
            if (name && std::strcmp(name, g_Taps[i].name) == 0)
            {
                g_Taps[i].stock = fn;
                g_State.taps_active = true;
                return g_EngineHook(name, g_Trampolines[i]);
            }
        }
        return g_EngineHook(name, fn);
    }
} // namespace

Cs2HudState& Cs2State()
{
    return g_State;
}

namespace
{
    vgui2::VPANEL FindVisibleScoreboard(vgui2::IPanel& panels, vgui2::VPANEL panel)
    {
        if (!panel || !panels.IsVisible(panel))
            return 0;
        if (std::strcmp(panels.GetName(panel), "ClientScoreBoard") == 0)
            return panel;
        for (int i = 0, n = panels.GetChildCount(panel); i < n; i++)
        {
            if (vgui2::VPANEL found = FindVisibleScoreboard(panels, panels.GetChild(panel, i)))
                return found;
        }
        return 0;
    }
} // namespace

#ifdef _WIN32
namespace
{
    // Windows virtual key of an engine key number (keydefs.h), for the keys a scoreboard is
    // commonly bound to; 0 when there is no plain equivalent
    int VirtualKey(int key)
    {
        if (key >= 'a' && key <= 'z')
            return key - 'a' + 'A';
        if (key >= '0' && key <= '9')
            return key;
        switch (key)
        {
            case 9: return VK_TAB;
            case 32: return VK_SPACE;
            case '`': return VK_OEM_3;
            case 132: return VK_MENU;    // K_ALT
            case 133: return VK_CONTROL; // K_CTRL
            case 134: return VK_SHIFT;   // K_SHIFT
            default: break;
        }
        if (key >= 135 && key <= 146) // K_F1..K_F12
            return VK_F1 + key - 135;
        return 0;
    }
} // namespace
#endif

void Cs2HudState_SyncStockScoreboard()
{
    if (!g_State.scoreboard_own)
        return;
#ifdef _WIN32
    // a menu holding the keyboard keeps the key release from the HUD: ask the keyboard itself
    int vk = VirtualKey(g_ScoreKey);
    bool key_down = vk && (GetAsyncKeyState(vk) & 0x8000);
    if (vk && !key_down)
        g_State.scoreboard_held = false;

    static vgui2::IPanel* panels = nullptr;
    if (!panels)
    {
        if (CreateInterfaceFn factory = Sys_GetFactory("vgui2.dll"))
            panels = static_cast<vgui2::IPanel*>(factory(VGUI_PANEL_INTERFACE_VERSION_GS, nullptr));
    }
    void** surface = eng()->vgui2_surface;
    if (!panels || !surface || !*surface)
        return;

    vgui2::VPANEL root = static_cast<vgui2::ISurface*>(*surface)->GetEmbeddedPanel();
    for (int i = 0, n = panels->GetChildCount(root); i < n; i++)
    {
        if (vgui2::VPANEL board = FindVisibleScoreboard(*panels, panels->GetChild(root, i)))
        {
            // client.dll opened its board for Tab: ours is shown instead while the key is down
            panels->SetVisible(board, false);
            if (!vk || key_down)
                g_State.scoreboard_held = true;
            return;
        }
    }
#endif
}

void Cs2HudState_ClipStockAmmo(bool on, int screen_w, int screen_h, int font_h)
{
    // CHudAmmo draws its numbers at ScreenHeight - 1.5 * font height in the right half; the pickup
    // history and the weapon menu stay above that line
    g_Clip.on = on && g_SprDraw;
    g_Clip.min_x = screen_w / 2;
    g_Clip.min_y = screen_h - font_h * 8 / 5;
}

void Cs2HudState_Subscribe(nitroapi::NitroApiInterface* nitro_api, std::vector<std::shared_ptr<nitroapi::Unsubscriber>>& unsub)
{
    nitroapi::ClientData* cl = nitro_api->GetClientData();

    // client.dll copies the engine function table here and hooks its messages from CHud::Init
    // with that copy, so the table must carry the tap before the call.
    unsub.emplace_back(cl->CLDLL_Initialize |= [](goldsrc::cl_enginefunc_t* funcs, int version, const auto& next) {
        if (funcs && funcs->pfnHookUserMsg != HookUserMsgTap)
        {
            g_EngineHook = funcs->pfnHookUserMsg;
            funcs->pfnHookUserMsg = HookUserMsgTap;
            g_SprDraw = funcs->pfnSPR_Draw;
            g_SprDrawHoles = funcs->pfnSPR_DrawHoles;
            g_SprDrawAdditive = funcs->pfnSPR_DrawAdditive;
            g_FillRGBA = funcs->pfnFillRGBA;
            funcs->pfnSPR_Draw = SprDraw;
            funcs->pfnSPR_DrawHoles = SprDrawHoles;
            funcs->pfnSPR_DrawAdditive = SprDrawAdditive;
            funcs->pfnFillRGBA = FillRGBA;
            g_EngineAddCommand = funcs->pfnAddCommand;
            funcs->pfnAddCommand = AddCommandTap;
        }
        return next->Invoke(funcs, version);
    });

    unsub.emplace_back(cl->UserMsg_CurWeapon += [](const char*, int size, void* data, int) {
        MessageReader reader(data, size);
        int state = reader.ReadByte();
        int id = reader.ReadChar();
        int clip = reader.ReadChar();
        if (id < 1)
        {
            g_State.weapon = 0;
            g_State.clip = -1;
        }
        else if (state)
        {
            g_State.weapon = id;
            g_State.clip = clip;
        }
    });

    // client.dll's viewport opens its own board straight from the key event too, so the key bound to
    // +showscores is taken here while the CS2 HUD is on and never reaches it
    // Only the release of that key gets here (the viewport takes the press and opens its own board,
    // which Cs2HudState_SyncStockScoreboard then swaps for ours); the release goes on to the game so
    // client.dll also closes its board.
    unsub.emplace_back(cl->HUD_Key_Event |= [](int down, int keynum, const char* binding, const auto& next) {
        if (binding && std::strcmp(binding, "+showscores") == 0)
        {
            g_ScoreKey = keynum;
            if (!down)
                g_State.scoreboard_held = false;
            else if (g_State.scoreboard_own)
                g_State.scoreboard_held = true;
        }
        return next->Invoke(down, keynum, binding);
    });

    unsub.emplace_back(cl->UserMsg_BombDrop +=[](const char*, int size, void* data, int) {
        MessageReader reader(data, size);
        reader.ReadCoord();
        reader.ReadCoord();
        reader.ReadCoord();
        if (reader.ReadByte() == 1)
            g_State.bomb_planted = true;
    });

    unsub.emplace_back(cl->UserMsg_AmmoX += [](const char*, int size, void* data, int) {
        MessageReader reader(data, size);
        int type = reader.ReadByte();
        int count = reader.ReadByte();
        if (type >= 0 && type < 32)
            g_State.ammo[type] = count;
    });
}
