#pragma once
#include "system/LatencyControl.hpp"

namespace perf {

void begin();
void apply();

void lowLatency();
void gpu(gpuLatency::Mode mode);
// Every frame waits for the display. With `underRefresh` the frame rate is also held a little under the refresh
// rate, in front of the input: a frame then never has to wait for its turn, and on a display with a variable rate
// (G-Sync, FreeSync) it stays inside the range in which nothing tears.
void syncToDisplay(bool underRefresh = true);
// the display's refresh rate and the cap that goes with it, for the settings pages
int refreshRate();
float underRefreshCap();
// false once the display was seen not to follow the frame rate; the cap under the refresh rate is then left out
bool displayFollows();
// another tool in the game already paces the frames (RTSS); Reflex and frame caps are then left to it
bool foreignPacer();
void tearing();
void limit(float fps);
// the limit waits in front of the game's input poll when that poll is known, and after Present otherwise
void alignToInput();

}
