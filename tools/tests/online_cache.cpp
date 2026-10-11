#include "modules/online/UserCache.hpp"
#include <cassert>
#include <map>
#include <thread>

int main() {
    UserCache<int> cache;
    int value = -1;
    assert(!cache.find("Player", value));
    cache.replace(std::map<std::string, int>{{"player", 42}});
    assert(cache.find("pLaYeR", value) && value == 42);
    assert(!cache.find("player2", value));
    auto retained = cache.snapshot();
    std::atomic<bool> done{false};
    std::thread writer([&] {
        for (int i = 0; i < 10000; ++i)
            cache.replace(std::map<std::string, int>{{"one", i}, {"two", i}});
        done = true;
    });
    while (!done) {
        auto snapshot = cache.snapshot();
        if (snapshot->size() == 2) assert(snapshot->at("ONE") == snapshot->at("TwO"));
    }
    writer.join();
    assert(retained->at("PLAYER") == 42);
    cache.replace(std::map<std::string, int>{});
    assert(!cache.find("one", value));
}
