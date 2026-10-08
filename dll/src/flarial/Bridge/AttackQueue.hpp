#pragma once

#include <array>
#include <cstdint>
#include <mutex>

namespace monchiAttack {
inline std::mutex lock;
inline std::array<uintptr_t, 32> actors{};
inline size_t count = 0;

inline void push(uintptr_t actor) {
    std::scoped_lock guard(lock);
    if (count < actors.size()) actors[count++] = actor;
}

inline unsigned drain(uintptr_t* out, unsigned capacity) {
    std::scoped_lock guard(lock);
    unsigned n = count < capacity ? unsigned(count) : capacity;
    for (unsigned i = 0; i < n; ++i) out[i] = actors[i];
    count = 0;
    return n;
}
}
