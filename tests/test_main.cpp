#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "game/callscan.hpp"
#include "hold_state.hpp"
#include "pad_button.hpp"
#include "prompt_device.hpp"
#include "ring.hpp"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
    if (!condition) {
        std::printf("FAIL %s\n", what);
        ++g_failures;
    }
}

void TestArcCounts() {
    std::vector<mhs::ring::Quad> quads(mhs::ring::kMaxSegments);

    Check(mhs::ring::BuildArc(quads.data(), quads.size(), 0, 0, 20, 16, 0.0f, 64) == 0,
          "zero progress emits nothing");
    Check(mhs::ring::BuildArc(quads.data(), quads.size(), 0, 0, 20, 16, 1.0f, 64) == 64,
          "full progress emits every segment");
    Check(mhs::ring::BuildArc(quads.data(), quads.size(), 0, 0, 20, 16, 0.5f, 64) == 32,
          "half progress emits half the segments");
    Check(mhs::ring::BuildArc(quads.data(), quads.size(), 0, 0, 20, 16, 2.0f, 64) == 64,
          "progress above one is clamped");
    Check(mhs::ring::BuildArc(quads.data(), quads.size(), 0, 0, 16, 20, 1.0f, 64) == 0,
          "inverted radii emit nothing");
    Check(mhs::ring::ClampSegments(2) == mhs::ring::kMinSegments, "segment count has a floor");
    Check(mhs::ring::ClampSegments(9999) == mhs::ring::kMaxSegments, "segment count has a ceiling");
}

void TestArcGeometry() {
    std::vector<mhs::ring::Quad> quads(mhs::ring::kMaxSegments);
    const float                 cx = 100.0f, cy = 200.0f, outer = 20.0f, inner = 16.0f;

    const auto count = mhs::ring::BuildArc(quads.data(), quads.size(), cx, cy, outer, inner, 1.0f, 64);
    Check(count == 64, "geometry test got its segments");

    Check(std::fabs(quads[0].x[1] - cx) < 0.001f, "arc starts on the vertical axis");
    Check(quads[0].y[1] < cy, "arc starts above the centre");

    for (std::size_t i = 0; i < count; ++i) {
        for (int v = 0; v < 4; ++v) {
            const float dx     = quads[i].x[v] - cx;
            const float dy     = quads[i].y[v] - cy;
            const float radius = std::sqrt(dx * dx + dy * dy);
            Check(radius > inner - 0.01f && radius < outer + 0.01f, "vertex sits inside the annulus");
        }
    }
}

void TestHoldState() {
    mhs::HoldState state;
    state.SetHoldMs(1000);

    state.Update(false, true, 5000);
    Check(!state.ConsumeCompleted(), "no completion while nothing can be skipped");
    Check(state.Progress() == 0.0f, "progress stays empty while unavailable");

    state.Update(true, true, 400);
    Check(!state.ConsumeCompleted(), "no completion before the hold is long enough");
    Check(std::fabs(state.Progress() - 0.4f) < 0.001f, "progress tracks the held time");

    state.Update(true, true, 700);
    Check(state.ConsumeCompleted(), "completion fires once the hold is long enough");
    Check(!state.ConsumeCompleted(), "completion is consumed exactly once");

    state.Update(true, true, 700);
    Check(!state.ConsumeCompleted(), "holding on does not fire again");

    state.Update(true, false, 16);
    Check(state.Progress() == 0.0f, "releasing resets progress");
    state.Update(true, true, 1500);
    Check(state.ConsumeCompleted(), "a fresh hold can fire again");

    mhs::HoldState late;
    late.SetHoldMs(100);
    late.Update(true, true, 200);
    late.Update(true, false, 16);
    Check(!late.ConsumeCompleted(), "release drops an unconsumed completion");
}

void TestFadeAnim() {
    mhs::FadeAnim fade;
    fade.Configure(100, 200);

    Check(!fade.Visible(), "starts hidden");

    fade.Update(true, 50);
    Check(fade.Visible(), "becomes visible while showing");
    Check(std::fabs(fade.Value() - 0.5f) < 0.001f, "fade in follows the configured duration");
    Check(fade.Eased() > fade.Value(), "ease out runs ahead of the linear value");

    fade.Update(true, 100);
    Check(fade.Value() == 1.0f, "fade in stops at one");
    Check(std::fabs(fade.Eased() - 1.0f) < 0.001f, "eased value tops out too");

    fade.Update(false, 100);
    Check(std::fabs(fade.Value() - 0.5f) < 0.001f, "fade out follows its own duration");
    Check(fade.Visible(), "still drawn while fading out");

    fade.Update(false, 500);
    Check(fade.Value() == 0.0f, "fade out stops at zero");
    Check(!fade.Visible(), "hidden once the fade out finished");
}

