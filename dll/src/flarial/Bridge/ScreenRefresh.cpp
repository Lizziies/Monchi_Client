// SPDX-License-Identifier: AGPL-3.0-only
#include "ScreenRefresh.hpp"

#include "SDK/Client/Render/ScreenView/ScreenView.hpp"
#include "Utils/Logger/Logger.hpp"
#include "Utils/Memory/Game/SignatureAndOffsetManager.hpp"

#include <windows.h>

namespace {

// static, 1.26.52: slot 73 of the ClientInstance vtable (0x5dbc390) takes the gui's own screen size and the safe zone and
// hands them to ClientInstance::_updateScreenSizeVariables (0x5dbc530), which stores what GuiData::calculateGuiScale
// returns as the gui scale and derives the scaled screen size from it. The slot is checked before the call, so another
// class behind the pointer costs the refresh and not the game.
constexpr int slot = 73;

bool recompute(void *instance, uintptr_t fn) {
    __try {
        auto *table = *reinterpret_cast<uintptr_t **>(instance);
        if (!table || table[slot] != fn) return false;
        reinterpret_cast<void (*)(void *)>(fn)(instance);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void layout(ScreenView *view) {
    try {
        if (!view || !view->VisualTree || !view->VisualTree->root) return;
        view->VisualTree->root->forEachChild([](std::shared_ptr<UIControl> &control) { control->updatePosition(); });
    } catch (...) {
    }
}

bool layoutGuarded(ScreenView *view) {
    __try {
        layout(view);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

}

void monchiScreen::serve(void *clientInstance, void *screenView) {
    if (wanted.load() <= 0 || !clientInstance) return;
    static ULONGLONG last = 0;
    ULONGLONG now = GetTickCount64();
    if (now - last < 50) return;
    last = now;
    wanted.fetch_sub(1);

    static uintptr_t fn = GET_SIG_ADDRESS("ClientInstance::updateScreenSize");
    static int told = 0;
    bool ok = fn && recompute(clientInstance, fn);
    if (ok) layoutGuarded(static_cast<ScreenView *>(screenView));
    int state = ok ? 1 : 2;
    if (told == state) return;
    told = state;
    if (ok) Logger::info("gui scale: the game recomputed its screen size and scale on request");
    else Logger::warn("gui scale: the game's recompute function {}", fn ? "does not sit where expected, nothing was called" : "was not found");
}

extern "C" __declspec(dllexport) void monchiFlarialRefreshScreen() {
    // a few times in a row: the value Monchi wants may only be in place a frame after the request
    monchiScreen::wanted = 3;
}
