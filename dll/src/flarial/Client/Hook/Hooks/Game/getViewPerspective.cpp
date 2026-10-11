// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial getViewPerspective.cpp: Monchi shares the installed perspective hook.
#include "getViewPerspective.hpp"
#include "Bridge/CameraRequests.hpp"

namespace {
int dispatch(int original) {
    try {
        auto event = nes::make_holder<PerspectiveEvent>((Perspective)original);
        eventMgr.trigger(event);
        int view = monchiCamera::perspective.load();
        monchiCamera::perspectiveCalls.fetch_add(1, std::memory_order_relaxed);
        int result = view >= 0 && view <= 2 ? view : int(event->getPerspective());
        if (view >= 0 && result != original) monchiCamera::perspectiveChanges.fetch_add(1, std::memory_order_relaxed);
        return result;
    } catch (...) { return original; }
}
}

int getViewPerspectiveHook::callback(uintptr_t* a1) {
    int original = 0;
    __try {
        original = getViewPerspectiveOriginal(a1);
        return dispatch(original);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return original; }
}

void getViewPerspectiveHook::enableHook() {
    monchiCamera::perspectiveReady = autoHook((void*)callback, (void**)&getViewPerspectiveOriginal);
}
