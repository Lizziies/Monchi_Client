#include "modules/hud/FpsSamples.hpp"
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

double old(std::span<const double> samples) {
    std::array<double, 512> sorted;
    std::copy(samples.begin(), samples.end(), sorted.begin());
    std::sort(sorted.begin(), sorted.begin() + samples.size(), std::greater<>());
    size_t n = std::max<size_t>(1, samples.size() / 100);
    double sum = 0;
    for (size_t i = 0; i < n; i++) sum += sorted[i];
    return sum > 0 ? n * 1000.0 / sum : 0;
}
int main() {
    assert(fpsSamples::low({}) == 0);
    std::array<double, 512> data;
    std::mt19937 random(52);
    for (int round = 0; round < 20; round++) {
        for (auto& value : data) value = (random() % 10000) / 100.0;
        for (size_t count : {1u, 2u, 99u, 100u, 101u, 511u, 512u}) {
            std::span<const double> values(data.data(), count);
            assert(std::fabs(fpsSamples::low(values) - old(values)) < 1e-9);
        }
    }
    data.fill(0);
    assert(fpsSamples::low(data) == 0);
    for (auto& value : data) value = (random() % 10000) / 100.0;
    volatile double sink = 0;
    auto bench = [&](auto fn) {
        auto begin = std::chrono::steady_clock::now();
        for (int i = 0; i < 20000; i++) sink = fn(data);
        return std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - begin).count() / 20000;
    };
    std::printf("1%% low: full sort %.3f us, partial sort %.3f us per update\n", bench(old), bench(fpsSamples::low));
}
