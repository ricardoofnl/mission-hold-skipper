#pragma once

#include <cstddef>
#include <cstdint>

#include "sa/addresses.hpp"

namespace mhs::sa {

using GxtChar = unsigned char;

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

// stays outside the packed block, the float is aligned to 8 after a pad byte
struct CMouseControllerState {
    bool  lButton;
    bool  rButton;
    bool  mButton;
    bool  wheelUp;
    bool  wheelDown;
    bool  x1Button;
    bool  x2Button;
    float wheelMoved;
    float movedX;
    float movedY;
};
static_assert(sizeof(CMouseControllerState) == 0x14);
static_assert(offsetof(CMouseControllerState, wheelMoved) == 8);

// member order is left, top, right, bottom, unlike the original constructor
struct CRect {
    float left, top, right, bottom;
};
static_assert(sizeof(CRect) == 16);

struct CSprite2d {
    void* texture;
};
static_assert(sizeof(CSprite2d) == 4);

// keyboard, mouse and pad all end up here, which is why reading it covers GInput
struct CControllerState {
    std::int16_t LeftStickX, LeftStickY, RightStickX, RightStickY;
    std::int16_t LeftShoulder1, LeftShoulder2, RightShoulder1, RightShoulder2;
    std::int16_t DPadUp, DPadDown, DPadLeft, DPadRight;
    std::int16_t Start, Select;
    std::int16_t ButtonSquare, ButtonTriangle, ButtonCross, ButtonCircle;
    std::int16_t ShockButtonL, ShockButtonR;
    std::int16_t chatIndicated, pedWalk, vehicleMouseLook, radioTrackSkip;
};
static_assert(sizeof(CControllerState) == 0x30);
static_assert(offsetof(CControllerState, ButtonCross) == 0x20);

// prefix only, the real CPad is 0x134 bytes, so never index this as an array
struct CPadPrefix {
    CControllerState NewState;
    CControllerState OldState;
    std::int16_t     SteeringLeftRightBuffer[10];
    std::int32_t     DrunkDrivingBufferUsed;
    CControllerState PCTempKeyState;
    CControllerState PCTempJoyState;
    CControllerState PCTempMouseState;
};
static_assert(offsetof(CPadPrefix, OldState) == 0x30);
static_assert(offsetof(CPadPrefix, PCTempJoyState) == 0xA8);

struct CRunningScript {
    CRunningScript* m_pNext;
    CRunningScript* m_pPrev;
    char            m_szName[8];
    std::uint8_t*   m_BaseIP;
    std::uint8_t*   m_IP;
    std::uint8_t*   m_IPStack[8];
    std::uint16_t   m_StackDepth;
    std::uint32_t   m_LocalVars[34];
    bool            m_IsActive;
    bool            m_CondResult;
    bool            m_UsesMissionCleanup;
    bool            m_IsExternal;
    bool            m_IsTextBlockOverride;
    std::int8_t     m_ExternalType;
    std::int32_t    m_WakeTime;
    std::uint16_t   m_AndOrState;
    bool            m_NotFlag;
    bool            m_IsDeathArrestCheckEnabled;
    bool            m_DoneDeathArrest;
    std::int32_t    m_SceneSkipIP;
    bool            m_ThisMustBeTheOnlyMissionRunning;
};
static_assert(offsetof(CRunningScript, m_StackDepth) == 0x38);
static_assert(offsetof(CRunningScript, m_UsesMissionCleanup) == 0xC6);
static_assert(offsetof(CRunningScript, m_WakeTime) == 0xCC);
static_assert(offsetof(CRunningScript, m_SceneSkipIP) == 0xD8);
static_assert(offsetof(CRunningScript, m_ThisMustBeTheOnlyMissionRunning) == 0xDC);

enum eFontStyle : std::uint8_t {
    FONT_GOTHIC,
    FONT_SUBTITLES,
    FONT_MENU,
    FONT_PRICEDOWN,
};

enum eFontAlignment : std::int8_t {
    ALIGN_CENTER = 0,
    ALIGN_LEFT   = 1,
    ALIGN_RIGHT  = 2,
};

inline CKeyboardState& NewKeyState() { return ref<CKeyboardState>(addr::CPad_NewKeyState); }
inline CKeyboardState& OldKeyState() { return ref<CKeyboardState>(addr::CPad_OldKeyState); }

inline CMouseControllerState& NewMouseState() { return ref<CMouseControllerState>(addr::CPad_NewMouseState); }
inline CMouseControllerState& OldMouseState() { return ref<CMouseControllerState>(addr::CPad_OldMouseState); }

inline CPadPrefix& Pad0() { return ref<CPadPrefix>(addr::CPad_Pads); }

inline CRunningScript* ActiveScripts() { return ref<CRunningScript*>(addr::CTheScripts_pActiveScripts); }

inline bool CutsceneRunning() {
    return ref<std::int8_t>(addr::CCutsceneMgr_ms_running) != 0
        || ref<bool>(addr::CCutsceneMgr_ms_cutsceneProcessing);
}

inline std::uint32_t FrameCounter() { return ref<std::uint32_t>(addr::CTimer_FrameCounter); }

inline float ScreenWidth() { return static_cast<float>(ref<std::int32_t>(addr::RsGlobal_maximumWidth)); }
inline float ScreenHeight() { return static_cast<float>(ref<std::int32_t>(addr::RsGlobal_maximumHeight)); }

// game art is authored for 640x448
inline float ScaleX(float v) { return v * ScreenWidth() / 640.0f; }
inline float ScaleY(float v) { return v * ScreenHeight() / 448.0f; }

inline bool IsForeground() { return fn<bool(__cdecl*)()>(addr::IsForeground)(); }

inline void CHudDraw() { fn<void(__cdecl*)()>(addr::CHud_Draw)(); }

inline void Draw2DPolygon(float x1, float y1, float x2, float y2,
                          float x3, float y3, float x4, float y4, const CRGBA& color) {
    fn<void(__cdecl*)(float, float, float, float, float, float, float, float, const CRGBA&)>(
        addr::CSprite2d_Draw2DPolygon)(x1, y1, x2, y2, x3, y3, x4, y4, color);
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
inline void SetColor(CRGBA c) { fn<void(__cdecl*)(CRGBA)>(addr::CFont_SetColor)(c); }
inline void SetFontStyle(eFontStyle s) { fn<void(__cdecl*)(eFontStyle)>(addr::CFont_SetFontStyle)(s); }
inline void SetWrapx(float v) { fn<void(__cdecl*)(float)>(addr::CFont_SetWrapx)(v); }
inline void SetCentreSize(float v) { fn<void(__cdecl*)(float)>(addr::CFont_SetCentreSize)(v); }
inline void SetRightJustifyWrap(float v) { fn<void(__cdecl*)(float)>(addr::CFont_SetRightJustifyWrap)(v); }
inline void SetDropColor(CRGBA c) { fn<void(__cdecl*)(CRGBA)>(addr::CFont_SetDropColor)(c); }
inline void SetBackground(bool on, bool enlargeBox) {
    fn<void(__cdecl*)(bool, bool)>(addr::CFont_SetBackground)(on, enlargeBox);
}
inline void SetDropShadowPosition(std::int16_t v) { fn<void(__cdecl*)(std::int16_t)>(addr::CFont_SetDropShadowPosition)(v); }
inline void SetEdge(std::int8_t v) { fn<void(__cdecl*)(std::int8_t)>(addr::CFont_SetEdge)(v); }
inline void SetProportional(bool on) { fn<void(__cdecl*)(bool)>(addr::CFont_SetProportional)(on); }
inline void SetJustify(bool on) { fn<void(__cdecl*)(bool)>(addr::CFont_SetJustify)(on); }
inline void SetOrientation(eFontAlignment a) { fn<void(__cdecl*)(eFontAlignment)>(addr::CFont_SetOrientation)(a); }
inline void PrintString(float x, float y, const char* text) {
    fn<void(__cdecl*)(float, float, const GxtChar*)>(addr::CFont_PrintString)(
        x, y, reinterpret_cast<const GxtChar*>(text));
}
} // namespace font

} // namespace mhs::sa
