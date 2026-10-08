// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <algorithm>
#include <cmath>
namespace walkBob {
struct Motion {
    float phase = 0.f, strength = 0.f;
    void update(float distance, float dt) {
        if (!std::isfinite(distance) || !std::isfinite(dt) || dt <= 0.f || dt > 0.25f || distance > 2.f) {
            phase = strength = 0.f;
            return;
        }
        phase = std::remainder(phase + distance * 4.f, 6.2831853f);
        const float target = std::clamp(distance / dt / 4.3f, 0.f, 1.f);
        strength += (target - strength) * (1.f - std::exp(-12.f * dt));
    }
    float sideways() const { return std::sin(phase) * strength * 0.035f; }
    float vertical() const { return -std::abs(std::cos(phase)) * strength * 0.025f; }
};
}
