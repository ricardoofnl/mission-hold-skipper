#pragma once

#include <cstddef>
#include <cstdint>

#include "game/iii/addresses.hpp"

namespace mhs::iii {

template <class T>
inline T& ref(addr::ea address) {
    return *reinterpret_cast<T*>(address);
}

template <class Fn>
inline Fn fn(addr::ea address) {
    return reinterpret_cast<Fn>(address);
}

#pragma pack(push, 1)

struct CRGBA {
    std::uint8_t r{}, g{}, b{}, a{};
};
static_assert(sizeof(CRGBA) == 4);

struct CKeyboardState {
    std::int16_t FKeys[12];
    std::int16_t standardKeys[256];
    std::int16_t esc, insert, del, home, end, pgup, pgdn;
    std::int16_t up, down, left, right;
    std::int16_t scroll, pause, numlock;
    std::int16_t div, mul, sub, add;
    std::int16_t enter, decimal;
    std::int16_t num1, num2, num3, num4, num5, num6, num7, num8, num9, num0;
    std::int16_t back, tab, capslock, extenter;
    std::int16_t lshift, rshift, shift, lctrl, rctrl, lalt, ralt, lwin, rwin, apps;
};
static_assert(sizeof(CKeyboardState) == 0x270);
static_assert(offsetof(CKeyboardState, standardKeys) == 0x18);
static_assert(offsetof(CKeyboardState, enter) == 0x23C);
static_assert(offsetof(CKeyboardState, extenter) == 0x25A);

#pragma pack(pop)

struct CMouseControllerState {
    bool  lButton;
    bool  rButton;
    bool  mButton;
    bool  wheelUp;
    bool  wheelDown;
    bool  x1Button;
    bool  x2Button;
    char  unused;
    float movedX;
    float movedY;
};
static_assert(sizeof(CMouseControllerState) == 0x10);
static_assert(offsetof(CMouseControllerState, movedX) == 8);

struct CRect {
    float left, top, right, bottom;
};

struct CSprite2d {
    void* texture;
};

struct CControllerState {
    std::int16_t LeftStickX, LeftStickY, RightStickX, RightStickY;
    std::int16_t LeftShoulder1, LeftShoulder2, RightShoulder1, RightShoulder2;
    std::int16_t DPadUp, DPadDown, DPadLeft, DPadRight;
    std::int16_t Start, Select;
    std::int16_t ButtonSquare, ButtonTriangle, ButtonCross, ButtonCircle;
    std::int16_t ShockButtonL, ShockButtonR;
    std::int16_t chatIndicated;
};
static_assert(sizeof(CControllerState) == 0x2A);
static_assert(offsetof(CControllerState, ButtonCross) == 0x20);

// prefix only, III keeps no steering buffer between the states, unlike VC
struct CPadPrefix {
    CControllerState NewState;
    CControllerState OldState;
    CControllerState PCTempKeyState;
    CControllerState PCTempJoyState;
    CControllerState PCTempMouseState;
};
static_assert(offsetof(CPadPrefix, PCTempJoyState) == 0x7E);

enum eFontStyle : std::int16_t {
    FONT_BANK,
    FONT_PAGER,
    FONT_HEADING,
};

inline CKeyboardState& NewKeyState() { return ref<CKeyboardState>(addr::CPad_NewKeyState); }
inline CKeyboardState& OldKeyState() { return ref<CKeyboardState>(addr::CPad_OldKeyState); }
inline CMouseControllerState& NewMouseState() { return ref<CMouseControllerState>(addr::CPad_NewMouseState); }
inline CPadPrefix& Pad0() { return ref<CPadPrefix>(addr::CPad_Pads); }

inline bool CutsceneRunning() { return ref<bool>(addr::CCutsceneMgr_ms_running); }
inline bool CutsceneProcessing() { return ref<bool>(addr::CCutsceneMgr_ms_cutsceneProcessing); }
inline std::uint32_t CutsceneLoadStatus() { return ref<std::uint32_t>(addr::CCutsceneMgr_ms_cutsceneLoadStatus); }
inline float CutsceneTimer() { return ref<float>(addr::CCutsceneMgr_ms_cutsceneTimer); }

inline std::uint32_t FrameCounter() { return ref<std::uint32_t>(addr::CTimer_FrameCounter); }

inline float ScreenWidth() { return static_cast<float>(ref<std::int32_t>(addr::RsGlobal_maximumWidth)); }
inline float ScreenHeight() { return static_cast<float>(ref<std::int32_t>(addr::RsGlobal_maximumHeight)); }

// game art is authored for 640x448
inline float ScaleX(float v) { return v * ScreenWidth() / 640.0f; }
inline float ScaleY(float v) { return v * ScreenHeight() / 448.0f; }

inline void CHudDraw() { fn<void(__cdecl*)()>(addr::CHud_Draw)(); }
inline void FinishCutscene() { fn<void(__cdecl*)()>(addr::CCutsceneMgr_FinishCutscene)(); }

// the quad goes into CSprite2d::maVertices, then out as a triangle fan, which is
// what the SA build gets from CSprite2d::Draw2DPolygon
inline void Draw2DPolygon(float x1, float y1, float x2, float y2,
                          float x3, float y3, float x4, float y4, const CRGBA& color) {
    fn<void(__cdecl*)(float, float, float, float, float, float, float, float,
                      const CRGBA&, const CRGBA&, const CRGBA&, const CRGBA&)>(
        addr::CSprite2d_SetVertices)(x1, y1, x2, y2, x3, y3, x4, y4, color, color, color, color);

    constexpr int kTextureRaster = 1;
    constexpr int kTriFan        = 5;
    fn<int(__cdecl*)(int, void*)>(addr::RwRenderStateSet)(kTextureRaster, nullptr);
    fn<int(__cdecl*)(int, void*, int)>(addr::RwIm2DRenderPrimitive)(
        kTriFan, reinterpret_cast<void*>(addr::CSprite2d_maVertices), 4);
}

namespace txd {
inline int AddSlot(const char* name) {
    return fn<int(__cdecl*)(const char*)>(addr::CTxdStore_AddTxdSlot)(name);
}
inline bool Load(int slot, const char* file) {
    return fn<bool(__cdecl*)(int, const char*)>(addr::CTxdStore_LoadTxd)(slot, file);
}
inline void AddRef(int slot) { fn<void(__cdecl*)(int)>(addr::CTxdStore_AddRef)(slot); }
inline void PushCurrent() { fn<void(__cdecl*)()>(addr::CTxdStore_PushCurrentTxd)(); }
inline void SetCurrent(int slot) { fn<void(__cdecl*)(int)>(addr::CTxdStore_SetCurrentTxd)(slot); }
inline void PopCurrent() { fn<void(__cdecl*)()>(addr::CTxdStore_PopCurrentTxd)(); }
} // namespace txd

// resolves the name against the current txd, so wrap it in Push/SetCurrent/Pop
inline void SpriteSetTexture(CSprite2d& sprite, const char* name) {
    fn<void(__thiscall*)(CSprite2d*, const char*)>(addr::CSprite2d_SetTexture)(&sprite, name);
}

inline void SpriteDraw(CSprite2d& sprite, const CRect& rect, const CRGBA& color) {
    fn<void(__thiscall*)(CSprite2d*, const CRect&, const CRGBA&)>(
        addr::CSprite2d_Draw_CRect)(&sprite, rect, color);
}

namespace font {
inline void SetScale(float w, float h) { fn<void(__cdecl*)(float, float)>(addr::CFont_SetScale)(w, h); }
// III takes the colour by reference, SA takes it by value
inline void SetColor(const CRGBA& c) { fn<void(__cdecl*)(const CRGBA*)>(addr::CFont_SetColor)(&c); }
inline void SetFontStyle(std::int16_t s) { fn<void(__cdecl*)(std::int16_t)>(addr::CFont_SetFontStyle)(s); }
inline void SetWrapx(float v) { fn<void(__cdecl*)(float)>(addr::CFont_SetWrapx)(v); }
inline void SetRightJustifyWrap(float v) { fn<void(__cdecl*)(float)>(addr::CFont_SetRightJustifyWrap)(v); }
inline void SetDropColor(const CRGBA& c) { fn<void(__cdecl*)(const CRGBA*)>(addr::CFont_SetDropColor)(&c); }
inline void SetDropShadowPosition(std::int16_t v) {
    fn<void(__cdecl*)(std::int16_t)>(addr::CFont_SetDropShadowPosition)(v);
}
inline void SetPropOn() { fn<void(__cdecl*)()>(addr::CFont_SetPropOn)(); }
inline void SetJustifyOff() { fn<void(__cdecl*)()>(addr::CFont_SetJustifyOff)(); }
inline void SetRightJustifyOn() { fn<void(__cdecl*)()>(addr::CFont_SetRightJustifyOn)(); }
inline void SetBackgroundOff() { fn<void(__cdecl*)()>(addr::CFont_SetBackgroundOff)(); }
inline void PrintString(float x, float y, const std::uint16_t* text) {
    fn<void(__cdecl*)(float, float, const std::uint16_t*)>(addr::CFont_PrintString)(x, y, text);
}
} // namespace font

} // namespace mhs::iii
