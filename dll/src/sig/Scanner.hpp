#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace scanner {

struct Pattern {
    std::vector<uint8_t> bytes;
    std::vector<bool> mask;
};

std::optional<Pattern> parse(const std::string& text);

struct Region {
    uintptr_t start;
    size_t size;
};

std::vector<Region> codeRegions(uintptr_t moduleBase);
std::vector<uintptr_t> find(const Pattern& p, const std::vector<Region>& regions, size_t limit = 2);

uintptr_t resolveRip(uintptr_t at, int offset, int length);

}
