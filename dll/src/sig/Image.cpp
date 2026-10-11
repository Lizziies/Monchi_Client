#include "Image.hpp"

#include <windows.h>

#include <algorithm>
#include <cstring>
#include <functional>

namespace image {

namespace {

bool lookup(uintptr_t value, const std::vector<Range>& ranges) {
    for (auto& r : ranges)
        if (value >= r.start && value < r.start + r.size) return true;
    return false;
}

}

Image::Image(uintptr_t base) : base_(base) {
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    size_ = nt->OptionalHeader.SizeOfImage;
    auto* sec = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++) {
        Range r{base + sec->VirtualAddress, sec->Misc.VirtualSize};
        if (sec->Characteristics & IMAGE_SCN_MEM_EXECUTE) code_.push_back(r);
        else if (sec->Characteristics & IMAGE_SCN_MEM_READ) data_.push_back(r);
    }
    auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
    pdata_ = dir.VirtualAddress ? base + dir.VirtualAddress : 0;
    pdataCount_ = dir.Size / sizeof(RUNTIME_FUNCTION);
}

bool Image::inCode(uintptr_t at) const { return lookup(at, code_); }

std::vector<uintptr_t> Image::strings(const std::string& text) const {
    std::vector<uintptr_t> out;
    if (text.empty()) return out;
    for (auto& r : data_) {
        auto* begin = reinterpret_cast<const char*>(r.start);
        auto* end = begin + r.size;
        std::boyer_moore_horspool_searcher search(text.begin(), text.end());
        for (auto* it = begin;;) {
            it = std::search(it, end, search);
            if (it == end) break;
            auto* start = it;
            while (start > begin && start[-1] != 0) start--;
            uintptr_t at = reinterpret_cast<uintptr_t>(start);
            if (out.empty() || out.back() != at) out.push_back(at);
            it += text.size();
        }
    }
    return out;
}

std::vector<uintptr_t> Image::leaRefs(uintptr_t target) const {
    std::vector<uintptr_t> out;
    for (auto& r : code_) {
        auto* data = reinterpret_cast<const uint8_t*>(r.start);
        // REX.W lea reg, [rip+disp32]: 48|4c 8d modrm(mod 0, rm 5) disp32
        for (size_t i = 0; i + 7 <= r.size; i++) {
            if (data[i + 1] != 0x8D || (data[i] != 0x48 && data[i] != 0x4C) || (data[i + 2] & 0xC7) != 0x05) continue;
            int32_t disp;
            std::memcpy(&disp, data + i + 3, sizeof(disp));
            uintptr_t at = r.start + i;
            if (at + 7 + disp == target) out.push_back(at);
        }
    }
    return out;
}

uintptr_t Image::funcStart(uintptr_t at) const {
    if (!pdata_ || at < base_) return 0;
    auto* table = reinterpret_cast<const RUNTIME_FUNCTION*>(pdata_);
    DWORD rva = DWORD(at - base_);
    size_t lo = 0, hi = pdataCount_;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (table[mid].EndAddress <= rva) lo = mid + 1;
        else hi = mid;
    }
    if (lo >= pdataCount_ || table[lo].BeginAddress > rva) return 0;

    const RUNTIME_FUNCTION* f = &table[lo];
    for (int guard = 0; guard < 16; guard++) {
        auto* info = reinterpret_cast<const uint8_t*>(base_ + (f->UnwindData & ~3u));
        if (!(info[0] >> 3 & 4)) break;
        size_t codes = (info[2] + 1u) & ~1u;
        f = reinterpret_cast<const RUNTIME_FUNCTION*>(info + 4 + codes * 2);
    }
    return base_ + f->BeginAddress;
}

std::vector<uintptr_t> Image::slotsHolding(uintptr_t func) const {
    std::vector<uintptr_t> out;
    for (auto& r : data_) {
        uintptr_t first = (r.start + 7) & ~uintptr_t(7);
        for (uintptr_t at = first; at + 8 <= r.start + r.size; at += 8)
            if (*reinterpret_cast<const uintptr_t*>(at) == func) out.push_back(at);
    }
    return out;
}

std::vector<uintptr_t> anchorFuncs(const Image& img, const std::string& text) {
    std::vector<uintptr_t> out;
    for (uintptr_t s : img.strings(text))
        for (uintptr_t ref : img.leaRefs(s))
            if (uintptr_t f = img.funcStart(ref)) out.push_back(f);
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

std::vector<uintptr_t> anchorVtables(const Image& img, const std::string& text, int slot) {
    std::vector<uintptr_t> out;
    for (uintptr_t f : anchorFuncs(img, text))
        for (uintptr_t at : img.slotsHolding(f))
            if (at >= uintptr_t(slot) * 8) out.push_back(at - uintptr_t(slot) * 8);
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

}
