#pragma once

#include <algorithm>
#include <array>
#include <span>

namespace fpsSamples {

inline double low(std::span<const double> samples) {
    if (samples.empty() || samples.size() > 512) return 0;
    std::array<double, 512> sorted;
    std::copy(samples.begin(), samples.end(), sorted.begin());
    size_t n = std::max<size_t>(1, samples.size() / 100);
    std::partial_sort(sorted.begin(), sorted.begin() + n, sorted.begin() + samples.size(), std::greater<>());
    double worst = 0;
    for (size_t i = 0; i < n; i++) worst += sorted[i];
    return worst > 0 ? n * 1000.0 / worst : 0;
}

}
