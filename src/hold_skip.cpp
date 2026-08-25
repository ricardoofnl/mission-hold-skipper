#include "hold_skip.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include <windows.h>

#include "config.hpp"
#include "ginput.hpp"
#include "hold_state.hpp"
#include "log.hpp"
#include "pad_button.hpp"
#include "prompt_device.hpp"
#include "ring.hpp"
#include "sa/game.hpp"

namespace mhs::hold_skip {

namespace {

HoldState     g_state;
FadeAnim      g_fade;
DeviceTracker g_device;

sa::CSprite2d g_icon{};
bool          g_iconTried{false};

bool SceneSkipOffered() {
    if (sa::CutsceneRunning()) {
        return true;
    }
    // guard the walk, a corrupt list must not hang the render thread
    int guard = 0;
    for (auto* script = sa::ActiveScripts(); script && guard < 128; script = script->m_pNext, ++guard) {
        if (script->m_SceneSkipIP != 0) {
            return true;
        }
    }
    return false;
}

bool PadButtonDown() {
    const auto  mask  = Cfg().padMask;
    const auto* state = reinterpret_cast<const std::uint8_t*>(&sa::Pad0().NewState);
    for (std::size_t i = 0; i < kPadButtonCount; ++i) {
        if (!(mask & (1u << i))) {
            continue;
        }
        if (*reinterpret_cast<const std::int16_t*>(state + kPadButtons[i].offset) != 0) {
            return true;
        }
    }
    return false;
}

bool KeyDown() {
    const auto& keys  = sa::NewKeyState();
    const auto& mouse = sa::NewMouseState();
    const auto& cfg   = Cfg();
    return (cfg.keyEnter && keys.enter != 0)
        || (cfg.keyNumpadEnter && keys.extenter != 0)
        || (cfg.keySpace && keys.standardKeys[' '] != 0)
        || (cfg.keyMouseLeft && mouse.lButton)
        || PadButtonDown();
}

bool KeyboardOrMouseActive() {
    const auto& keys = sa::NewKeyState();
    const auto* raw  = reinterpret_cast<const std::int16_t*>(&keys);
    for (std::size_t i = 0; i < sizeof(sa::CKeyboardState) / sizeof(std::int16_t); ++i) {
        if (raw[i] != 0) {
            return true;
        }
    }
    const auto& mouse = sa::NewMouseState();
    return mouse.lButton || mouse.rButton || mouse.mButton
        || std::fabs(mouse.movedX) > 2.0f || std::fabs(mouse.movedY) > 2.0f;
}

bool PadActive(bool keyboardActive) {
    // GInput owns the input layer, its own answer beats any guess we could make
    if (auto* pad = ginput::Pad()) {
        return pad->HasPadInHands();
    }

    const auto& joy      = sa::Pad0().PCTempJoyState;
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
    return sa::Pad0().NewState.ButtonCross != 0 && !keyboardActive;
}

const char* IconTxdPath() {
    const auto& cfg         = Cfg();
    bool        playStation = cfg.iconStyle == IconStyle::PlayStation;
    if (cfg.iconStyle == IconStyle::Auto) {
        playStation = ginput::PlayStationButtons();
    }
    return playStation ? "models\\ps3btns.txd" : "models\\x360btns.txd";
}

void LoadIconOnce() {
    if (g_iconTried) {
        return;
    }
    g_iconTried = true;

    const auto& cfg = Cfg();
    if (cfg.iconMode == IconMode::Off) {
        return;
    }
    const int button = FirstPadButton(cfg.padMask);
    if (button < 0) {
        return;
    }

    const char* path = IconTxdPath();
    if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) {
        MHS_LOG_INFO("no pad icon txd at %s, the prompt stays text only", path);
        return;
    }

    const int slot = sa::txd::AddSlot("mhsbtns");
    if (slot < 0 || !sa::txd::Load(slot, path)) {
        MHS_LOG_WARN("could not load %s", path);
        return;
    }
    sa::txd::AddRef(slot);
    sa::txd::PushCurrent();
    sa::txd::SetCurrent(slot);
    sa::SpriteSetTexture(g_icon, kPadButtons[button].texture.data());
    sa::txd::PopCurrent();

    if (!g_icon.texture) {
        MHS_LOG_WARN("texture '%s' missing from %s", kPadButtons[button].texture.data(), path);
        return;
    }
    MHS_LOG_INFO("pad icon '%s' loaded from %s", kPadButtons[button].texture.data(), path);
}

float RingCenterX() {
    const auto& cfg = Cfg();
    return cfg.ringX < 0.0f ? sa::ScreenWidth() - sa::ScaleX(40.0f) : sa::ScaleX(cfg.ringX);
}

float RingCenterY() {
    const auto& cfg = Cfg();
    return cfg.ringY < 0.0f ? sa::ScreenHeight() - sa::ScaleY(40.0f) : sa::ScaleY(cfg.ringY);
}

void DrawArc(float progress, sa::CRGBA color, float centerX, float centerY,
             float outer, float inner) {
    std::array<ring::Quad, ring::kMaxSegments> quads{};
    const auto count = ring::BuildArc(quads.data(), quads.size(), centerX, centerY,
                                      outer, inner, progress, Cfg().ringSegments);
    for (std::size_t i = 0; i < count; ++i) {
        const auto& q = quads[i];
        sa::Draw2DPolygon(q.x[0], q.y[0], q.x[1], q.y[1], q.x[2], q.y[2], q.x[3], q.y[3], color);
    }
}

sa::CRGBA WithAlphaScale(sa::CRGBA color, float scale) {
    color.a = static_cast<std::uint8_t>(std::clamp(static_cast<float>(color.a) * scale, 0.0f, 255.0f));
    return color;
}

} // namespace

