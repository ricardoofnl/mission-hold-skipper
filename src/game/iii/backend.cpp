#include "game/iii/backend.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iterator>

#include <windows.h>

#include "game/callscan.hpp"
#include "game/codeview.hpp"
#include "ginput.hpp"
#include "hold_skip.hpp"
#include "hook.hpp"
#include "log.hpp"

namespace mhs::iii {

namespace {

std::uintptr_t g_finishCall{0};
std::uintptr_t g_hudDrawCall{0};
bool           g_skipOffered{false};

// vanilla reached its own skip check, so the scene is skippable and a skip key
// just went down, which is where the hold takes over
void __cdecl HookedFinishCutscene() {
    g_skipOffered = true;
}

void __cdecl HookedCHudDraw() {
    CHudDraw();

    static bool logged = false;
    if (!logged) {
        logged = true;
        MHS_LOG_INFO("first frame through the draw hook");
        if (auto* pad = ginput::Pad()) {
            MHS_LOG_INFO("GInput API in use, version 0x%06X, pad connected %d",
                         pad->GetVersion(), pad->IsPadConnected() ? 1 : 0);
        }
    }

    hold_skip::TickOncePerFrame();
    hold_skip::Draw();
}

bool FindCall(const char* what, std::uintptr_t from, std::size_t size,
              std::uintptr_t callee, std::uintptr_t& out) {
    const auto found = callscan::FindCalls(reinterpret_cast<const std::uint8_t*>(from), size, from, callee);
    if (found.count != 1) {
        MHS_LOG_ERROR("found %u calls to %s, expected exactly one", static_cast<unsigned>(found.count), what);
        return false;
    }
    out = found.at;
    MHS_LOG_INFO("call to %s sits at 0x%08X", what, out);
    return true;
}

bool CallPointsAt(std::uintptr_t at, const void* target) {
    const auto* site = reinterpret_cast<const std::uint8_t*>(at);
    if (site[0] != 0xE8) {
        return false;
    }
    std::int32_t relative{};
    std::memcpy(&relative, site + 1, sizeof(relative));
    return at + 5 + static_cast<std::uintptr_t>(relative) == reinterpret_cast<std::uintptr_t>(target);
}

} // namespace

// without gta3.exe byte signatures the table is checked structurally instead,
// both call sites have to be exactly where this table says they are
bool VersionMatches() {
    const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleA(nullptr));
    if (base != 0x400000) {
        MHS_LOG_ERROR("unexpected image base 0x%08X, expected 0x00400000", base);
        return false;
    }

    std::uintptr_t code{};
    std::size_t    codeSize{};
    if (!codeview::Code(code, codeSize)) {
        MHS_LOG_ERROR("cannot find the code section");
        return false;
    }

    const auto update = addr::CCutsceneMgr_Update;
    if (update < code || update >= code + codeSize) {
        MHS_LOG_ERROR("CCutsceneMgr::Update at 0x%08X is outside the code section", update);
        return false;
    }
    const auto room = std::min<std::size_t>(0x400, code + codeSize - update);

    return FindCall("CHud::Draw", code, codeSize, addr::CHud_Draw, g_hudDrawCall)
        && FindCall("CCutsceneMgr::FinishCutscene", update, room, addr::CCutsceneMgr_FinishCutscene,
                    g_finishCall);
}

bool InstallHooks() {
    if (!hook::RedirectCall(g_finishCall, &HookedFinishCutscene)) {
        MHS_LOG_ERROR("failed to redirect the CCutsceneMgr::FinishCutscene call");
        return false;
    }
    if (!hook::RedirectCall(g_hudDrawCall, &HookedCHudDraw)) {
        MHS_LOG_ERROR("failed to redirect the CHud::Draw call");
        return false;
    }
    // another mod can repoint the same two calls, and the last one to load wins
    if (!CallPointsAt(g_finishCall, &HookedFinishCutscene)
        || !CallPointsAt(g_hudDrawCall, &HookedCHudDraw)) {
        MHS_LOG_WARN("a call site does not point at the plugin, hold to skip is inactive");
        return false;
    }
    return true;
}

bool SkipAvailable() {
    if (!CutsceneRunning()) {
        g_skipOffered = false;
        return false;
    }
    return g_skipOffered;
}

void PerformSkip() {
    g_skipOffered = false;
    FinishCutscene();
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
    // III and VC print 16 bit characters, unlike SA
    std::uint16_t wide[96]{};
    std::size_t   length = 0;
    while (text[length] != '\0' && length + 1 < std::size(wide)) {
        wide[length] = static_cast<std::uint16_t>(static_cast<unsigned char>(text[length]));
        ++length;
    }

    font::SetBackgroundOff();
    font::SetPropOn();
    font::SetFontStyle(fontStyle < 0 ? FONT_HEADING : static_cast<std::int16_t>(fontStyle));
    font::SetScale(scaleX, scaleY);
    font::SetJustifyOff();
    font::SetRightJustifyOn();
    font::SetRightJustifyWrap(0.0f);
    font::SetWrapx(wrapAt);
    font::SetDropShadowPosition(1);
    font::SetDropColor(drop);
    font::SetColor(color);
    font::PrintString(x, y, wide);
}

} // namespace mhs::iii
