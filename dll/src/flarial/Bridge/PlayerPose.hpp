// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <mutex>
namespace playerPose {
struct Pose { double time; float yaw, pitch, y, x, z; bool valid; };
inline std::mutex lock;
inline Pose value{};
inline Pose read() { std::scoped_lock guard(lock); return value; }
}