void TickOncePerFrame() {
    static std::uint32_t lastFrame = 0xFFFFFFFFu;
    static std::uint64_t lastTick  = 0;

    const auto frame = sa::FrameCounter();
    if (frame == lastFrame) {
        return;
    }
    lastFrame = frame;

    const auto now = GetTickCount64();
    // a loading stall must not silently fill the whole ring
    const auto delta = lastTick == 0
                           ? 0u
                           : static_cast<std::uint32_t>(std::min<std::uint64_t>(now - lastTick, 250));
    lastTick = now;

    const auto& cfg = Cfg();
    g_state.SetHoldMs(cfg.holdMs);

    const bool available = SceneSkipOffered();
    g_state.Update(available, KeyDown(), delta);

    g_device.SetForced(cfg.promptDevice);
    const bool keyboardActive = KeyboardOrMouseActive();
    g_device.Update(keyboardActive, PadActive(keyboardActive));

    static bool lastPadPrompt = false;
    if (g_device.PadPrompt() != lastPadPrompt) {
        lastPadPrompt = g_device.PadPrompt();
        MHS_LOG_INFO("prompt device is now %s", lastPadPrompt ? "pad" : "keyboard");
    }

    g_fade.Configure(cfg.fadeInMs, cfg.fadeOutMs);
    g_fade.Update(available && (g_state.Holding() || cfg.showHintBeforeHold), delta);

    LoadIconOnce();
}

bool ConsumeCompleted() {
    return g_state.ConsumeCompleted();
}

void Draw() {
    if (!g_fade.Visible()) {
        return;
    }

    const auto&  cfg    = Cfg();
    const float appear  = g_fade.Eased();
    const float progress = g_state.Progress();

    const float scale   = 0.82f + 0.18f * appear;
    const float centerX = RingCenterX();
    const float centerY = RingCenterY() + sa::ScaleY(6.0f) * (1.0f - appear);
    const float outer   = sa::ScaleY(cfg.ringRadius) * scale;
    const float inner   = std::max(0.0f, outer - sa::ScaleY(cfg.ringThickness) * scale);

    static bool logged = false;
    if (!logged) {
        logged = true;
        MHS_LOG_INFO("prompt at %.0f,%.0f radius %.1f on a %.0fx%.0f screen",
                     centerX, centerY, sa::ScaleY(cfg.ringRadius),
                     sa::ScreenWidth(), sa::ScreenHeight());
    }

    // black disc first, an inner radius of 0 makes BuildArc emit a full fan
    if (cfg.colorBackdrop.a != 0) {
        DrawArc(1.0f, WithAlphaScale(cfg.colorBackdrop, appear), centerX, centerY, outer, 0.0f);
    }
    if (cfg.colorTrack.a != 0) {
        DrawArc(1.0f, WithAlphaScale(cfg.colorTrack, appear), centerX, centerY, outer, inner);
    }
    if (progress > 0.0f) {
        DrawArc(progress, WithAlphaScale(cfg.colorProgress, appear), centerX, centerY, outer, inner);
    }

    const auto alpha = static_cast<std::uint8_t>(std::clamp(255.0f * appear, 0.0f, 255.0f));

    // keyboard has no glyph anywhere, not in the game and not in GInput's txd
    if (g_icon.texture && g_device.PadPrompt()) {
        const float half = inner * cfg.iconScale * 0.5f;
        const sa::CRect rect{centerX - half, centerY - half, centerX + half, centerY + half};
        sa::SpriteDraw(g_icon, rect, sa::CRGBA{255, 255, 255, alpha});
    }

    const auto& label = PickLabel(g_device.PadPrompt(), cfg.label, cfg.labelPad);
    if (label.empty()) {
        return;
    }

    sa::font::SetBackground(false, false);
    sa::font::SetProportional(true);
    sa::font::SetFontStyle(sa::FONT_MENU);
    sa::font::SetScale(sa::ScaleX(cfg.labelScaleX), sa::ScaleY(cfg.labelScaleY));
    sa::font::SetJustify(false);
    sa::font::SetOrientation(sa::ALIGN_RIGHT);
    sa::font::SetRightJustifyWrap(0.0f);
    sa::font::SetWrapx(sa::ScreenWidth());
    sa::font::SetDropShadowPosition(1);
    sa::font::SetDropColor(sa::CRGBA{0, 0, 0, alpha});
    sa::font::SetEdge(0);
    sa::font::SetColor(sa::CRGBA{255, 255, 255, alpha});
    // right aligned, so x is where the text ends and it grows towards the left
    sa::font::PrintString(centerX - outer - sa::ScaleX(6.0f),
                          centerY - sa::ScaleY(6.0f),
                          label.c_str());
}

} // namespace mhs::hold_skip
