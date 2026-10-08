#pragma once

#include <mutex>
#include <string>
#include <unordered_map>

namespace modulePolicy {
inline std::mutex mutex;
inline std::unordered_map<std::string, bool> blocked;

inline bool allowed(const std::string& name) {
    std::scoped_lock lock(mutex);
    auto it = blocked.find(name);
    return it != blocked.end() && !it->second;
}

inline void set(const std::string& name, bool block) {
    std::scoped_lock lock(mutex);
    blocked[name] = block;
}
}
