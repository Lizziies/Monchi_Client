#pragma once

#include <windows.h>
#include <algorithm>

namespace pacing {

template<class Focused>
bool waitBackground(HANDLE timer, DWORD timeout, Focused focused) {
    ULONGLONG end = GetTickCount64() + timeout;
    for (;;) {
        if (focused()) return false;
        ULONGLONG now = GetTickCount64();
        if (now >= end) return true;
        if (WaitForSingleObject(timer, DWORD(std::min<ULONGLONG>(10, end - now))) != WAIT_TIMEOUT) return true;
    }
}

}
