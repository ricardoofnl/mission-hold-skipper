// host side checks for the parts that never touch game memory
#include <cmath>
#include <cstdio>
#include <vector>

#include "hold_state.hpp"
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

    // first segment starts straight up
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

    // completing and releasing in the same frame must not leave a pending skip
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

    // hiding takes the longer duration, and it keeps drawing until it lands on zero
    fade.Update(false, 100);
    Check(std::fabs(fade.Value() - 0.5f) < 0.001f, "fade out follows its own duration");
    Check(fade.Visible(), "still drawn while fading out");

    fade.Update(false, 500);
    Check(fade.Value() == 0.0f, "fade out stops at zero");
    Check(!fade.Visible(), "hidden once the fade out finished");
}

} // namespace

int main() {
    TestArcCounts();
    TestArcGeometry();
    TestHoldState();
    TestFadeAnim();

    if (g_failures == 0) {
        std::printf("all checks passed\n");
        return 0;
    }
    std::printf("%d check(s) failed\n", g_failures);
    return 1;
}