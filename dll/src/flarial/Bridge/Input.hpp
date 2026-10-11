#pragma once
#include <atomic>

// Input side of the MonchiFlarial.dll interface. From 1.21.120 on Flarial reads keys from the game window instead of
// Keyboard::feed; the core has no window hook of its own, so Monchi passes the key messages it receives.

namespace monchiInput {
// set while Monchi's menu or another Monchi screen owns keyboard and mouse; Flarial's modules then see no input
inline std::atomic<bool> captured{false};
// set while Monchi's own chat replaces the game's: the chat panel is moved out of the picture
inline std::atomic<bool> hideChat{false};
inline std::atomic<bool> hideScoreboard{false};
}

extern "C" {
// true when a Flarial module consumed the message and the game must not see it
using MonchiFlarialKey = bool (*)(void* hwnd, unsigned msg, unsigned long long wp, long long lp);
using MonchiFlarialCapture = void (*)(bool captured);
using MonchiFlarialHideChat = void (*)(bool hide);
using MonchiFlarialHideScoreboard = void (*)(bool hide);
// button 1 left, 2 right, 3 middle, 4 wheel, 5/6 side buttons; action 1 press, 0 release, 0x78 wheel up, 0x88 wheel down
using MonchiFlarialMouse = bool (*)(int button, int action, int x, int y, int dx, int dy);
}
