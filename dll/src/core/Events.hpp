#pragma once

#include <cstdint>
#include <string>

enum class MouseButton { None, Left, Right, Middle, X1, X2 };

struct KeyEvent {
    int vk = 0;
    bool down = false;
    bool repeat = false;
    bool cancel = false;
};

struct MouseEvent {
    MouseButton button = MouseButton::None;
    bool down = false;
    int wheel = 0;
    int dx = 0;
    int dy = 0;
    int64_t qpc = 0;
    bool cancel = false;
};

struct ServerEvent {
    std::string name;
    std::string host;
    bool joined = false;
};
