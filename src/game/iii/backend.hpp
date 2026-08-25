#pragma once

#include <cstddef>

#include "game/iii/game.hpp"

namespace mhs::iii {

inline constexpr const char* kGameName = "GTA III";
inline constexpr const char* kGameVersion = "1.0";

// vanilla skips the moment the button goes down, so the skip itself is ours to do
inline constexpr bool kSkipIsExplicit = true;

bool VersionMatches();
bool InstallHooks();

bool SkipAvailable();
void PerformSkip();

bool KeyEnterDown();
bool KeyNumpadEnterDown();
bool KeySpaceDown();
bool MouseLeftDown();
bool PadButtonDown(std::size_t stateOffset);
bool KeyboardOrMouseActive();
bool PadActive(bool keyboardActive);

void DrawLabel(float x, float y, const char* text, CRGBA color, CRGBA drop,
               float scaleX, float scaleY, float wrapAt);

} // namespace mhs::iii
