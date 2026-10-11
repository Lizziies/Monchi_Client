#pragma once

#include "core/Events.hpp"

#include <windows.h>

inline int mouseVk(MouseButton b) {
    switch (b) {
    case MouseButton::Middle: return VK_MBUTTON;
    case MouseButton::X1: return VK_XBUTTON1;
    case MouseButton::X2: return VK_XBUTTON2;
    default: return 0;
    }
}
