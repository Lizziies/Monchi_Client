#pragma once
#include <atomic>
namespace monchiCamera {
inline std::atomic<float> zoom{1.f};
inline std::atomic<bool> hideCrosshair{false};
inline std::atomic<bool> hideHud{false};
inline std::atomic<int> perspective{-1};
inline std::atomic<bool> perspectiveReady{false};
inline std::atomic<unsigned> perspectiveCalls{0}, perspectiveChanges{0};
}