void TestPromptDevice() {
    mhs::DeviceTracker tracker;
    Check(!tracker.PadPrompt(), "starts on the keyboard prompt");

    tracker.Update(false, true);
    Check(tracker.PadPrompt(), "pad input switches the prompt");

    tracker.Update(false, false);
    Check(tracker.PadPrompt(), "no input keeps the last device");

    tracker.Update(true, false);
    Check(!tracker.PadPrompt(), "keyboard input switches back");

    tracker.Update(true, true);
    Check(!tracker.PadPrompt(), "keyboard wins when both report input");

    tracker.Update(false, true);
    tracker.SetForced(mhs::PromptDevice::Keyboard);
    Check(!tracker.PadPrompt(), "forcing keyboard overrides detection");
    tracker.SetForced(mhs::PromptDevice::Pad);
    Check(tracker.PadPrompt(), "forcing pad overrides detection");
    tracker.SetForced(mhs::PromptDevice::Auto);
    Check(tracker.PadPrompt(), "auto returns to what was detected");

    const std::string keyboard = "HOLD ENTER TO SKIP";
    const std::string pad      = "HOLD ~x~ TO SKIP";
    const std::string empty;
    Check(mhs::PickLabel(true, keyboard, pad) == pad, "pad prompt picks the pad label");
    Check(mhs::PickLabel(false, keyboard, pad) == keyboard, "keyboard prompt picks the keyboard label");
    Check(mhs::PickLabel(true, keyboard, empty) == keyboard, "an empty pad label falls back");
}

void TestPadButtons() {
    Check(mhs::FindPadButton("PAD_CROSS") == 0, "the first token is Cross");
    Check(mhs::FindPadButton("PAD_TRIANGLE") >= 0, "Triangle is a known token");
    Check(mhs::FindPadButton("PAD_NOPE") == -1, "an unknown token is rejected");
    Check(mhs::FindPadButton("pad_cross") == -1, "tokens are matched after upper casing");

    const auto cross = mhs::kPadButtons[mhs::FindPadButton("PAD_CROSS")];
    Check(cross.offset == 0x20, "Cross sits at 0x20");
    Check(cross.texture == "cross", "Cross maps to the cross texture");
    const auto square = mhs::kPadButtons[mhs::FindPadButton("PAD_SQUARE")];
    Check(square.offset == 0x1C, "Square sits at 0x1C");
    const auto r2 = mhs::kPadButtons[mhs::FindPadButton("PAD_R2")];
    Check(r2.offset == 0x0E, "R2 sits at 0x0E");

    for (std::size_t i = 0; i < mhs::kPadButtonCount; ++i) {
        Check(mhs::kPadButtons[i].offset % 2 == 0, "every offset lands on an int16");
        Check(mhs::kPadButtons[i].offset < 0x30, "every offset stays inside CControllerState");
    }

    Check(mhs::FirstPadButton(0) == -1, "no enabled button means no icon");
    Check(mhs::FirstPadButton(1u << 3) == 3, "a single button is picked");
    Check(mhs::FirstPadButton((1u << 3) | (1u << 1)) == 1, "the lowest enabled button wins");
}

void TestCallScan() {
    constexpr std::uintptr_t kCode   = 0x404EE0;
    constexpr std::uintptr_t kCallee = 0x405140;

    // call rel32 at kCode + 3, so rel32 = kCallee - (kCode + 3 + 5) = 0x258
    std::vector<std::uint8_t> code{0x53, 0x56, 0x8B, 0xE8, 0x58, 0x02, 0x00, 0x00, 0x5E, 0xC3};

    auto found = mhs::callscan::FindCalls(code.data(), code.size(), kCode, kCallee);
    Check(found.count == 1, "one matching call is found");
    Check(found.at == kCode + 3, "the call site address is reported");

    found = mhs::callscan::FindCalls(code.data(), code.size(), kCode, 0x400000);
    Check(found.count == 0, "a call to another callee does not match");

    std::vector<std::uint8_t> twice{0xE8, 0x5B, 0x02, 0x00, 0x00, 0x90,
                                   0xE8, 0x55, 0x02, 0x00, 0x00, 0xC3};
    found = mhs::callscan::FindCalls(twice.data(), twice.size(), kCode, kCallee);
    Check(found.count == 2, "both matching calls are counted");
    Check(found.at == kCode, "the first match wins");

    std::vector<std::uint8_t> truncated{0x90, 0xE8, 0x58, 0x02};
    found = mhs::callscan::FindCalls(truncated.data(), truncated.size(), kCode, kCallee);
    Check(found.count == 0, "a call cut off by the end of the range is ignored");

    found = mhs::callscan::FindCalls(nullptr, 64, kCode, kCallee);
    Check(found.count == 0, "a null range finds nothing");
}

} // namespace

int main() {
    TestArcCounts();
    TestArcGeometry();
    TestHoldState();
    TestFadeAnim();
    TestPromptDevice();
    TestPadButtons();
    TestCallScan();

    if (g_failures == 0) {
        std::printf("all checks passed\n");
        return 0;
    }
    std::printf("%d check(s) failed\n", g_failures);
    return 1;
}
