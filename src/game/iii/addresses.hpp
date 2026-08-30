#pragma once

#include <cstdint>

// addresses for gta3.exe v1.0, lifted from plugin-sdk's plugin_III tables and
// cross checked against re3
namespace mhs::iii::addr {

using ea = std::uintptr_t;

constexpr ea CCutsceneMgr_Update         = 0x404EE0;
constexpr ea CCutsceneMgr_FinishCutscene = 0x405140;

constexpr ea CCutsceneMgr_ms_running            = 0x95CCF5;
constexpr ea CCutsceneMgr_ms_cutsceneProcessing = 0x95CD9F;
constexpr ea CCutsceneMgr_ms_cutsceneLoadStatus = 0x95CB40;
constexpr ea CCutsceneMgr_ms_cutsceneTimer      = 0x941548;

constexpr ea CHud_Draw          = 0x5052A0;
constexpr ea CHud_DrawAfterFade = 0x509030;

// III has no standalone Draw2DPolygon, the ring goes through these instead
constexpr ea CSprite2d_SetVertices = 0x51F070;
constexpr ea CSprite2d_maVertices  = 0x6E9168;
constexpr ea RwRenderStateSet      = 0x5A43C0;
constexpr ea RwIm2DRenderPrimitive = 0x5A4430;

constexpr ea CSprite2d_SetTexture = 0x51EA40;
constexpr ea CSprite2d_Draw_CRect = 0x51ED50;

constexpr ea CTxdStore_AddTxdSlot     = 0x5274E0;
constexpr ea CTxdStore_LoadTxd        = 0x5276B0;
constexpr ea CTxdStore_AddRef         = 0x527930;
constexpr ea CTxdStore_PushCurrentTxd = 0x527900;
constexpr ea CTxdStore_SetCurrentTxd  = 0x5278C0;
constexpr ea CTxdStore_PopCurrentTxd  = 0x527910;

constexpr ea CFont_SetScale              = 0x501B80;
constexpr ea CFont_SetColor              = 0x501BD0;
constexpr ea CFont_SetFontStyle          = 0x501DB0;
constexpr ea CFont_SetWrapx              = 0x501CC0;
constexpr ea CFont_SetRightJustifyWrap   = 0x501DC0;
constexpr ea CFont_SetDropColor          = 0x501DE0;
constexpr ea CFont_SetDropShadowPosition = 0x501E70;
constexpr ea CFont_SetPropOn             = 0x501DA0;
constexpr ea CFont_SetJustifyOff         = 0x501C80;
constexpr ea CFont_SetRightJustifyOn     = 0x501D50;
constexpr ea CFont_SetBackgroundOff      = 0x501CF0;
constexpr ea CFont_PrintString           = 0x500F50;

constexpr ea CPad_Pads          = 0x6F0360;
constexpr ea CPad_NewKeyState   = 0x6E60D0;
constexpr ea CPad_OldKeyState   = 0x6F1E70;
constexpr ea CPad_NewMouseState = 0x8809F0;

constexpr ea CTimer_FrameCounter = 0x9412EC;

constexpr ea RsGlobal_maximumWidth  = 0x8F4364;
constexpr ea RsGlobal_maximumHeight = 0x8F4368;

} // namespace mhs::iii::addr
