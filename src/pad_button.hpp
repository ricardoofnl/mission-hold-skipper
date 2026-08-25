#pragma once

#include <cstddef>
#include <string_view>

namespace mhs {

// offsets are into CControllerState, texture names into GInput's button txd packs
struct PadButton {
    std::string_view token;
    std::size_t      offset;
    std::string_view texture;
};

inline constexpr PadButton kPadButtons[]{
    {"PAD_CROSS", 0x20, "cross"},
    {"PAD_CIRCLE", 0x22, "circle"},
    {"PAD_SQUARE", 0x1C, "square"},
    {"PAD_TRIANGLE", 0x1E, "triangle"},
    {"PAD_L1", 0x08, "l1"},
    {"PAD_L2", 0x0A, "l2"},
    {"PAD_R1", 0x0C, "r1"},
    {"PAD_R2", 0x0E, "r2"},
};

inline constexpr std::size_t kPadButtonCount = std::size(kPadButtons);

inline int FindPadButton(std::string_view token) {
    for (std::size_t i = 0; i < kPadButtonCount; ++i) {
        if (kPadButtons[i].token == token) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

inline int FirstPadButton(unsigned mask) {
    for (std::size_t i = 0; i < kPadButtonCount; ++i) {
        if (mask & (1u << i)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

} // namespace mhs
