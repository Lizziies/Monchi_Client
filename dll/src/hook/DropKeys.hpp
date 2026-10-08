#pragma once

#include <bitset>
#include <cstdint>

namespace gameinput {

inline int scanIndex(uint32_t scan) {
    return int((scan & 0xff) | ((scan & 0xe000) ? 0x100 : 0));
}

class DropKeys {
public:
    template<class Map>
    void update(const std::bitset<256>& keys, uintptr_t layout, Map map) {
        if (keys == keys_ && layout == layout_) return;
        keys_ = keys;
        layout_ = layout;
        scans_.reset();
        for (int vk = 0; vk < 256; ++vk)
            if (keys[vk]) scans_.set(scanIndex(map(vk)));
    }

    bool contains(uint8_t vk, uint32_t scan) const {
        return keys_[vk] || scans_[scanIndex(scan)];
    }

private:
    std::bitset<256> keys_;
    std::bitset<512> scans_;
    uintptr_t layout_ = 0;
};

}
