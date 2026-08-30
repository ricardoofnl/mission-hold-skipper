#pragma once

// one backend per game, picked at compile time, aliased to game:: for the shared
// logic in hold_skip
#if defined(MHS_GAME_III)
#include "game/iii/backend.hpp"
#elif defined(MHS_GAME_VC)
#include "game/vc/backend.hpp"
#else
#include "game/sa/backend.hpp"
#endif

namespace mhs {
#if defined(MHS_GAME_III)
namespace game = iii;
#elif defined(MHS_GAME_VC)
namespace game = vc;
#else
namespace game = sa;
#endif
} // namespace mhs
