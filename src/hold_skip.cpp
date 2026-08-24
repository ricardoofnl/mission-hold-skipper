#include "hold_skip.hpp"

#include <algorithm>
#include <array>
#include <cstdint>

#include <windows.h>

#include "config.hpp"
#include "hold_state.hpp"
#include "log.hpp"
#include "ring.hpp"
#include "sa/game.hpp"

namespace mhs::hold_skip {

namespace {

HoldState g_state;
FadeAnim  g_fade;

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

bool KeyDown() {
    const auto& keys  = sa::NewKeyState();
    const auto& mouse = sa::NewMouseState();
    const auto& cfg   = Cfg();
    return (cfg.keyEnter && keys.enter != 0)
        || (cfg.keyNumpadEnter && keys.extenter != 0)
        || (cfg.keySpace && keys.standardKeys[' '] != 0)
        || (cfg.keyMouseLeft && mouse.lButton);
}

// bottom right corner
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

    // pop in and slide up a little, standard show and hide feel
    const float scale   = 0.82f + 0.18f * appear;
    const float centerX = RingCenterX();
    const float centerY = RingCenterY() + sa::ScaleY(6.0f) * (1.0f - appear);
    const float outer   = sa::ScaleY(cfg.ringRadius) * scale;
    const float inner   = std::max(0.0f, outer - sa::ScaleY(cfg.ringThickness) * scale);

    // one line so a log tells us where the prompt actually landed
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

    if (cfg.label.empty()) {
        return;
    }

    const auto textAlpha = static_cast<std::uint8_t>(std::clamp(255.0f * appear, 0.0f, 255.0f));

    sa::font::SetBackground(false, false);
    sa::font::SetProportional(true);
    sa::font::SetFontStyle(sa::FONT_MENU);
    sa::font::SetScale(sa::ScaleX(cfg.labelScaleX), sa::ScaleY(cfg.labelScaleY));
    sa::font::SetJustify(false);
    sa::font::SetOrientation(sa::ALIGN_RIGHT);
    sa::font::SetRightJustifyWrap(0.0f);
    sa::font::SetWrapx(sa::ScreenWidth());
    sa::font::SetDropShadowPosition(1);
    sa::font::SetDropColor(sa::CRGBA{0, 0, 0, textAlpha});
    sa::font::SetEdge(0);
    sa::font::SetColor(sa::CRGBA{255, 255, 255, textAlpha});
    // right aligned, so x is where the text ends and it grows towards the left
    sa::font::PrintString(centerX - outer - sa::ScaleX(6.0f),
                          centerY - sa::ScaleY(6.0f),
                          cfg.label.c_str());
}

} // namespace mhs::hold_skip