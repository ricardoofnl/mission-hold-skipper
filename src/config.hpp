#pragma once

#include <cstdint>
#include <string>

#include "sa/game.hpp"

namespace mhs {

struct Config {
    bool enabled{true};

    std::uint32_t holdMs{1200};
    bool          keyEnter{true};
    bool          keyNumpadEnter{true};
    bool          keySpace{false};
    bool          keyMouseLeft{true};

    // off by default, the prompt only shows while the key is actually held
    bool        showHintBeforeHold{false};
    std::string label{"HOLD ENTER TO SKIP"};

    std::uint32_t fadeInMs{140};
    std::uint32_t fadeOutMs{220};

    // -1 keeps the default bottom right placement, otherwise these are 640x448 units
    float ringX{-1.0f};
    float ringY{-1.0f};
    float ringRadius{13.0f};
    float ringThickness{2.5f};
    int   ringSegments{48};

    float labelScaleX{0.28f};
    float labelScaleY{0.7f};

    // plain black disc, the white arc fills over its rim while ENTER is held
    sa::CRGBA colorBackdrop{0, 0, 0, 200};
    sa::CRGBA colorTrack{255, 255, 255, 0};
    sa::CRGBA colorProgress{255, 255, 255, 255};

    std::string logLevel{"info"};
};

Config& Cfg();

// missing file or missing keys just leave the defaults in place
void LoadConfig(const std::string& iniPath);

} // namespace mhs