#include "ring.hpp"

#include <algorithm>
#include <cmath>

namespace mhs::ring {

namespace {
constexpr float kTwoPi = 6.283185307179586f;
constexpr float kStart = -1.570796326794897f; // straight up
} // namespace

int ClampSegments(int segments) {
    return std::clamp(segments, kMinSegments, kMaxSegments);
}

std::size_t BuildArc(Quad* out, std::size_t capacity, float cx, float cy,
                     float outerRadius, float innerRadius, float progress, int segments) {
    if (!out || capacity == 0) {
        return 0;
    }

    progress = std::clamp(progress, 0.0f, 1.0f);
    if (progress <= 0.0f || outerRadius <= innerRadius) {
        return 0;
    }

    const int   count = ClampSegments(segments);
    const float step  = kTwoPi / static_cast<float>(count);
    const float sweep = kTwoPi * progress;

    std::size_t written = 0;
    for (int i = 0; i < count && written < capacity; ++i) {
        // index driven so accumulated float error cannot add a degenerate segment
        const float a0 = step * static_cast<float>(i);
        if (a0 >= sweep - 1e-5f) {
            break;
        }
        const float a1 = std::min(a0 + step, sweep);
        const float c0 = std::cos(kStart + a0);
        const float s0 = std::sin(kStart + a0);
        const float c1 = std::cos(kStart + a1);
        const float s1 = std::sin(kStart + a1);

        Quad& q = out[written++];
        q.x[0] = cx + c0 * innerRadius;
        q.y[0] = cy + s0 * innerRadius;
        q.x[1] = cx + c0 * outerRadius;
        q.y[1] = cy + s0 * outerRadius;
        q.x[2] = cx + c1 * outerRadius;
        q.y[2] = cy + s1 * outerRadius;
        q.x[3] = cx + c1 * innerRadius;
        q.y[3] = cy + s1 * innerRadius;
    }

    return written;
}

} // namespace mhs::ring
