#include "Scanner.hpp"

#include <windows.h>

#include <cctype>
#include <cstring>

namespace scanner {

std::optional<Pattern> parse(const std::string& text) {
    Pattern p;
    size_t i = 0;
    while (i < text.size()) {
        if (std::isspace((unsigned char)text[i])) {
            i++;
            continue;
        }
        if (text[i] == '?') {
            p.bytes.push_back(0);
            p.mask.push_back(false);
            while (i < text.size() && text[i] == '?') i++;
            continue;
        }
        if (i + 1 >= text.size() || !std::isxdigit((unsigned char)text[i]) || !std::isxdigit((unsigned char)text[i + 1]))
            return std::nullopt;
        p.bytes.push_back((uint8_t)std::stoul(text.substr(i, 2), nullptr, 16));
        p.mask.push_back(true);
        i += 2;
    }
    if (p.bytes.empty() || !p.mask.front()) return std::nullopt;
    return p;
}

std::vector<Region> codeRegions(uintptr_t base) {
    std::vector<Region> out;
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    auto* sec = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++) {
        if (sec->Characteristics & IMAGE_SCN_MEM_EXECUTE)
            out.push_back({base + sec->VirtualAddress, sec->Misc.VirtualSize});
    }
    return out;
}

std::vector<uintptr_t> find(const Pattern& p, const std::vector<Region>& regions, size_t limit) {
    std::vector<uintptr_t> hits;
    size_t n = p.bytes.size();
    uint8_t first = p.bytes[0];

    for (auto& r : regions) {
        auto* data = reinterpret_cast<const uint8_t*>(r.start);
        if (r.size < n) continue;
        const uint8_t* end = data + r.size - n + 1;
        const uint8_t* cur = data;
        while (cur < end) {
            cur = static_cast<const uint8_t*>(std::memchr(cur, first, size_t(end - cur)));
            if (!cur) break;
            bool ok = true;
            for (size_t k = 1; k < n; k++) {
                if (p.mask[k] && cur[k] != p.bytes[k]) {
                    ok = false;
                    break;
                }
            }
            if (ok) {
                hits.push_back(reinterpret_cast<uintptr_t>(cur));
                if (hits.size() >= limit) return hits;
            }
            cur++;
        }
    }
    return hits;
}

uintptr_t resolveRip(uintptr_t at, int offset, int length) {
    int32_t rel = 0;
    std::memcpy(&rel, reinterpret_cast<void*>(at + offset), sizeof(rel));
    return at + length + rel;
}

}
