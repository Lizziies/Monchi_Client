// SPDX-License-Identifier: AGPL-3.0-only
// Monchi's key forwarding into Flarial's KeyHook, exported from MonchiFlarial.dll (see Input.hpp).
#include "Input.hpp"

#include "Hook/Hooks/Input/KeyHook.hpp"
#include "Events/Input/MouseEvent.hpp"
#include "Client.hpp"
#include "GUI/D2D.hpp"
#include "Utils/VersionUtils.hpp"

#include <windows.h>

extern "C" {

__declspec(dllexport) bool monchiFlarialKey(void* hwnd, unsigned msg, unsigned long long wp, long long lp) {
    // older versions still feed keys through the Keyboard::feed hook; handling them here too would fire twice
    if (!VersionUtils::checkAboveOrEqual(21, 120)) return false;
    if (monchiInput::captured && msg != WM_KILLFOCUS) return false;
    if ((msg == WM_KEYDOWN || msg == WM_KEYUP || msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP) && wp >= 256) return false;
    return KeyHook::handle(static_cast<HWND>(hwnd), msg, WPARAM(wp), LPARAM(lp));
}

// 1.26.52 has no InputHandler::tick or MouseDevice::feed binding, so the mouse reaches Flarial's modules from Monchi's window hook
__declspec(dllexport) bool monchiFlarialMouse(int button, int action, int x, int y, int dx, int dy) {
    // Flarial's own mouse listener is not registered in this build, so nothing else tells the modules which buttons are
    // down (Block Hit reads the right one); a release counts even while Monchi's menu has the input
    if (button == 1) MC::heldLeft = action == 1;
    if (button == 2) MC::heldRight = action == 1;
    if (monchiInput::captured || Client::disable) return false;
    auto event = nes::make_holder<MouseEvent>(static_cast<char>(button), static_cast<char>(action), static_cast<short>(x),
                                              static_cast<short>(y), static_cast<short>(dx), static_cast<short>(dy));
    eventMgr.trigger(event);
    return event->isCancelled();
}

// 1.26.52 has no binding that stops the game's chat from drawing, so its panel is pushed off the screen instead
__declspec(dllexport) void monchiFlarialHideChat(bool hide) {
    monchiInput::hideChat = hide;
}

__declspec(dllexport) void monchiFlarialHideScoreboard(bool hide) {
    monchiInput::hideScoreboard = hide;
}

__declspec(dllexport) void monchiFlarialCapture(bool captured) {
    if (captured == monchiInput::captured) return;
    monchiInput::captured = captured;
    // Monchi stops forwarding the mouse while its menu is open, so a release in there would never arrive
    if (captured) MC::heldLeft = MC::heldRight = false;
    // keys held when Monchi took over would stay down in Flarial's key table
    if (captured) KeyHook::handle(nullptr, WM_KILLFOCUS, 0, 0);
}

}
