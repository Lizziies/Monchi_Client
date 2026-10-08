#pragma once

#include <windows.h>

namespace game {
struct LoadState {
    bool finished;
    bool loaded;
};
inline LoadState waitLoad(HANDLE thread, DWORD timeout) {
    if (!thread) return {true, false};
    if (WaitForSingleObject(thread, timeout) != WAIT_OBJECT_0) return {false, false};
    DWORD result = 0;
    return {true, GetExitCodeThread(thread, &result) && result != 0};
}
}
