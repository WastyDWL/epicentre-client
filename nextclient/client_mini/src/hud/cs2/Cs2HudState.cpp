#include "Cs2HudState.h"

#include <cstring>
#include <string>

#include <data_types/MessageReader.h>
#include <goldsrc/client_api.h>

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

    MsgFn g_Trampolines[] = {Trampoline<0>, Trampoline<1>, Trampoline<2>, Trampoline<3>, Trampoline<4>, Trampoline<5>};
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

    unsub.emplace_back(cl->UserMsg_BombDrop += [](const char*, int size, void* data, int) {
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
