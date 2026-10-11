// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <atomic>

// Monchi's MC GUI Scale changes what GuiData::calculateGuiScale returns, but the game only asks that function when the
// window or its own scale option changes. A request here makes the game recompute on its own thread.
namespace monchiScreen {
inline std::atomic<int> wanted{0};

// called from the screen hook, on the game's thread, after a screen has been drawn
void serve(void *clientInstance, void *screenView);
}

extern "C" {
using MonchiFlarialRefreshScreen = void (*)();
}
