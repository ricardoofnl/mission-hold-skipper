#include "hold_skip.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include <windows.h>

#include "config.hpp"
#include "game/bindings.hpp"
#include "ginput.hpp"
#include "hold_state.hpp"
#include "log.hpp"
#include "pad_button.hpp"
#include "prompt_device.hpp"
#include "ring.hpp"

namespace mhs::hold_skip {

namespace {

HoldState     g_state;
FadeAnim      g_fade;
DeviceTracker g_device;

game::CSprite2d g_icon{};
bool          g_iconTried{false};

bool PadButtonDown() {
    const auto mask = Cfg().padMask;
    for (std::size_t i = 0; i < kPadButtonCount; ++i) {
        if (!(mask & (1u << i))) {
            continue;
        }
        if (game::PadButtonDown(kPadButtons[i].offset)) {
            return true;
        }
    }
    return false;
}

bool KeyDown() {
    const auto& cfg = Cfg();
    return (cfg.keyEnter && game::KeyEnterDown())
        || (cfg.keyNumpadEnter && game::KeyNumpadEnterDown())
        || (cfg.keySpace && game::KeySpaceDown())
        || (cfg.keyMouseLeft && game::MouseLeftDown())
        || PadButtonDown();
}

const char* IconTxdPath() {
    const auto& cfg         = Cfg();
    bool        playStation = cfg.iconStyle == IconStyle::PlayStation;
    if (cfg.iconStyle == IconStyle::Auto) {
        playStation = ginput::PlayStationButtons();
    }
    return playStation ? "models\\ps3btns.txd" : "models\\x360btns.txd";
}

bool LoadIconTxd(const char* path, int button) {
    const int slot = game::txd::AddSlot("mhsbtns");
    if (slot < 0 || !game::txd::Load(slot, path)) {
        return false;
    }
    game::txd::AddRef(slot);
    game::txd::PushCurrent();
    game::txd::SetCurrent(slot);
    game::SpriteSetTexture(g_icon, kPadButtons[button].texture.data());
    game::txd::PopCurrent();
    return g_icon.texture != nullptr;
}

// CTxdStore runs on the render thread here, a fault inside it must not take the
// game down with us
bool LoadIconGuarded(const char* path, int button) {
    __try {
        return LoadIconTxd(path, button);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// deferred until the pad prompt is actually on screen, loading a txd while the
// game is still starting up crashes GTA III
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
    // III's LoadTxd retries RwStreamOpen forever, so a missing file would hang
    if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) {
        MHS_LOG_INFO("no pad icon txd at %s, the prompt stays text only", path);
        return;
    }

    if (!LoadIconGuarded(path, button)) {
        MHS_LOG_WARN("could not take '%s' from %s, the prompt stays text only",
                     kPadButtons[button].texture.data(), path);
        return;
    }
    MHS_LOG_INFO("pad icon '%s' loaded from %s", kPadButtons[button].texture.data(), path);
}

float RingCenterX() {
    const auto& cfg = Cfg();
    return cfg.ringX < 0.0f ? game::ScreenWidth() - game::ScaleX(40.0f) : game::ScaleX(cfg.ringX);
}

float RingCenterY() {
    const auto& cfg = Cfg();
    return cfg.ringY < 0.0f ? game::ScreenHeight() - game::ScaleY(40.0f) : game::ScaleY(cfg.ringY);
}

void DrawArc(float progress, game::CRGBA color, float centerX, float centerY,
             float outer, float inner) {
    std::array<ring::Quad, ring::kMaxSegments> quads{};
    const auto count = ring::BuildArc(quads.data(), quads.size(), centerX, centerY,
                                      outer, inner, progress, Cfg().ringSegments);
    for (std::size_t i = 0; i < count; ++i) {
        const auto& q = quads[i];
        game::Draw2DPolygon(q.x[0], q.y[0], q.x[1], q.y[1], q.x[2], q.y[2], q.x[3], q.y[3], color);
    }
}

game::CRGBA WithAlphaScale(game::CRGBA color, float scale) {
    color.a = static_cast<std::uint8_t>(std::clamp(static_cast<float>(color.a) * scale, 0.0f, 255.0f));
    return color;
}

} // namespace

void TickOncePerFrame() {
    static std::uint32_t lastFrame = 0xFFFFFFFFu;
    static std::uint64_t lastTick  = 0;

    const auto frame = game::FrameCounter();
    if constexpr (game::kNeedsFrameGuard) {
        if (frame == lastFrame) {
            return;
        }
    }
    lastFrame = frame;

    const auto now     = GetTickCount64();
    const auto elapsed = lastTick == 0 ? 0ull : now - lastTick;
    lastTick           = now;

    // a load, a pause or a stall is not held time, it throws the hold away
    const bool stalled = elapsed > 250;
    const auto delta   = stalled ? 0u : static_cast<std::uint32_t>(elapsed);
    if (stalled) {
        g_state.Interrupt();
    }

    const auto& cfg = Cfg();
    g_state.SetHoldMs(cfg.holdMs);

    const bool available = game::SkipAvailable();
    const bool keyDown   = KeyDown();

    static bool lastAvailable = false;
    if (available != lastAvailable) {
        lastAvailable = available;
        MHS_LOG_INFO("the game %s a skip", available ? "offers" : "no longer offers");
    }
    if (available) {
        MHS_LOG_DEBUG("frame %u delta %u progress %.0f%% enter %d numpad %d space %d lmb %d pad %d",
                      frame, delta, g_state.Progress() * 100.0f,
                      game::KeyEnterDown() ? 1 : 0, game::KeyNumpadEnterDown() ? 1 : 0,
                      game::KeySpaceDown() ? 1 : 0, game::MouseLeftDown() ? 1 : 0,
                      PadButtonDown() ? 1 : 0);
    }

    g_state.Update(available, keyDown, delta);

    // SA's own hook consumes the completion, III and VC need us to do the skip
    if constexpr (game::kSkipIsExplicit) {
        if (g_state.ConsumeCompleted()) {
            MHS_LOG_INFO("hold complete, skipping the scene");
            game::PerformSkip();
        }
    }

    g_device.SetForced(cfg.promptDevice);
    const bool keyboardActive = game::KeyboardOrMouseActive();
    g_device.Update(keyboardActive, game::PadActive(keyboardActive));

    static bool lastPadPrompt = false;
    if (g_device.PadPrompt() != lastPadPrompt) {
        lastPadPrompt = g_device.PadPrompt();
        MHS_LOG_INFO("prompt device is now %s", lastPadPrompt ? "pad" : "keyboard");
    }

    g_fade.Configure(cfg.fadeInMs, cfg.fadeOutMs);
    g_fade.Update(available && (g_state.Holding() || cfg.showHintBeforeHold), delta);
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
    const float centerY = RingCenterY() + game::ScaleY(6.0f) * (1.0f - appear);
    const float outer   = game::ScaleY(cfg.ringRadius) * scale;
    const float inner   = std::max(0.0f, outer - game::ScaleY(cfg.ringThickness) * scale);

    static bool logged = false;
    if (!logged) {
        logged = true;
        MHS_LOG_INFO("prompt at %.0f,%.0f radius %.1f on a %.0fx%.0f screen",
                     centerX, centerY, game::ScaleY(cfg.ringRadius),
                     game::ScreenWidth(), game::ScreenHeight());
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
    if (g_device.PadPrompt()) {
        LoadIconOnce();
    }
    if (g_icon.texture && g_device.PadPrompt()) {
        const float half = inner * cfg.iconScale * 0.5f;
        const game::CRect rect{centerX - half, centerY - half, centerX + half, centerY + half};
        game::SpriteDraw(g_icon, rect, game::CRGBA{255, 255, 255, alpha});
    }

    const auto& label = PickLabel(g_device.PadPrompt(), cfg.label, cfg.labelPad);
    if (label.empty()) {
        return;
    }

    // right aligned, so x is where the text ends and it grows towards the left
    game::DrawLabel(centerX - outer - game::ScaleX(6.0f),
                    centerY - game::ScaleY(6.0f),
                    label.c_str(),
                    game::CRGBA{255, 255, 255, alpha},
                    game::CRGBA{0, 0, 0, alpha},
                    game::ScaleX(cfg.labelScaleX),
                    game::ScaleY(cfg.labelScaleY),
                    game::ScreenWidth(),
                    cfg.fontStyle);
}

} // namespace mhs::hold_skip
