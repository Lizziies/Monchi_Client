#pragma once

#include <windows.h>

namespace input {
inline bool detachWindow(HWND window, WNDPROC ours, WNDPROC previous) {
    if (!window || !previous || !IsWindow(window)) return true;
    auto current = reinterpret_cast<WNDPROC>(GetWindowLongPtrW(window, GWLP_WNDPROC));
    if (current != ours) return false;
    SetLastError(0);
    auto result = SetWindowLongPtrW(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(previous));
    return result != 0 || GetLastError() == 0;
}
}
