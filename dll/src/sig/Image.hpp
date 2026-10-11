#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace image {

struct Range {
    uintptr_t start = 0;
    size_t size = 0;
};

class Image {
public:
    explicit Image(uintptr_t base);

    uintptr_t base() const { return base_; }
    bool inCode(uintptr_t at) const;
    bool contains(uintptr_t at) const { return at >= base_ && at < base_ + size_; }

    // start of every string that contains text, found in the non-executable sections
    std::vector<uintptr_t> strings(const std::string& text) const;
    // places in code that load target with a rip-relative lea
    std::vector<uintptr_t> leaRefs(uintptr_t target) const;
    // function start from the unwind table, 0 when at is not inside a function
    uintptr_t funcStart(uintptr_t at) const;
    // addresses of 8-byte values equal to func in the data sections, so vtable slots
    std::vector<uintptr_t> slotsHolding(uintptr_t func) const;

private:
    uintptr_t base_ = 0;
    size_t size_ = 0;
    std::vector<Range> code_;
    std::vector<Range> data_;
    uintptr_t pdata_ = 0;
    size_t pdataCount_ = 0;
};

// functions that use a string containing text, sorted by address
std::vector<uintptr_t> anchorFuncs(const Image& img, const std::string& text);
// vtable bases where the anchor function sits at slot, sorted by address
std::vector<uintptr_t> anchorVtables(const Image& img, const std::string& text, int slot);
bool selfCheck(std::string& why);

}
