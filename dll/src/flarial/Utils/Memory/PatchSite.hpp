// SPDX-License-Identifier: AGPL-3.0-only
// New file for the 1.26.52 camera modules; replaces Flarial's fixed-length nopBytes/patchBytes calls, whose lengths
// were written for 1.26.3 instructions.
#pragma once

#include <windows.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>

namespace codepatch {

constexpr size_t maxLen = 16;

struct Site {
    uintptr_t address = 0;
    size_t length = 0;
    uint8_t original[maxLen]{};
    uint8_t patched[maxLen]{};
    bool applied = false;

    bool armed() const { return address != 0 && length != 0; }
};

inline bool readBytes(uintptr_t at, uint8_t* out, size_t n) {
    __try {
        memcpy(out, reinterpret_cast<const void*>(at), n);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

inline bool writeBytes(uintptr_t at, const uint8_t* in, size_t n) {
    DWORD old = 0;
    if (!VirtualProtect(reinterpret_cast<LPVOID>(at), n, PAGE_EXECUTE_READWRITE, &old)) return false;
    bool ok = true;
    __try {
        memcpy(reinterpret_cast<void*>(at), in, n);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        ok = false;
    }
    VirtualProtect(reinterpret_cast<LPVOID>(at), n, old, &old);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<LPCVOID>(at), n);
    return ok;
}

// Takes the site only if the bytes at the address are exactly the instruction(s) the analysis expected.
inline bool armBytes(Site& site, uintptr_t address, const uint8_t* expect, size_t n, const uint8_t* replacement) {
    site = {};
    if (!address || n == 0 || n > maxLen) return false;
    uint8_t now[maxLen];
    if (!readBytes(address, now, n) || memcmp(now, expect, n) != 0) return false;
    site.address = address;
    site.length = n;
    memcpy(site.original, now, n);
    memcpy(site.patched, replacement, n);
    return true;
}

inline bool arm(Site& site, uintptr_t address, std::initializer_list<uint8_t> expect, std::initializer_list<uint8_t> replacement) {
    if (replacement.size() != expect.size()) return false;
    return armBytes(site, address, expect.begin(), expect.size(), replacement.begin());
}

inline bool armNop(Site& site, uintptr_t address, std::initializer_list<uint8_t> expect) {
    uint8_t nops[maxLen];
    memset(nops, 0x90, sizeof nops);
    return armBytes(site, address, expect.begin(), expect.size(), nops);
}

// 0F 85 rel32 (jne) becomes 90 E9 rel32: the same six bytes, the same target, but always taken.
inline bool armJneToJmp(Site& site, uintptr_t address) {
    site = {};
    uint8_t now[6];
    if (!address || !readBytes(address, now, sizeof now)) return false;
    if (now[0] != 0x0F || now[1] != 0x85) return false;
    site.address = address;
    site.length = sizeof now;
    memcpy(site.original, now, sizeof now);
    memcpy(site.patched, now, sizeof now);
    site.patched[0] = 0x90;
    site.patched[1] = 0xE9;
    return true;
}

// 75 rel8 (jne) becomes EB rel8 (jmp).
inline bool armShortJneToJmp(Site& site, uintptr_t address) {
    site = {};
    uint8_t now[2];
    if (!address || !readBytes(address, now, sizeof now)) return false;
    if (now[0] != 0x75) return false;
    site.address = address;
    site.length = sizeof now;
    memcpy(site.original, now, sizeof now);
    memcpy(site.patched, now, sizeof now);
    site.patched[0] = 0xEB;
    return true;
}

inline bool engage(Site& site) {
    if (!site.armed() || site.applied) return site.applied;
    uint8_t now[maxLen];
    if (!readBytes(site.address, now, site.length) || memcmp(now, site.original, site.length) != 0) return false;
    site.applied = writeBytes(site.address, site.patched, site.length);
    return site.applied;
}

// Puts the original bytes back, but only when the site still holds our patch.
inline bool release(Site& site) {
    if (!site.applied) return true;
    uint8_t now[maxLen];
    if (readBytes(site.address, now, site.length) && memcmp(now, site.patched, site.length) == 0) {
        if (!writeBytes(site.address, site.original, site.length)) return false;
    }
    site.applied = false;
    return true;
}

}
