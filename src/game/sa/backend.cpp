#include "game/sa/backend.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>

#include <windows.h>

#include "config.hpp"
#include "ginput.hpp"
#include "hold_skip.hpp"
#include "hook.hpp"
#include "log.hpp"

namespace mhs::sa {

namespace {

bool __cdecl HookedIsSkipButtonPressed() {
    hold_skip::TickOncePerFrame();
    // vanilla auto skips while the game is in the background, keep that
    return hold_skip::ConsumeCompleted() || !IsForeground();
}

// another input mod may replace the same function, and the last one to load wins
void VerifySkipHook() {
    const auto* at = reinterpret_cast<const std::uint8_t*>(addr::IsCutsceneSkipButtonBeingPressed);
    if (at[0] == 0xE9) {
        std::int32_t relative{};
        std::memcpy(&relative, at + 1, sizeof(relative));
        const auto target = addr::IsCutsceneSkipButtonBeingPressed + 5 + static_cast<std::uintptr_t>(relative);
        if (target == reinterpret_cast<std::uintptr_t>(&HookedIsSkipButtonPressed)) {
            return;
        }
    }
    MHS_LOG_WARN("something replaced the skip button hook, hold to skip is inactive");
}

void __cdecl HookedCHudDraw() {
    CHudDraw();

    // by the first frame every other .asi has had its turn at patching, and
    // GInput, which must never be touched from DllMain, is up
    static bool verified = false;
    if (!verified) {
        verified = true;
        MHS_LOG_INFO("first frame through the draw hook");
        VerifySkipHook();
        if (auto* pad = ginput::Pad()) {
            MHS_LOG_INFO("GInput API in use, version 0x%06X, pad connected %d",
                         pad->GetVersion(), pad->IsPadConnected() ? 1 : 0);
        }
    }

    hold_skip::TickOncePerFrame();
    hold_skip::Draw();
}

bool Readable(std::uintptr_t at, std::size_t size) {
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(reinterpret_cast<const void*>(at), &info, sizeof(info)) != sizeof(info)) {
        return false;
    }
    if (info.State != MEM_COMMIT) {
        return false;
    }
    const auto end = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
    return at + size <= end;
}

struct Signature {
    const char*         what;
    std::uintptr_t      at;
    const std::uint8_t* bytes;
    std::size_t         size;
};

} // namespace

bool VersionMatches() {
    const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleA(nullptr));
    if (base != 0x400000) {
        MHS_LOG_ERROR("unexpected image base 0x%08X, expected 0x00400000", base);
        return false;
    }

    static const std::uint8_t kSkipButton[]{0x6A, 0x00, 0xE8, 0x59, 0x9E, 0x06, 0x00, 0x83};
    static const std::uint8_t kScriptProcess[]{0x53, 0x56, 0x8B, 0xF1, 0x8B, 0x86, 0xD8, 0x00};
    static const std::uint8_t kHudDraw[]{0x80, 0x3D, 0x88, 0x30, 0xA4, 0x00, 0x01, 0x0F};
    static const std::uint8_t kHudDrawCall[]{0xE8, 0xDC, 0x15, 0x05, 0x00};

    const Signature signatures[]{
        {"IsCutsceneSkipButtonBeingPressed", addr::IsCutsceneSkipButtonBeingPressed, kSkipButton, sizeof(kSkipButton)},
        {"CRunningScript::Process", addr::CRunningScript_Process, kScriptProcess, sizeof(kScriptProcess)},
        {"CHud::Draw", addr::CHud_Draw, kHudDraw, sizeof(kHudDraw)},
        {"Render2dStuff call to CHud::Draw", addr::Render2dStuff_CallCHudDraw, kHudDrawCall, sizeof(kHudDrawCall)},
    };

    for (const auto& signature : signatures) {
        if (!Readable(signature.at, signature.size)) {
            MHS_LOG_ERROR("cannot read %s at 0x%08X", signature.what, signature.at);
            return false;
        }
        if (!hook::BytesMatch(signature.at, signature.bytes, signature.size)) {
            MHS_LOG_ERROR("byte mismatch for %s at 0x%08X", signature.what, signature.at);
            return false;
        }
    }
    return true;
}

