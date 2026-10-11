#include "hook/DropKeys.hpp"
#include <windows.h>
#include <cassert>
#include <chrono>
#include <cstdio>

int main() {
    gameinput::DropKeys cache;
    std::bitset<256> keys;
    int calls = 0;
    auto map = [&](int vk) { ++calls; return uint32_t(vk == VK_RCONTROL ? 0xe01d : vk); };
    keys.set(VK_RCONTROL);
    cache.update(keys, 1, map);
    assert(cache.contains(VK_CONTROL, 0xe01d));
    assert(!cache.contains(VK_CONTROL, 0x1d));
    assert(cache.contains(VK_RCONTROL, 0));
    cache.update(keys, 1, map); assert(calls == 1);
    cache.update(keys, 2, map); assert(calls == 2);
    keys.reset(); cache.update(keys, 2, map);
    assert(!cache.contains(VK_RCONTROL, 0xe01d));

    HKL layout = GetKeyboardLayout(0);
    auto native = [layout](int vk) { return MapVirtualKeyExW(UINT(vk), MAPVK_VK_TO_VSC_EX, layout); };
    keys.set('A'); keys.set('D'); keys.set(VK_LSHIFT); keys.set(VK_RCONTROL);
    cache.update(keys, reinterpret_cast<uintptr_t>(layout), native);
    for (int vk = 0; vk < 256; ++vk) {
        uint32_t scan = native(vk);
        bool old = keys[vk];
        for (int drop = 0; drop < 256 && !old; ++drop)
            old = keys[drop] && gameinput::scanIndex(native(drop)) == gameinput::scanIndex(scan);
        assert(old == cache.contains(uint8_t(vk), scan));
    }
    volatile unsigned result = 0;
    auto bench = [&](bool cached) {
        auto begin = std::chrono::steady_clock::now();
        for (int i = 0; i < 100000; ++i) {
            uint8_t vk = uint8_t('A' + i % 26);
            uint32_t scan = uint32_t(1 + i % 80);
            bool blocked = cached ? cache.contains(vk, scan) : keys[vk];
            if (!cached)
                for (int drop = 0; drop < 256 && !blocked; ++drop)
                    blocked = keys[drop] && gameinput::scanIndex(native(drop)) == gameinput::scanIndex(scan);
            result = result + unsigned(blocked);
        }
        return std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - begin).count() / 100000;
    };
    double old = bench(false), cached = bench(true);
    std::printf("drop lookup: old %.3f us, cached %.3f us per key (%u)\n", old, cached, unsigned(result));
}
