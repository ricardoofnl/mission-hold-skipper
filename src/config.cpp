#include "config.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>

#include <mini/ini.h>

#include "log.hpp"

namespace mhs {

namespace {

constexpr const char* kSection = "MissionHoldSkipper";

std::string Upper(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return value;
}

bool ParseBool(const std::string& value, bool fallback) {
    if (value.empty()) {
        return fallback;
    }
    const auto upper = Upper(value);
    return upper == "1" || upper == "TRUE" || upper == "YES" || upper == "ON";
}

// RRGGBBAA, alpha optional
game::CRGBA ParseColor(const std::string& value, game::CRGBA fallback) {
    if (value.size() != 6 && value.size() != 8) {
        return fallback;
    }
    char*      end   = nullptr;
    const auto packed = std::strtoul(value.c_str(), &end, 16);
    if (end != value.c_str() + value.size()) {
        return fallback;
    }
    if (value.size() == 6) {
        return game::CRGBA{static_cast<std::uint8_t>((packed >> 16) & 0xFF),
                         static_cast<std::uint8_t>((packed >> 8) & 0xFF),
                         static_cast<std::uint8_t>(packed & 0xFF), 255};
    }
    return game::CRGBA{static_cast<std::uint8_t>((packed >> 24) & 0xFF),
                     static_cast<std::uint8_t>((packed >> 16) & 0xFF),
                     static_cast<std::uint8_t>((packed >> 8) & 0xFF),
                     static_cast<std::uint8_t>(packed & 0xFF)};
}

void ParseKeys(const std::string& value, Config& cfg) {
    if (value.empty()) {
        return;
    }
    cfg.keyEnter = cfg.keyNumpadEnter = cfg.keySpace = cfg.keyMouseLeft = false;
    cfg.padMask  = 0;

    const auto upper = Upper(value);
    for (std::size_t start = 0; start <= upper.size();) {
        const auto comma = upper.find(',', start);
        const auto end   = comma == std::string::npos ? upper.size() : comma;
        auto       token = upper.substr(start, end - start);
        token.erase(0, token.find_first_not_of(" \t"));
        if (const auto last = token.find_last_not_of(" \t"); last != std::string::npos) {
            token.erase(last + 1);
        }

        if (token == "ENTER") {
            cfg.keyEnter = true;
        } else if (token == "NUMPAD_ENTER") {
            cfg.keyNumpadEnter = true;
        } else if (token == "SPACE") {
            cfg.keySpace = true;
        } else if (token == "MOUSE_LEFT") {
            cfg.keyMouseLeft = true;
        } else if (const auto pad = FindPadButton(token); pad >= 0) {
            cfg.padMask |= 1u << pad;
        } else if (!token.empty()) {
            MHS_LOG_WARN("unknown key in Keys: %s", token.c_str());
        }

        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    }

    if (!cfg.keyEnter && !cfg.keyNumpadEnter && !cfg.keySpace && !cfg.keyMouseLeft && cfg.padMask == 0) {
        MHS_LOG_WARN("Keys left nothing usable, falling back to ENTER");
        cfg.keyEnter = cfg.keyNumpadEnter = true;
    }
}

IconMode ParseIconMode(const std::string& value, IconMode fallback) {
    if (value.empty()) {
        return fallback;
    }
    const auto upper = Upper(value);
    if (upper == "SPRITE") {
        return IconMode::Sprite;
    }
    if (upper == "OFF" || upper == "0") {
        return IconMode::Off;
    }
    if (upper == "AUTO") {
        return IconMode::Auto;
    }
    MHS_LOG_WARN("unknown PadIconMode '%s', keeping auto", value.c_str());
    return IconMode::Auto;
}

IconStyle ParseIconStyle(const std::string& value, IconStyle fallback) {
    if (value.empty()) {
        return fallback;
    }
    const auto upper = Upper(value);
    if (upper == "PS" || upper == "PLAYSTATION") {
        return IconStyle::PlayStation;
    }
    if (upper == "XBOX" || upper == "X360") {
        return IconStyle::Xbox;
    }
    if (upper == "AUTO") {
        return IconStyle::Auto;
    }
    MHS_LOG_WARN("unknown PadIconStyle '%s', keeping auto", value.c_str());
    return IconStyle::Auto;
}

PromptDevice ParsePromptDevice(const std::string& value, PromptDevice fallback) {
    if (value.empty()) {
        return fallback;
    }
    const auto upper = Upper(value);
    if (upper == "KEYBOARD") {
        return PromptDevice::Keyboard;
    }
    if (upper == "PAD") {
        return PromptDevice::Pad;
    }
    if (upper == "AUTO") {
        return PromptDevice::Auto;
    }
    MHS_LOG_WARN("unknown PromptDevice '%s', keeping auto", value.c_str());
    return PromptDevice::Auto;
}

} // namespace

Config& Cfg() {
    static Config cfg;
    return cfg;
}

void LoadConfig(const std::string& iniPath) {
    Config& cfg = Cfg();

    mINI::INIFile      file(iniPath);
    mINI::INIStructure ini;
    if (!file.read(ini) || !ini.has(kSection)) {
        MHS_LOG_WARN("no usable config at %s, using defaults", iniPath.c_str());
        return;
    }

    const auto& section = ini[kSection];
    const auto  get     = [&section](const char* key) -> std::string {
        return section.has(key) ? section.get(key) : std::string{};
    };
    const auto number = [&get](const char* key, float fallback) {
        const auto raw = get(key);
        return raw.empty() ? fallback : std::strtof(raw.c_str(), nullptr);
    };

    cfg.enabled            = ParseBool(get("Enabled"), cfg.enabled);
    cfg.holdMs             = static_cast<std::uint32_t>(number("HoldMs", static_cast<float>(cfg.holdMs)));
    cfg.showHintBeforeHold = ParseBool(get("ShowHintBeforeHold"), cfg.showHintBeforeHold);
    cfg.fadeInMs           = static_cast<std::uint32_t>(number("FadeInMs", static_cast<float>(cfg.fadeInMs)));
    cfg.fadeOutMs          = static_cast<std::uint32_t>(number("FadeOutMs", static_cast<float>(cfg.fadeOutMs)));
    cfg.ringX              = number("RingX", cfg.ringX);
    cfg.ringY              = number("RingY", cfg.ringY);
    cfg.ringRadius         = number("RingRadius", cfg.ringRadius);
    cfg.ringThickness      = number("RingThickness", cfg.ringThickness);
    cfg.ringSegments       = static_cast<int>(number("RingSegments", static_cast<float>(cfg.ringSegments)));
    cfg.iconMode           = ParseIconMode(get("PadIconMode"), cfg.iconMode);
    cfg.iconStyle          = ParseIconStyle(get("PadIconStyle"), cfg.iconStyle);
    cfg.iconScale          = number("PadIconScale", cfg.iconScale);
    cfg.labelScaleX        = number("LabelScaleX", cfg.labelScaleX);
    cfg.labelScaleY        = number("LabelScaleY", cfg.labelScaleY);
    cfg.fontStyle          = static_cast<int>(number("FontStyle", static_cast<float>(cfg.fontStyle)));
    cfg.colorBackdrop      = ParseColor(get("ColorBackdrop"), cfg.colorBackdrop);
    cfg.colorTrack         = ParseColor(get("ColorTrack"), cfg.colorTrack);
    cfg.colorProgress      = ParseColor(get("ColorProgress"), cfg.colorProgress);

    if (const auto label = get("Label"); !label.empty()) {
        cfg.label = label;
    }
    if (section.has("LabelPad")) {
        cfg.labelPad = get("LabelPad");
    }
    cfg.promptDevice = ParsePromptDevice(get("PromptDevice"), cfg.promptDevice);
    if (const auto level = get("LogLevel"); !level.empty()) {
        cfg.logLevel = level;
    }
    ParseKeys(get("Keys"), cfg);

    if (cfg.holdMs < 50) {
        MHS_LOG_WARN("HoldMs %u is too short, clamping to 50", cfg.holdMs);
        cfg.holdMs = 50;
    }
    if (cfg.ringThickness >= cfg.ringRadius) {
        MHS_LOG_WARN("RingThickness %.1f does not fit RingRadius %.1f, halving it",
                     cfg.ringThickness, cfg.ringRadius);
        cfg.ringThickness = cfg.ringRadius * 0.5f;
    }
}

} // namespace mhs