bool InstallHooks() {
    if (!hook::MakeJmp(addr::IsCutsceneSkipButtonBeingPressed, &HookedIsSkipButtonPressed)) {
        MHS_LOG_ERROR("failed to hook IsCutsceneSkipButtonBeingPressed");
        return false;
    }
    if (!hook::RedirectCall(addr::Render2dStuff_CallCHudDraw, &HookedCHudDraw)) {
        MHS_LOG_ERROR("failed to redirect the CHud::Draw call");
        return false;
    }
    return true;
}

bool SkipAvailable() {
    if (CutsceneRunning()) {
        return true;
    }
    // guard the walk, a corrupt list must not hang the render thread
    int guard = 0;
    for (auto* script = ActiveScripts(); script && guard < 128; script = script->m_pNext, ++guard) {
        if (script->m_SceneSkipIP != 0) {
            return true;
        }
    }
    return false;
}

bool KeyEnterDown() { return NewKeyState().enter != 0; }
bool KeyNumpadEnterDown() { return NewKeyState().extenter != 0; }
bool KeySpaceDown() { return NewKeyState().standardKeys[' '] != 0; }
bool MouseLeftDown() { return NewMouseState().lButton; }

bool PadButtonDown(std::size_t stateOffset) {
    const auto* state = reinterpret_cast<const std::uint8_t*>(&Pad0().NewState);
    return *reinterpret_cast<const std::int16_t*>(state + stateOffset) != 0;
}

bool KeyboardOrMouseActive() {
    const auto& keys = NewKeyState();
    const auto* raw  = reinterpret_cast<const std::int16_t*>(&keys);
    for (std::size_t i = 0; i < sizeof(CKeyboardState) / sizeof(std::int16_t); ++i) {
        if (raw[i] != 0) {
            return true;
        }
    }
    const auto& mouse = NewMouseState();
    return mouse.lButton || mouse.rButton || mouse.mButton
        || std::fabs(mouse.movedX) > 2.0f || std::fabs(mouse.movedY) > 2.0f;
}

bool PadActive(bool keyboardActive) {
    // GInput owns the input layer, its own answer beats any guess we could make
    if (auto* pad = ginput::Pad()) {
        return pad->HasPadInHands();
    }

    const auto& joy      = Pad0().PCTempJoyState;
    const auto  pushed   = [](std::int16_t v) { return v > 48 || v < -48; };
    const bool  onJoyPad = joy.ButtonCross || joy.ButtonCircle || joy.ButtonSquare
        || joy.ButtonTriangle || joy.Start || joy.Select
        || joy.LeftShoulder1 || joy.LeftShoulder2 || joy.RightShoulder1 || joy.RightShoulder2
        || joy.DPadUp || joy.DPadDown || joy.DPadLeft || joy.DPadRight
        || pushed(joy.LeftStickX) || pushed(joy.LeftStickY)
        || pushed(joy.RightStickX) || pushed(joy.RightStickY);
    if (onJoyPad) {
        return true;
    }
    return Pad0().NewState.ButtonCross != 0 && !keyboardActive;
}

void DrawLabel(float x, float y, const char* text, CRGBA color, CRGBA drop,
               float scaleX, float scaleY, float wrapAt, int fontStyle) {
    font::SetBackground(false, false);
    font::SetProportional(true);
    font::SetFontStyle(fontStyle < 0 ? FONT_MENU : static_cast<eFontStyle>(fontStyle));
    font::SetScale(scaleX, scaleY);
    font::SetJustify(false);
    font::SetOrientation(ALIGN_RIGHT);
    font::SetRightJustifyWrap(0.0f);
    font::SetWrapx(wrapAt);
    font::SetDropShadowPosition(1);
    font::SetDropColor(drop);
    font::SetEdge(0);
    font::SetColor(color);
    font::PrintString(x, y, text);
}

} // namespace mhs::sa
