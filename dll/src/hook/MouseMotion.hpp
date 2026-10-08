#pragma once

#include <cmath>
#include <cstdint>

struct MouseMotion {
    double held = 0;
    double fraction = 0;

    int64_t step(int64_t delta, float scale, double release) {
        held += double(delta) * scale;
        double out = release >= 1.0 ? held : held * release;
        held -= out;
        if (std::fabs(held) < 0.01) held = 0;
        double value = out + fraction;
        int64_t result = int64_t(std::floor(value));
        fraction = value - double(result);
        return result;
    }
};
