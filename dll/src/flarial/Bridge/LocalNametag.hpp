// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <mutex>
#include <string>
namespace localNametag {
inline std::mutex lock;
inline std::string name;
inline bool matches(const std::string& tag) {
    std::scoped_lock guard(lock);
    return !name.empty() && tag == name;
}
}
