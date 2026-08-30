#pragma once

#include <cstdint>

// every address here was read out of gta_sa.exe v1.0 US (14383616 bytes,
// md5 170b3a9108687b26da2d8901c6948a18) and cross checked against gta-reversed
namespace mhs::sa::addr {

using ea = std::uintptr_t;

constexpr ea IsCutsceneSkipButtonBeingPressed = 0x4D5D10;
constexpr ea IsForeground                     = 0x746070;
constexpr ea CRunningScript_Process           = 0x469F00;
constexpr ea CHud_Draw                        = 0x58FAE0;

constexpr ea CSprite2d_Draw2DPolygon = 0x7285B0;
constexpr ea CSprite2d_DrawRect      = 0x727B60;
constexpr ea CSprite2d_SetTexture    = 0x727270;
constexpr ea CSprite2d_Draw_CRect    = 0x728350;

constexpr ea CTxdStore_AddTxdSlot     = 0x731C80;
constexpr ea CTxdStore_LoadTxd        = 0x7320B0;
constexpr ea CTxdStore_AddRef         = 0x731A00;
constexpr ea CTxdStore_PushCurrentTxd = 0x7316A0;
constexpr ea CTxdStore_SetCurrentTxd  = 0x7319C0;
constexpr ea CTxdStore_PopCurrentTxd  = 0x7316B0;

constexpr ea CFont_SetScale              = 0x719380;
constexpr ea CFont_SetColor              = 0x719430;
constexpr ea CFont_SetFontStyle          = 0x719490;
constexpr ea CFont_SetWrapx              = 0x7194D0;
constexpr ea CFont_SetCentreSize         = 0x7194E0;
constexpr ea CFont_SetRightJustifyWrap   = 0x7194F0;
constexpr ea CFont_SetDropColor          = 0x719510;
constexpr ea CFont_SetDropShadowPosition = 0x719570;
constexpr ea CFont_SetEdge               = 0x719590;
constexpr ea CFont_SetProportional       = 0x7195B0;
constexpr ea CFont_SetBackground         = 0x7195C0;
constexpr ea CFont_SetJustify            = 0x719600;
constexpr ea CFont_SetOrientation        = 0x719610;
constexpr ea CFont_GetStringWidth        = 0x71A0E0;
constexpr ea CFont_PrintString           = 0x71A700;

// the call inside Render2dStuff, patched instead of CHud::Draw itself so the
// original stays reachable without a trampoline
constexpr ea Render2dStuff_CallCHudDraw = 0x53E4FF;

constexpr ea CTheScripts_pActiveScripts = 0xA8B42C;
constexpr ea CTheScripts_pIdleScripts   = 0xA8B428;
constexpr ea CTheScripts_ScriptSpace    = 0xA49960;

constexpr ea CCutsceneMgr_ms_running            = 0xB5F851;
constexpr ea CCutsceneMgr_ms_cutsceneProcessing = 0xB5F852;

constexpr ea CPad_NewKeyState = 0xB73190;
constexpr ea CPad_OldKeyState = 0xB72F20;
constexpr ea CPad_Pads        = 0xB73458;

constexpr ea CPad_NewMouseState = 0xB73418;
constexpr ea CPad_OldMouseState = 0xB7342C;

constexpr ea CTimer_FrameCounter          = 0xB7CB4C;
constexpr ea CTimer_TimeInMilliseconds    = 0xB7CB84;

constexpr ea RsGlobal_maximumWidth  = 0xC17044;
constexpr ea RsGlobal_maximumHeight = 0xC17048;

} // namespace mhs::sa::addr
