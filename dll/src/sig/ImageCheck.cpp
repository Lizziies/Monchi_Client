#include "Image.hpp"

#include <windows.h>

#include <algorithm>
#include <string>

namespace image {

namespace {

struct Probe {
    virtual ~Probe() = default;
    virtual int first() { return 1; }
#if defined(_MSC_VER)
    __declspec(noinline)
#else
    __attribute__((noinline))
#endif
    virtual const char* mark() { return "monchi image check anchor 4f1c"; }
    virtual int last() { return 3; }
};

}

// the resolver run against this very dll: string, function and vtable of a class with a known layout
bool selfCheck(std::string& why) {
    auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"Monchi.dll"));
    if (!base) base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    Image img(base);

    Probe probe;
    volatile const char* keep = probe.mark();
    (void)keep;
    auto vtable = *reinterpret_cast<uintptr_t*>(&probe);
    const std::string anchor = "monchi image check anchor 4f1c";

    auto funcs = anchorFuncs(img, anchor);
    if (funcs.empty()) {
        why = "anchor string has no function";
        return false;
    }
    int slot = -1;
    for (int i = 0; i < 8 && slot < 0; i++)
        for (auto f : funcs)
            if (reinterpret_cast<uintptr_t*>(vtable)[i] == f) slot = i;
    if (slot < 0) {
        why = "anchor function is not in the vtable of the probe";
        return false;
    }
    auto tables = anchorVtables(img, anchor, slot);
    if (std::find(tables.begin(), tables.end(), vtable) == tables.end()) {
        why = "vtable of the probe not found from the anchor";
        return false;
    }
    if (!img.inCode(reinterpret_cast<uintptr_t*>(vtable)[slot + 1])) {
        why = "next slot is not code";
        return false;
    }
    return true;
}

}
