#pragma once

namespace tweaks {

void timerResolution(bool on);
void highPriority(bool on);
void noPowerThrottling(bool on);
void inputBoost(bool on);
bool wantsInputBoost();
void threadBoost(bool on);
void restore();

struct State {
    bool priority = false;
    float timerMs = 0.f;
    bool powerThrottlingOff = false;
    bool scheduling = false;
};

State state();

}
