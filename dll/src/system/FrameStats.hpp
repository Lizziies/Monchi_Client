#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace timing {

struct Summary {
    size_t count = 0;
    double mean = 0, p95 = 0, p99 = 0, worst = 0;
};

class FrameStats {
public:
    void add(double ms) {
        if (!std::isfinite(ms) || ms <= 0) return;
        values_[next_] = ms;
        next_ = (next_ + 1) % values_.size();
        count_ = std::min(count_ + 1, values_.size());
    }

    void clear() { next_ = count_ = 0; }

    Summary summary() const {
        Summary result;
        result.count = count_;
        if (!count_) return result;
        auto sorted = values_;
        for (size_t i = 0; i < count_; i++) result.mean += sorted[i];
        result.mean /= count_;
        std::sort(sorted.begin(), sorted.begin() + count_);
        result.p95 = sorted[size_t(std::ceil(count_ * 0.95)) - 1];
        result.p99 = sorted[size_t(std::ceil(count_ * 0.99)) - 1];
        result.worst = sorted[count_ - 1];
        return result;
    }

private:
    std::array<double, 256> values_{};
    size_t next_ = 0, count_ = 0;
};

}
