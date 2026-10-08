#include "sdk/BlockLookup.hpp"
#include <array>
#include <cstdlib>
#include <cstring>
#include <windows.h>

namespace {
alignas(8) std::array<uint8_t, 0x200> actor{}, dimension{}, source{}, chunk{}, held{}, block{}, type{};
alignas(8) std::array<uint8_t, 0x68 * 24> subs{};
std::array<uintptr_t, 13> dimensionTable{};
std::array<uintptr_t, 3> sourceTable{};
std::array<uintptr_t, 4> storageTable{};
uintptr_t storage[1]{};
int calls = 0;
unsigned seen = 0;
uintptr_t getBlock(uintptr_t, const blockLookup::Pos*) { std::abort(); }
uintptr_t fakeElement(uintptr_t from, unsigned index) {
    if (from != reinterpret_cast<uintptr_t>(storage)) std::abort();
    calls++;
    seen = index;
    return reinterpret_cast<uintptr_t>(block.data());
}
template<class T> void put(uint8_t* at, T value) { std::memcpy(at, &value, sizeof(value)); }
void check(bool ok) { if (!ok) std::abort(); }
uint64_t key(int x, int z) { return uint64_t(uint32_t(x >> 4)) | uint64_t(uint32_t(z >> 4)) << 32; }
}

int main() {
    const auto image = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(image);
    const size_t size = reinterpret_cast<IMAGE_NT_HEADERS*>(image + dos->e_lfanew)->OptionalHeader.SizeOfImage;
    const auto player = reinterpret_cast<uintptr_t>(actor.data());
    const int lookup = int(reinterpret_cast<uintptr_t>(&getBlock) - image);
    const blockLookup::Layout layout{0x1c8, 0xf0, 2, lookup, 0x68, 0xe8};
    put(actor.data() + 0x1c8, reinterpret_cast<uintptr_t>(dimension.data()));
    put(dimension.data(), reinterpret_cast<uintptr_t>(dimensionTable.data()));
    dimensionTable[12] = image + 2;
    put(dimension.data() + 0xf0, reinterpret_cast<uintptr_t>(source.data()));
    put(source.data(), reinterpret_cast<uintptr_t>(sourceTable.data()));
    sourceTable[2] = image + lookup;
    put(source.data() + 0x3a, int16_t(-64));
    put(source.data() + 0x38, int16_t(320));
    put(source.data() + 0xb8, key(-17, 32));
    put(source.data() + 0xc8, reinterpret_cast<uintptr_t>(held.data()));
    put(source.data() + 0xd0, reinterpret_cast<uintptr_t>(chunk.data()));
    put(held.data() + 8, 1);
    put(chunk.data() + 0x78, key(-17, 32));
    put(chunk.data() + 0x140, reinterpret_cast<uintptr_t>(subs.data()));
    put(chunk.data() + 0x148, reinterpret_cast<uintptr_t>(subs.data() + subs.size()));
    // y 65 with the world starting at -64 is the ninth sub chunk
    put(subs.data() + 8 * 0x68 + 0x30, reinterpret_cast<uintptr_t>(storage));
    storage[0] = reinterpret_cast<uintptr_t>(storageTable.data());
    storageTable[3] = reinterpret_cast<uintptr_t>(&fakeElement);
    put(block.data() + 0x68, reinterpret_cast<uintptr_t>(type.data()));
    auto* name = type.data() + 0xe8;
    std::memcpy(name, "minecraft:stone", 15);
    put(name + 16, size_t(15));
    put(name + 24, size_t(15));

    check(blockLookup::name(player, image, size, layout, {-17, 65, 32}) == "minecraft:stone");
    check(seen == (unsigned(-17 & 15) << 8 | unsigned(32 & 15) << 4 | unsigned((65 + 64) & 15)));
    std::string custom = "custom:polished_crystal_block";
    put(name, reinterpret_cast<uintptr_t>(custom.data()));
    put(name + 16, custom.size());
    put(name + 24, custom.size());
    check(blockLookup::name(player, image, size, layout, {-17, 65, 32}) == custom);

    // another chunk than the one the game has cached, a chunk that is gone, a height outside the world: no call
    int before = calls;
    check(blockLookup::name(player, image, size, layout, {0, 65, 32}).empty() && calls == before);
    put(held.data() + 8, 0);
    check(blockLookup::name(player, image, size, layout, {-17, 65, 32}).empty() && calls == before);
    put(held.data() + 8, 1);
    put(chunk.data() + 0x78, key(0, 0));
    check(blockLookup::name(player, image, size, layout, {-17, 65, 32}).empty() && calls == before);
    put(chunk.data() + 0x78, key(-17, 32));
    check(blockLookup::name(player, image, size, layout, {-17, 320, 32}).empty() && calls == before);
    check(blockLookup::name(player, image, size, layout, {-17, -65, 32}).empty() && calls == before);
    // a storage whose function is not the game's is never called
    storageTable[3] = reinterpret_cast<uintptr_t>(storage);
    check(blockLookup::name(player, image, size, layout, {-17, 65, 32}).empty() && calls == before);
    storageTable[3] = reinterpret_cast<uintptr_t>(&fakeElement);

    sourceTable[2] = 0;
    check(blockLookup::name(player, image, size, layout, {-17, 65, 32}).empty() && calls == before);
    sourceTable[2] = image + lookup;
    dimensionTable[12] = 0;
    check(blockLookup::name(player, image, size, layout, {-17, 65, 32}).empty() && calls == before);
    dimensionTable[12] = image + 2;
    put(name + 16, size_t(257));
    check(blockLookup::name(player, image, size, layout, {-17, 65, 32}).empty());
    put(name + 16, custom.size());
    put(name, uintptr_t(1));
    check(blockLookup::name(player, image, size, layout, {-17, 65, 32}).empty());
    check(blockLookup::name(0, image, size, layout, {-17, 65, 32}).empty());
    auto missing = layout;
    missing.type = -1;
    check(blockLookup::name(player, image, size, missing, {-17, 65, 32}).empty());
    check(blockLookup::element(1, 1, 0) == 0);
}
