#include "game/iii/backend.hpp"

#include <algorithm>
#include <cctype>
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
std::uintptr_t g_hudDrawOriginal{0};

// ms_cutsceneName is a char[8] and the game compares it case insensitively
bool SameName(const char* name, const char* other) {
    for (std::size_t i = 0; i < 8; ++i) {
        const auto a = static_cast<char>(std::tolower(static_cast<unsigned char>(name[i])));
        const auto b = static_cast<char>(std::tolower(static_cast<unsigned char>(other[i])));
        if (a != b) {
            return false;
        }
        if (a == '\0') {
            return true;
        }
    }
    return true;
}

// vanilla would have skipped instantly here, the hold replaces that
void __cdecl HookedFinishCutscene() {
    MHS_LOG_DEBUG("swallowed the vanilla skip");
}

void __cdecl HookedCHudDraw() {
    fn<void(__cdecl*)()>(g_hudDrawOriginal)();

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

std::size_t FindCall(std::uintptr_t from, std::size_t size, std::uintptr_t callee, std::uintptr_t& out) {
    const auto found = callscan::FindCalls(reinterpret_cast<const std::uint8_t*>(from), size, from, callee);
    if (found.count == 1) {
        out = found.at;
    }
    return found.count;
}

void LogBytes(const char* what, std::uintptr_t at) {
    const auto* p = reinterpret_cast<const std::uint8_t*>(at);
    MHS_LOG_ERROR("%s at 0x%08X starts with %02X %02X %02X %02X %02X", what, at, p[0], p[1], p[2], p[3], p[4]);
}

// the prompt can go through either hud draw, whichever call site is still free
bool FindDrawCall(std::uintptr_t code, std::size_t codeSize) {
    struct Candidate {
        const char*    what;
        std::uintptr_t callee;
    };
    const Candidate candidates[]{
        {"CHud::Draw", addr::CHud_Draw},
        {"CHud::DrawAfterFade", addr::CHud_DrawAfterFade},
    };

    for (const auto& candidate : candidates) {
        const auto count = FindCall(code, codeSize, candidate.callee, g_hudDrawCall);
        if (count == 1) {
            g_hudDrawOriginal = candidate.callee;
            MHS_LOG_INFO("call to %s sits at 0x%08X", candidate.what, g_hudDrawCall);
            return true;
        }
        MHS_LOG_WARN("found %u calls to %s, looking for another draw point",
                     static_cast<unsigned>(count), candidate.what);
    }
    for (const auto& candidate : candidates) {
        LogBytes(candidate.what, candidate.callee);
    }
    return false;
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
// the call sites have to be exactly where this table says they are
bool VersionMatches() {
    const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleA(nullptr));
    if (base != 0x400000) {
        MHS_LOG_ERROR("unexpected image base 0x%08X, expected 0x00400000", base);
        return false;
    }

    std::uint32_t imageSize{}, timestamp{};
    if (codeview::Identity(imageSize, timestamp)) {
        MHS_LOG_INFO("host image %u bytes, PE timestamp 0x%08X", imageSize, timestamp);
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

    if (FindCall(update, room, addr::CCutsceneMgr_FinishCutscene, g_finishCall) != 1) {
        MHS_LOG_ERROR("no single call to CCutsceneMgr::FinishCutscene inside CCutsceneMgr::Update");
        LogBytes("CCutsceneMgr::Update", update);
        return false;
    }
    MHS_LOG_INFO("call to CCutsceneMgr::FinishCutscene sits at 0x%08X", g_finishCall);

    return FindDrawCall(code, codeSize);
}

bool InstallHooks() {
    if (!hook::RedirectCall(g_finishCall, &HookedFinishCutscene)) {
        MHS_LOG_ERROR("failed to redirect the CCutsceneMgr::FinishCutscene call");
        return false;
    }
    if (!hook::RedirectCall(g_hudDrawCall, &HookedCHudDraw)) {
        MHS_LOG_ERROR("failed to redirect the hud draw call");
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

// vanilla's own gate, minus its just pressed edge, so the prompt can come up
// before the player touches anything
bool SkipAvailable() {
    if (!CutsceneRunning() || CutsceneLoadStatus() != 0 || ActiveCamMode() != kCamModeFlyBy) {
        return false;
    }
    // the ending cutscene is the one scene the game never offers
    const char* name = CutsceneName();
    if (SameName(name, "end")) {
        return false;
    }
    MHS_LOG_DEBUG("skip offered, scene '%.8s', load status %u, timer %.2f",
                  name, CutsceneLoadStatus(), CutsceneTimer());
    return true;
}

void PerformSkip() {
    FinishCutscene();
}

// the field re3 calls enter is the one in the keypad block, extenter is the
// main return key
bool KeyEnterDown() { return NewKeyState().extenter != 0; }
bool KeyNumpadEnterDown() { return NewKeyState().enter != 0; }
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
    // III and VC print 16 bit characters, and the width table indexes on
    // character minus 32, so anything below a space would read out of bounds
    std::uint16_t wide[96]{};
    std::size_t   length = 0;
    for (std::size_t i = 0; text[i] != '\0' && length + 1 < std::size(wide); ++i) {
        const auto c = static_cast<unsigned char>(text[i]);
        if (c >= ' ' && c < 0x7F) {
            wide[length++] = c;
        }
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
    // the drop alpha is scaled from the colour set last, so set the colour first
    font::SetColor(color);
    font::SetDropColor(drop);
    font::PrintString(x, y, wide);
}

} // namespace mhs::iii
