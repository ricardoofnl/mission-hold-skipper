#pragma once

#include <cstddef>

#include "game/sa/game.hpp"

// the shared logic only ever talks to these, every backend declares the same set
namespace mhs::sa {

inline constexpr const char* kGameName = "GTA SA";
inline constexpr const char* kGameVersion = "1.0 US";
inline constexpr std::uint32_t kDefaultHoldMs = 1200;

bool VersionMatches();
bool InstallHooks();

// SA gates every skip path through one function, so the skip itself needs nothing
inline constexpr bool kSkipIsExplicit = false;
// SA ticks from the input hook and from the draw hook, so it needs the frame guard
inline constexpr bool kNeedsFrameGuard = true;
bool SkipAvailable();
inline void PerformSkip() {}

bool KeyEnterDown();
bool KeyNumpadEnterDown();
bool KeySpaceDown();
bool MouseLeftDown();
bool PadButtonDown(std::size_t stateOffset);
bool KeyboardOrMouseActive();
bool PadActive(bool keyboardActive);

void DrawLabel(float x, float y, const char* text, CRGBA color, CRGBA drop,
               float scaleX, float scaleY, float wrapAt, int fontStyle);

} // namespace mhs::sa
