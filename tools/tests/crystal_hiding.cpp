#ifdef NDEBUG
#undef NDEBUG
#endif
#include "sdk/HiddenFlag.hpp"
#include "flarial/Bridge/AttackQueue.hpp"
#include <cassert>
#include <thread>
int main() {
    constexpr uint8_t mask = 32;
    assert(hiddenFlag::value(9, mask, false, false) == 41);
    assert(hiddenFlag::value(43, mask, false, true) == 11);
    assert(hiddenFlag::value(43, mask, true, true) == 43);
    std::thread producer([] { for (uintptr_t i = 1; i <= 40; ++i) monchiAttack::push(i); });
    producer.join();
    uintptr_t out[32]{};
    assert(monchiAttack::drain(out, 32) == 32);
    for (uintptr_t i = 0; i < 32; ++i) assert(out[i] == i + 1);
    assert(monchiAttack::drain(out, 32) == 0);
    monchiAttack::push(99);
    assert(monchiAttack::drain(out, 32) == 1 && out[0] == 99);
}
