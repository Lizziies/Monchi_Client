#pragma once

#include "core/Events.hpp"

#include <windows.h>

#include <cstdint>

namespace input {

bool install(HWND window);
bool uninstall();

struct Ours {
    Ours();
    ~Ours();
};

bool gameplay();
void forgetPlay();
bool grabbed();
void screenHint(int screen);
int screen();
bool focused();
bool cursorShown();
// once per frame: reads focus and cursor state for everyone who asks during the frame
void sample();
bool takeFocusLost();
bool inMinecraft();
void syncCursor(bool menuOpen);
void releaseHeld();

bool down(int vk);
// Once a frame: a key this table still holds as pressed although the keyboard says it is up has lost its release
// (it went to another window, or another hook took it). It is let go here, or whatever a module holds for it
// (walking on after a menu) would never end.
void reconcile();
int cps(MouseButton button);
int64_t lastClickQpc();
int64_t lastMoveQpc();

void consumeMotion(int& dx, int& dy);

}
