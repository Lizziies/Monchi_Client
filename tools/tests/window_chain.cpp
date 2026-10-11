#include "hook/WindowChain.hpp"
#include <cstdlib>
#include <cstdio>

LRESULT CALLBACK first(HWND w, UINT m, WPARAM wp, LPARAM lp) { return DefWindowProcW(w, m, wp, lp); }
LRESULT CALLBACK second(HWND w, UINT m, WPARAM wp, LPARAM lp) { if (m == WM_APP) return 42; return DefWindowProcW(w, m, wp, lp); }
void check(bool ok) { static int step = 0; ++step; if (!ok) { std::fprintf(stderr, "window check %d failed, error %lu\n", step, GetLastError()); std::abort(); } }
int main() {
    HWND w = CreateWindowExW(0, L"STATIC", L"", 0, 0, 0, 1, 1, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    check(w != nullptr);
    auto original = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(w, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(first)));
    SetWindowLongPtrW(w, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(second));
    check(!input::detachWindow(w, first, original));
    check(GetWindowLongPtrW(w, GWLP_WNDPROC) == reinterpret_cast<LONG_PTR>(second));
    SetWindowLongPtrW(w, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(first));
    check(input::detachWindow(w, first, original));
    check(GetWindowLongPtrW(w, GWLP_WNDPROC) == reinterpret_cast<LONG_PTR>(original));
    DestroyWindow(w);
    check(input::detachWindow(w, first, original));
}
