#pragma once

#include "Memory.hpp"
#include <string>

namespace blockLookup {

struct Pos { int x, y, z; };

struct Layout {
    int actorDimension;
    int dimensionSource;
    int sourceAccessor;
    int lookup;
    int type;
    int name;
};

inline uintptr_t element(uintptr_t function, uintptr_t storage, unsigned index) {
    __try {
        return reinterpret_cast<uintptr_t (*)(uintptr_t, unsigned)>(function)(storage, index);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

// BlockSource::getBlock (1.26.52.3, 0x2d80120) asks getChunk (0x2d7f440) for the chunk, and getChunk rewrites the
// source's last-chunk cache: key at +0xb8, a weak reference at +0xc0/+0xc8, the chunk at +0xd0. The source belongs to
// the game's thread. Calling getBlock from the frame thread makes the game release that weak reference once too
// often and die in free() (heap failure 8, stack ending in 0x2d7f571). So nothing is called
// that writes: the block is read out of the chunk the game has cached, when that is the chunk asked for. Otherwise
// there is no answer this time, and the caller asks again.
//
// Chunk: its position at +0x78, sub chunks as a vector at +0x140 (0x68 bytes each, block storage at +0x30). The
// storage's fourth virtual function returns the block at ((x & 15) << 8) | ((z & 15) << 4) | (y & 15) and only reads.
inline uintptr_t cached(uintptr_t source, uintptr_t image, size_t imageSize, const Pos& pos) {
    uint64_t want = uint64_t(uint32_t(pos.x >> 4)) | uint64_t(uint32_t(pos.z >> 4)) << 32;
    if (mem::get<uint64_t>(source + 0xb8, ~want) != want) return 0;
    uintptr_t held = mem::pointer(source + 0xc8);
    if (!held || !mem::get<int>(held + 8)) return 0;
    uintptr_t chunk = mem::pointer(source + 0xd0);
    if (!chunk || mem::get<uint64_t>(chunk + 0x78, ~want) != want) return 0;
    int low = mem::get<int16_t>(source + 0x3a), high = mem::get<int16_t>(source + 0x38);
    if (pos.y < low || pos.y >= high) return 0;
    unsigned up = unsigned(pos.y - low);
    uintptr_t first = mem::pointer(chunk + 0x140), last = mem::pointer(chunk + 0x148);
    if (!first || last <= first || (up >> 4) >= (last - first) / 0x68) return 0;
    uintptr_t storage = mem::pointer(first + uintptr_t(up >> 4) * 0x68 + 0x30);
    uintptr_t table = storage ? mem::pointer(storage) : 0;
    uintptr_t read = table ? mem::pointer(table + 3 * 8) : 0;
    if (read < image || read - image >= imageSize) return 0;
    return element(read, storage, unsigned(pos.x & 15) << 8 | unsigned(pos.z & 15) << 4 | (up & 15));
}

inline std::string name(uintptr_t actor, uintptr_t image, size_t imageSize, Layout layout, const Pos& pos) {
    if (!actor || !image || layout.actorDimension < 0 || layout.dimensionSource < 0 ||
        layout.sourceAccessor <= 0 || layout.lookup <= 0 || layout.type < 0 || layout.name < 0) return {};
    uintptr_t dimension = mem::pointer(actor + layout.actorDimension);
    uintptr_t table = dimension ? mem::pointer(dimension) : 0;
    if (!table || mem::pointer(table + 12 * 8) != image + layout.sourceAccessor) return {};
    uintptr_t source = mem::pointer(dimension + layout.dimensionSource);
    table = source ? mem::pointer(source) : 0;
    // the layout above is the one of the build whose getBlock sits here
    if (!table || mem::pointer(table + 2 * 8) != image + layout.lookup) return {};
    uintptr_t block = cached(source, image, imageSize, pos);
    uintptr_t type = block ? mem::pointer(block + layout.type) : 0;
    if (!type) return {};
    uintptr_t at = type + layout.name;
    size_t size = mem::get<size_t>(at + 16), capacity = mem::get<size_t>(at + 24);
    if (!size || size > 256 || capacity < size || (capacity < 16 && size > 15)) return {};
    uintptr_t data = capacity >= 16 ? mem::pointer(at) : at;
    std::string result(size, '\0');
    if (!data || !mem::readBytes(data, result.data(), size)) return {};
    // Block identifiers contain a namespace and path, rather than formatted display text.
    if (result.find(':') == std::string::npos || result.front() == ':' || result.back() == ':') return {};
    for (unsigned char c : result)
        if (!(c >= 'a' && c <= 'z') && !(c >= '0' && c <= '9') &&
            c != '_' && c != '-' && c != '.' && c != '/' && c != ':') return {};
    return result;
}

}
