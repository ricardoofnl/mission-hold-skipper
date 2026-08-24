#pragma once

#include <cstddef>
#include <cstdint>

namespace mhs::ring {

// one annulus segment, vertices ordered inner(a0), outer(a0), outer(a1), inner(a1)
// which is a valid triangle fan for CSprite2d::Draw2DPolygon
struct Quad {
    float x[4];
    float y[4];
};

constexpr int kMinSegments = 8;
constexpr int kMaxSegments = 256;

int ClampSegments(int segments);

// fills out with the segments covering progress of a full turn, starting at the
// top and going clockwise, returns how many were written
std::size_t BuildArc(Quad* out, std::size_t capacity, float cx, float cy,
                     float outerRadius, float innerRadius, float progress, int segments);

} // namespace mhs::ring