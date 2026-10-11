#pragma once

#include <array>

namespace gameinput {

void tick();
bool active();
const char* status();

void hold(int vk);
// takes a key out of what the game reads for this frame; the mouse button codes (VK_LBUTTON to VK_XBUTTON2) work too
void drop(int vk);
void cancelKey(int vk, bool cancel);
// The game reads its keys from GameInput, where SendInput never arrives. A latch stays down until it is released, a tap
// stays down for the given time and at least for one frame.
void latch(int vk, bool down);
void tap(int vk, int ms);
void clearLatches();
// clicks above the limit and the bounce of a worn switch never reach the game; 0 turns a limit off
void limitClicks(int leftPerSecond, int rightPerSecond, float debounceMs);
void scaleMouse(float factor);
// mouse presses the game was not shown, counted by reason: menu open, window not focused, held over from the menu,
// taken out by a module, click limiter
std::array<unsigned, 5> hiddenClicks();
// left clicks with shift held in the keyboard reading, and how many of them the game got without the click or the shift
std::array<unsigned, 2> shiftClickCount();
void scaleMouse(float x, float y);
void smoothMouse(float factor);
void holdWheel();

void beginFrame();
void publish();

// The game asks GameInput for the current reading once per frame, right before it uses the input. A frame limit that
// waits there, and not after Present, keeps the wait in front of the input instead of behind it; the GPU vendor's
// pacing (Reflex, Anti-Lag 2) belongs at the same spot. Only used once that poll has shown a steady one-per-frame rhythm.
struct FrameStart {
    bool hooked = false;
    bool verified = false;
    float waitMs = 0.f;
    float sampleAgeMs = 0.f;
    float pollsPerFrame = 0.f;
};

void pace(float fps);
bool frameStartVerified();
FrameStart frameStart();
// called at Present time: how old the newest input the game has taken is at that moment
void notePresent();

}
