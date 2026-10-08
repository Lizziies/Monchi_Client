#pragma once

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <cstring>

namespace mem {

inline std::atomic<uint32_t> frameStamp{1};

inline void nextFrame() { frameStamp.fetch_add(1, std::memory_order_relaxed); }

// Most writes go to the game's own heap objects, which are writable anyway. Asking once per page and frame is
// one syscall instead of two VirtualProtect calls (each flushing the TLB) for every single write.
inline bool writable(uintptr_t address, size_t size) {
    struct Entry {
        uintptr_t page;
        uint32_t stamp;
        bool ok;
    };
    thread_local Entry cache[8]{};
    uintptr_t page = address & ~uintptr_t(0xFFF);
    if (((address + size - 1) & ~uintptr_t(0xFFF)) != page) return false;
    uint32_t now = frameStamp.load(std::memory_order_relaxed);
    Entry& e = cache[(page >> 12) & 7];
    if (e.page == page && e.stamp == now) return e.ok;
    MEMORY_BASIC_INFORMATION mbi{};
    bool ok = VirtualQuery(reinterpret_cast<void*>(page), &mbi, sizeof(mbi)) && mbi.State == MEM_COMMIT &&
              (mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE)) && !(mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS));
    e = {page, now, ok};
    return ok;
}

// set while a guarded read runs, so the fault net does not report an address that was only being tried
inline thread_local int probing = 0;

inline bool readSlow(uintptr_t address, void* out, size_t size) {
    SIZE_T got = 0;
    return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), out, size, &got) && got == size;
}

// The live readers take a thousand and more values a frame. ReadProcessMemory is a system call each time (measured
// 332 ns); a copy under a fault handler is the same protection against a pointer that has gone away for 2.6 ns.
inline bool readFast(uintptr_t address, void* out, size_t size) {
#ifdef _MSC_VER
    bool ok = true;
    probing++;
    __try {
        std::memcpy(out, reinterpret_cast<const void*>(address), size);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        ok = false;
    }
    probing--;
    return ok;
#else
    return readSlow(address, out, size);
#endif
}

inline bool readable(uintptr_t address, size_t size) { return address >= 0x10000 && address < 0x7FFFFFFF0000 && size < 0x7FFFFFFF0000 - address; }

template <class T>
bool read(uintptr_t address, T& out) {
    if (!readable(address, sizeof(T))) return false;
    return readFast(address, &out, sizeof(T));
}

// big blocks (the heap sweep) stay with the system call, which copies what it can without a fault per page
inline bool readBytes(uintptr_t address, void* out, size_t size) {
    if (!readable(address, size)) return false;
    return size <= 4096 ? readFast(address, out, size) : readSlow(address, out, size);
}

template <class T>
T get(uintptr_t address, T fallback = T{}) {
    T v{};
    return read(address, v) ? v : fallback;
}

inline uintptr_t pointer(uintptr_t address) { return get<uintptr_t>(address, 0); }

// a page that was writable when it was asked about can be gone a moment later, when another thread frees it
inline bool copyGuarded(void* to, const void* from, size_t size) {
#ifdef _MSC_VER
    __try {
        std::memcpy(to, from, size);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#else
    std::memcpy(to, from, size);
    return true;
#endif
}

template <class T>
bool write(uintptr_t address, const T& value) {
    if (address < 0x10000) return false;
    if (writable(address, sizeof(T))) return copyGuarded(reinterpret_cast<void*>(address), &value, sizeof(T));
    DWORD old = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(address), sizeof(T), PAGE_EXECUTE_READWRITE, &old)) return false;
    bool ok = copyGuarded(reinterpret_cast<void*>(address), &value, sizeof(T));
    VirtualProtect(reinterpret_cast<void*>(address), sizeof(T), old, &old);
    return ok;
}

}
