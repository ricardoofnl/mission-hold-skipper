#pragma once

#include <cstdint>
#include <string>

#include "game/bindings.hpp"
#include "pad_button.hpp"
#include "prompt_device.hpp"

namespace mhs {

enum class IconMode { Auto, Sprite, Off };

enum class IconStyle { Auto, PlayStation, Xbox };

struct Config {
    bool enabled{true};

    std::uint32_t holdMs{1200};
    bool          keyEnter{true};
    bool          keyNumpadEnter{true};
    bool          keySpace{false};
    bool          keyMouseLeft{true};
    unsigned      padMask{1u << 0}; // PAD_CROSS

    bool         showHintBeforeHold{false};
    std::string  label{"HOLD ENTER TO SKIP"};
    std::string  labelPad{"HOLD TO SKIP"};
    PromptDevice promptDevice{PromptDevice::Auto};

    std::uint32_t fadeInMs{140};
    std::uint32_t fadeOutMs{220};

    // -1 keeps the bottom right placement, anything else is in 640x448 units
    float ringX{-1.0f};
    float ringY{-1.0f};
    float ringRadius{18.0f};
    float ringThickness{2.5f};
    int   ringSegments{48};

    IconMode  iconMode{IconMode::Auto};
    IconStyle iconStyle{IconStyle::Auto};
    float     iconScale{1.5f};

    float labelScaleX{0.28f};
    float labelScaleY{0.7f};
    // -1 keeps each game's own default font
    int   fontStyle{-1};

    game::CRGBA colorBackdrop{0, 0, 0, 200};
    game::CRGBA colorTrack{255, 255, 255, 0};
    game::CRGBA colorProgress{255, 255, 255, 255};

    std::string logLevel{"info"};
};

Config& Cfg();

void LoadConfig(const std::string& iniPath);

} // namespace mhs
