// SPDX-License-Identifier: AGPL-3.0-only
// Monchi's drain of the core's queue of module switches (see Client/Module/Manager.hpp). Kept free of the core's headers so a test
// can run it: a switch that throws must be reported and dropped, and must never hold up the ones behind it.
#pragma once

#include <mutex>
#include <queue>
#include <utility>

namespace toggleQueue {

template <class Item, class Apply, class Failed>
void drain(std::queue<Item>& queue, std::mutex& lock, Apply&& apply, Failed&& failed) {
    std::queue<Item> now;
    {
        std::lock_guard<std::mutex> guard(lock);
        std::swap(now, queue);
    }
    while (!now.empty()) {
        Item item = std::move(now.front());
        now.pop();
        try {
            apply(item);
        } catch (...) {
            failed(item);
        }
    }
}

}
