#pragma once

#include "LatencyControl.hpp"
#include <atomic>

namespace gpuLatency {

class PaceGate {
public:
    void update(const Control* control) {
        ready_.store(control && control->status().stage == Stage::BeforeInput, std::memory_order_release);
    }

    bool ready() const { return ready_.load(std::memory_order_acquire); }

private:
    std::atomic<bool> ready_{false};
};

}
