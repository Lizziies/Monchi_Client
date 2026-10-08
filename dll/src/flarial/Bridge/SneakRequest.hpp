// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial Twerk::onTick: apply all three sneak inputs on the game tick.
#pragma once
#include <atomic>
namespace monchiSneak {
inline std::atomic<int> request{-1};
inline std::atomic<bool> applied{false};
inline std::atomic<bool> failed{false};
}
