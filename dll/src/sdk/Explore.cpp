#include "Explore.hpp"
#include "Memory.hpp"

#ifdef MONCHI_DEV

#include "core/Log.hpp"
#include "core/Paths.hpp"
#include "sig/Scanner.hpp"

#include <windows.h>
#include <tlhelp32.h>

#include <capstone/capstone.h>

extern "C" {
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
}

#include <algorithm>
#include <cmath>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace explore {

namespace {

namespace fs = std::filesystem;

std::atomic<bool> busy{false};
std::atomic<bool> stopping{false};
fs::path scriptDir;
fs::path outDir;

uintptr_t imageBase() { return reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)); }

const IMAGE_NT_HEADERS* headers() {
    auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(imageBase());
    return reinterpret_cast<const IMAGE_NT_HEADERS*>(imageBase() + dos->e_lfanew);
}

struct Range {
    uintptr_t start = 0;
    size_t size = 0;
};

std::vector<Range> sections(bool code, bool data) {
    std::vector<Range> out;
    auto* nt = headers();
    auto* sec = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++) {
        bool exec = (sec->Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
        if ((exec && code) || (!exec && data)) out.push_back({imageBase() + sec->VirtualAddress, sec->Misc.VirtualSize});
    }
    return out;
}

bool inCode(uintptr_t at) {
    for (auto& r : sections(true, false))
        if (at >= r.start && at < r.start + r.size) return true;
    return false;
}

bool readMem(uintptr_t address, void* out, size_t n) {
    SIZE_T got = 0;
    return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), out, n, &got) && got == n;
}

// reads in chunks and calls fn(chunkAddress, bytes, count, span); chunks overlap by `overlap` bytes
template <class Fn>
void walk(uintptr_t start, size_t size, size_t overlap, Fn&& fn) {
    constexpr size_t chunk = 1u << 20;
    std::vector<uint8_t> buf(chunk + overlap);
    for (size_t off = 0; off < size; off += chunk) {
        size_t want = std::min(chunk + overlap, size - off);
        size_t n = want;
        if (!readMem(start + off, buf.data(), want)) {
            for (size_t page = 0; page < want; page += 0x1000) {
                size_t len = std::min<size_t>(0x1000, want - page);
                if (!readMem(start + off + page, buf.data() + page, len)) std::memset(buf.data() + page, 0, len);
            }
        }
        if (fn(start + off, buf.data(), n, std::min(chunk, size - off))) return;
    }
}

uintptr_t arg(lua_State* L, int i) { return static_cast<uintptr_t>(luaL_checkinteger(L, i)); }
void push(lua_State* L, uintptr_t v) { lua_pushinteger(L, static_cast<lua_Integer>(v)); }

int pushList(lua_State* L, const std::vector<uintptr_t>& list) {
    lua_newtable(L);
    for (size_t i = 0; i < list.size(); i++) {
        push(L, list[i]);
        lua_rawseti(L, -2, static_cast<int>(i) + 1);
    }
    return 1;
}

std::vector<uintptr_t> scan(const std::vector<Range>& ranges, const scanner::Pattern& p, size_t limit) {
    std::vector<uintptr_t> hits;
    size_t n = p.bytes.size();
    for (auto& r : ranges) {
        walk(r.start, r.size, n, [&](uintptr_t base, const uint8_t* data, size_t count, size_t span) {
            for (size_t i = 0; i < span && i + n <= count; i++) {
                if (data[i] != p.bytes[0]) continue;
                bool ok = true;
                for (size_t k = 1; k < n && ok; k++) ok = !p.mask[k] || data[i + k] == p.bytes[k];
                if (ok) hits.push_back(base + i);
                if (hits.size() >= limit) return true;
            }
            return false;
        });
        if (hits.size() >= limit) break;
    }
    return hits;
}

std::string toHex(const std::string& bytes, bool wide) {
    std::string hex;
    char tmp[8];
    for (unsigned char c : bytes) {
        std::snprintf(tmp, sizeof(tmp), "%02x ", c);
        hex += tmp;
        if (wide) hex += "00 ";
    }
    return hex;
}

int lBase(lua_State* L) {
    push(L, imageBase());
    return 1;
}

int lSections(lua_State* L) {
    auto* nt = headers();
    auto* sec = IMAGE_FIRST_SECTION(nt);
    lua_newtable(L);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++) {
        lua_newtable(L);
        lua_pushlstring(L, reinterpret_cast<const char*>(sec->Name), strnlen(reinterpret_cast<const char*>(sec->Name), 8));
        lua_setfield(L, -2, "name");
        push(L, imageBase() + sec->VirtualAddress);
        lua_setfield(L, -2, "start");
        lua_pushinteger(L, sec->Misc.VirtualSize);
        lua_setfield(L, -2, "size");
        lua_pushboolean(L, (sec->Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0);
        lua_setfield(L, -2, "exec");
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

int lHex(lua_State* L) {
    uintptr_t at = arg(L, 1);
    size_t n = std::min<size_t>(static_cast<size_t>(luaL_checkinteger(L, 2)), 1u << 16);
    std::vector<uint8_t> buf(n);
    if (!readMem(at, buf.data(), n)) return 0;
    std::string out;
    char tmp[4];
    for (auto b : buf) {
        std::snprintf(tmp, sizeof(tmp), "%02x", b);
        out += tmp;
    }
    lua_pushstring(L, out.c_str());
    return 1;
}

template <class T>
int readValue(lua_State* L) {
    T v{};
    if (!readMem(arg(L, 1), &v, sizeof(T))) return 0;
    if constexpr (std::is_floating_point_v<T>) lua_pushnumber(L, static_cast<lua_Number>(v));
    else lua_pushinteger(L, static_cast<lua_Integer>(v));
    return 1;
}

int lRaw(lua_State* L) {
    uintptr_t at = arg(L, 1);
    size_t n = std::min<size_t>(static_cast<size_t>(luaL_checkinteger(L, 2)), 1u << 20);
    std::vector<char> buf(n);
    if (!readMem(at, buf.data(), n)) return 0;
    lua_pushlstring(L, buf.data(), n);
    return 1;
}

int lWriteF32(lua_State* L) {
    lua_pushboolean(L, mem::write(arg(L, 1), static_cast<float>(luaL_checknumber(L, 2))));
    return 1;
}

int lCstr(lua_State* L) {
    uintptr_t at = arg(L, 1);
    int max = static_cast<int>(std::min<lua_Integer>(luaL_optinteger(L, 2, 128), 4096));
    std::string s;
    for (int i = 0; i < max; i++) {
        char c = 0;
        if (!readMem(at + i, &c, 1) || !c) break;
        s += c;
    }
    lua_pushstring(L, s.c_str());
    return 1;
}

int lFind(lua_State* L) {
    auto p = scanner::parse(luaL_checkstring(L, 1));
    if (!p) return luaL_error(L, "bad pattern");
    return pushList(L, scan(sections(true, false), *p, static_cast<size_t>(luaL_optinteger(L, 2, 16))));
}

int lFindData(lua_State* L) {
    auto p = scanner::parse(luaL_checkstring(L, 1));
    if (!p) return luaL_error(L, "bad pattern");
    return pushList(L, scan(sections(false, true), *p, static_cast<size_t>(luaL_optinteger(L, 2, 16))));
}

int lBytes(lua_State* L) {
    size_t len = 0;
    const char* text = luaL_checklstring(L, 1, &len);
    auto p = scanner::parse(toHex(std::string(text, len), lua_toboolean(L, 2)));
    if (!p) return luaL_error(L, "bad text");
    return pushList(L, scan(sections(false, true), *p, static_cast<size_t>(luaL_optinteger(L, 3, 16))));
}

// rip-relative operands: the 4-byte displacement is preceded by a modrm byte with mod=00 and rm=101
int lXrefs(lua_State* L) {
    uintptr_t target = arg(L, 1);
    size_t limit = static_cast<size_t>(luaL_optinteger(L, 2, 64));
    std::vector<uintptr_t> hits;
    for (auto& r : sections(true, false)) {
        walk(r.start, r.size, 16, [&](uintptr_t base, const uint8_t* data, size_t count, size_t span) {
            for (size_t i = 1; i < span && i + 4 <= count; i++) {
                if ((data[i - 1] & 0xC7) != 0x05) continue;
                int32_t disp;
                std::memcpy(&disp, data + i, 4);
                uintptr_t after = base + i + 4;
                for (int imm : {0, 1, 2, 4}) {
                    if (static_cast<uintptr_t>(static_cast<int64_t>(after + imm) + disp) == target) {
                        hits.push_back(base + i);
                        break;
                    }
                }
                if (hits.size() >= limit) return true;
            }
            return false;
        });
        if (hits.size() >= limit) break;
    }
    return pushList(L, hits);
}

int lCallers(lua_State* L) {
    uintptr_t target = arg(L, 1);
    size_t limit = static_cast<size_t>(luaL_optinteger(L, 2, 64));
    std::vector<uintptr_t> hits;
    for (auto& r : sections(true, false)) {
        walk(r.start, r.size, 8, [&](uintptr_t base, const uint8_t* data, size_t count, size_t span) {
            for (size_t i = 0; i < span && i + 5 <= count; i++) {
                if (data[i] != 0xE8 && data[i] != 0xE9) continue;
                int32_t disp;
                std::memcpy(&disp, data + i + 1, 4);
                if (static_cast<uintptr_t>(static_cast<int64_t>(base + i + 5) + disp) == target) hits.push_back(base + i);
                if (hits.size() >= limit) return true;
            }
            return false;
        });
        if (hits.size() >= limit) break;
    }
    return pushList(L, hits);
}

int lFunc(lua_State* L) {
    DWORD64 base = 0;
    auto* entry = RtlLookupFunctionEntry(arg(L, 1), &base, nullptr);
    if (!entry) return 0;
    for (int depth = 0; depth < 8; depth++) {
        auto* info = reinterpret_cast<const uint8_t*>(base + entry->UnwindData);
        if (!((info[0] >> 3) & 4)) break;
        int codes = info[2];
        entry = reinterpret_cast<PRUNTIME_FUNCTION>(const_cast<uint8_t*>(info) + 4 + ((codes + 1) & ~1) * 2);
    }
    push(L, static_cast<uintptr_t>(base + entry->BeginAddress));
    push(L, static_cast<uintptr_t>(base + entry->EndAddress));
    return 2;
}

int lVtable(lua_State* L) {
    uintptr_t at = arg(L, 1);
    int count = static_cast<int>(std::min<lua_Integer>(luaL_optinteger(L, 2, 32), 2048));
    lua_newtable(L);
    int out = 1;
    for (int i = 0; i < count; i++) {
        uintptr_t fn = 0;
        if (!readMem(at + i * 8, &fn, 8) || !inCode(fn)) break;
        push(L, fn);
        lua_rawseti(L, -2, out++);
    }
    return 1;
}

int lHeap(lua_State* L) {
    uint64_t value = static_cast<uint64_t>(luaL_checkinteger(L, 1));
    size_t limit = static_cast<size_t>(luaL_optinteger(L, 2, 16));
    std::vector<uintptr_t> hits;
    MEMORY_BASIC_INFORMATION mbi{};
    uintptr_t at = 0x10000;
    while (hits.size() < limit && VirtualQuery(reinterpret_cast<void*>(at), &mbi, sizeof(mbi))) {
        uintptr_t next = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
        bool ok = mbi.State == MEM_COMMIT && mbi.Type == MEM_PRIVATE && !(mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) &&
                  (mbi.Protect & (PAGE_READWRITE | PAGE_WRITECOPY));
        if (ok) {
            walk(reinterpret_cast<uintptr_t>(mbi.BaseAddress), mbi.RegionSize, 8, [&](uintptr_t base, const uint8_t* data, size_t count, size_t span) {
                for (size_t i = 0; i + 8 <= count && i < span; i += 8) {
                    uint64_t v;
                    std::memcpy(&v, data + i, 8);
                    if (v == value) hits.push_back(base + i);
                    if (hits.size() >= limit) return true;
                }
                return false;
            });
        }
        if (next <= at) break;
        at = next;
    }
    return pushList(L, hits);
}

// one to three consecutive floats inside the given ranges, in private writable memory
int lHeapFloats(lua_State* L) {
    float lo[3] = {-3e38f, -3e38f, -3e38f}, hi[3] = {3e38f, 3e38f, 3e38f};
    int given = std::min(3, lua_gettop(L) / 2);
    for (int i = 0; i < given; i++) {
        lo[i] = static_cast<float>(luaL_checknumber(L, 1 + i * 2));
        hi[i] = static_cast<float>(luaL_checknumber(L, 2 + i * 2));
    }
    size_t limit = static_cast<size_t>(luaL_optinteger(L, given * 2 + 1, 32));
    std::vector<uintptr_t> hits;
    MEMORY_BASIC_INFORMATION mbi{};
    uintptr_t at = 0x10000;
    while (hits.size() < limit && VirtualQuery(reinterpret_cast<void*>(at), &mbi, sizeof(mbi))) {
        uintptr_t next = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
        bool ok = mbi.State == MEM_COMMIT && mbi.Type == MEM_PRIVATE && !(mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) &&
                  (mbi.Protect & (PAGE_READWRITE | PAGE_WRITECOPY));
        if (ok) {
            walk(reinterpret_cast<uintptr_t>(mbi.BaseAddress), mbi.RegionSize, 12, [&](uintptr_t base, const uint8_t* data, size_t count, size_t span) {
                for (size_t i = 0; i + 12 <= count && i < span; i += 4) {
                    float v[3];
                    std::memcpy(v, data + i, 12);
                    if (v[0] >= lo[0] && v[0] <= hi[0] && v[1] >= lo[1] && v[1] <= hi[1] && v[2] >= lo[2] && v[2] <= hi[2])
                        hits.push_back(base + i);
                    if (hits.size() >= limit) return true;
                }
                return false;
            });
        }
        if (next <= at) break;
        at = next;
    }
    return pushList(L, hits);
}

// Narrowing scan over all private writable memory: scanstart(kind, lo, hi) keeps every address whose value is
// inside [lo, hi]; scannext(lo, hi) keeps the ones still inside. kind: 1 = u8, 4 = i32, 5 = f32.
int scanKind = 0;
std::vector<uintptr_t> scanHits;

bool scanMatch(const uint8_t* p, double lo, double hi) {
    double v = 0;
    if (scanKind == 1) v = *p;
    else if (scanKind == 4) { int32_t x; std::memcpy(&x, p, 4); v = x; }
    else { float x; std::memcpy(&x, p, 4); if (!std::isfinite(x)) return false; v = x; }
    return v >= lo && v <= hi;
}

int lScanStart(lua_State* L) {
    scanKind = static_cast<int>(luaL_checkinteger(L, 1));
    double lo = luaL_checknumber(L, 2), hi = luaL_checknumber(L, 3);
    size_t step = scanKind == 1 ? 1 : 4, size = scanKind == 1 ? 1 : 4;
    scanHits.clear();
    MEMORY_BASIC_INFORMATION mbi{};
    uintptr_t at = 0x10000;
    while (scanHits.size() < 50'000'000 && VirtualQuery(reinterpret_cast<void*>(at), &mbi, sizeof(mbi))) {
        uintptr_t next = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
        bool ok = mbi.State == MEM_COMMIT && mbi.Type == MEM_PRIVATE && !(mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) &&
                  (mbi.Protect & (PAGE_READWRITE | PAGE_WRITECOPY));
        if (ok) {
            walk(reinterpret_cast<uintptr_t>(mbi.BaseAddress), mbi.RegionSize, size, [&](uintptr_t base, const uint8_t* data, size_t count, size_t span) {
                for (size_t i = 0; i + size <= count && i < span; i += step)
                    if (scanMatch(data + i, lo, hi)) scanHits.push_back(base + i);
                return false;
            });
        }
        if (next <= at) break;
        at = next;
    }
    lua_pushinteger(L, static_cast<lua_Integer>(scanHits.size()));
    return 1;
}

int lScanNext(lua_State* L) {
    double lo = luaL_checknumber(L, 1), hi = luaL_checknumber(L, 2);
    std::vector<uintptr_t> keep;
    uint8_t buf[4];
    for (uintptr_t a : scanHits)
        if (readMem(a, buf, scanKind == 1 ? 1 : 4) && scanMatch(buf, lo, hi)) keep.push_back(a);
    scanHits.swap(keep);
    lua_pushinteger(L, static_cast<lua_Integer>(scanHits.size()));
    return 1;
}

int lScanList(lua_State* L) {
    size_t n = std::min<size_t>(scanHits.size(), static_cast<size_t>(luaL_optinteger(L, 1, 64)));
    return pushList(L, std::vector<uintptr_t>(scanHits.begin(), scanHits.begin() + n));
}

// 8-byte values in [lo, hi), in private writable memory: everything that points into an object
int lHeapRange(lua_State* L) {
    uint64_t lo = static_cast<uint64_t>(luaL_checkinteger(L, 1)), hi = static_cast<uint64_t>(luaL_checkinteger(L, 2));
    size_t limit = static_cast<size_t>(luaL_optinteger(L, 3, 64));
    std::vector<uintptr_t> hits;
    MEMORY_BASIC_INFORMATION mbi{};
    uintptr_t at = 0x10000;
    while (hits.size() < limit && VirtualQuery(reinterpret_cast<void*>(at), &mbi, sizeof(mbi))) {
        uintptr_t next = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
        bool ok = mbi.State == MEM_COMMIT && mbi.Type == MEM_PRIVATE && !(mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) &&
                  (mbi.Protect & (PAGE_READWRITE | PAGE_WRITECOPY));
        if (ok) {
            walk(reinterpret_cast<uintptr_t>(mbi.BaseAddress), mbi.RegionSize, 8, [&](uintptr_t base, const uint8_t* data, size_t count, size_t span) {
                for (size_t i = 0; i + 8 <= count && i < span; i += 8) {
                    uint64_t v;
                    std::memcpy(&v, data + i, 8);
                    if (v >= lo && v < hi) hits.push_back(base + i);
                    if (hits.size() >= limit) return true;
                }
                return false;
            });
        }
        if (next <= at) break;
        at = next;
    }
    return pushList(L, hits);
}

// three consecutive 32-bit integers inside the given ranges (block positions), in private writable memory
int lHeapInts(lua_State* L) {
    int32_t lo[3], hi[3];
    for (int i = 0; i < 3; i++) {
        lo[i] = static_cast<int32_t>(luaL_checkinteger(L, 1 + i * 2));
        hi[i] = static_cast<int32_t>(luaL_checkinteger(L, 2 + i * 2));
    }
    size_t limit = static_cast<size_t>(luaL_optinteger(L, 7, 32));
    std::vector<uintptr_t> hits;
    MEMORY_BASIC_INFORMATION mbi{};
    uintptr_t at = 0x10000;
    while (hits.size() < limit && VirtualQuery(reinterpret_cast<void*>(at), &mbi, sizeof(mbi))) {
        uintptr_t next = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
        bool ok = mbi.State == MEM_COMMIT && mbi.Type == MEM_PRIVATE && !(mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) &&
                  (mbi.Protect & (PAGE_READWRITE | PAGE_WRITECOPY));
        if (ok) {
            walk(reinterpret_cast<uintptr_t>(mbi.BaseAddress), mbi.RegionSize, 12, [&](uintptr_t base, const uint8_t* data, size_t count, size_t span) {
                for (size_t i = 0; i + 12 <= count && i < span; i += 4) {
                    int32_t v[3];
                    std::memcpy(v, data + i, 12);
                    if (v[0] >= lo[0] && v[0] <= hi[0] && v[1] >= lo[1] && v[1] <= hi[1] && v[2] >= lo[2] && v[2] <= hi[2])
                        hits.push_back(base + i);
                    if (hits.size() >= limit) return true;
                }
                return false;
            });
        }
        if (next <= at) break;
        at = next;
    }
    return pushList(L, hits);
}

// offsets inside [addr, addr+len) where count consecutive floats match the given values within tol
int lScanFloats(lua_State* L) {
    uintptr_t at = arg(L, 1);
    size_t len = std::min<size_t>(static_cast<size_t>(luaL_checkinteger(L, 2)), 1u << 20);
    float tol = static_cast<float>(luaL_checknumber(L, 3));
    int count = lua_gettop(L) - 3;
    if (count < 1 || count > 8) return luaL_error(L, "1 to 8 values");
    float want[8];
    for (int i = 0; i < count; i++) want[i] = static_cast<float>(luaL_checknumber(L, 4 + i));
    std::vector<uint8_t> buf(len);
    if (!readMem(at, buf.data(), len)) {
        lua_newtable(L);
        return 1;
    }
    std::vector<uintptr_t> hits;
    for (size_t i = 0; i + count * 4 <= len; i += 4) {
        bool ok = true;
        for (int k = 0; k < count && ok; k++) {
            float v;
            std::memcpy(&v, buf.data() + i + k * 4, 4);
            ok = std::fabs(v - want[k]) <= tol;
        }
        if (ok) hits.push_back(i);
    }
    return pushList(L, hits);
}

// {offset, value} for every float in the block within [lo, hi], skipping values too small to be an angle
int lFloatsIn(lua_State* L) {
    uintptr_t at = arg(L, 1);
    size_t len = std::min<size_t>(static_cast<size_t>(luaL_checkinteger(L, 2)), 1u << 16);
    float lo = static_cast<float>(luaL_checknumber(L, 3)), hi = static_cast<float>(luaL_checknumber(L, 4));
    std::vector<uint8_t> buf(len);
    lua_newtable(L);
    if (!readMem(at, buf.data(), len)) return 1;
    int out = 1;
    for (size_t i = 0; i + 4 <= len; i += 4) {
        float v;
        std::memcpy(&v, buf.data() + i, 4);
        if (!(v >= lo && v <= hi) || std::fabs(v) < 0.01f) continue;
        lua_pushinteger(L, static_cast<lua_Integer>(i));
        lua_rawseti(L, -2, out++);
        lua_pushnumber(L, v);
        lua_rawseti(L, -2, out++);
    }
    return 1;
}

// offsets and values of 8-byte values in the block that point into readable private memory
int lPointers(lua_State* L) {
    uintptr_t at = arg(L, 1);
    size_t len = std::min<size_t>(static_cast<size_t>(luaL_checkinteger(L, 2)), 1u << 16);
    std::vector<uint8_t> buf(len);
    lua_newtable(L);
    if (!readMem(at, buf.data(), len)) return 1;
    int out = 1;
    for (size_t i = 0; i + 8 <= len; i += 8) {
        uintptr_t v;
        std::memcpy(&v, buf.data() + i, 8);
        if (v < 0x10000 || v > 0x7FFFFFFFFFFF || (v & 7)) continue;
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery(reinterpret_cast<void*>(v), &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT || mbi.Type != MEM_PRIVATE) continue;
        if (!(mbi.Protect & (PAGE_READWRITE | PAGE_READONLY | PAGE_WRITECOPY)) || (mbi.Protect & PAGE_GUARD)) continue;
        lua_newtable(L);
        lua_pushinteger(L, static_cast<lua_Integer>(i));
        lua_rawseti(L, -2, 1);
        push(L, v);
        lua_rawseti(L, -2, 2);
        lua_rawseti(L, -2, out++);
    }
    return 1;
}

// only functions of the game's own code can be called, with plain integer arguments
int lCall(lua_State* L) {
    using Fn = uintptr_t(__fastcall*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
    uintptr_t target = arg(L, 1);
    if (!inCode(target)) return luaL_error(L, "not a game function");
    auto fn = reinterpret_cast<Fn>(target);
    uintptr_t a = static_cast<uintptr_t>(luaL_optinteger(L, 2, 0));
    uintptr_t b = static_cast<uintptr_t>(luaL_optinteger(L, 3, 0));
    uintptr_t c = static_cast<uintptr_t>(luaL_optinteger(L, 4, 0));
    uintptr_t d = static_cast<uintptr_t>(luaL_optinteger(L, 5, 0));
    uintptr_t result = 0;
    __try {
        result = fn(a, b, c, d);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return luaL_error(L, "exception %08x", static_cast<unsigned>(GetExceptionCode()));
    }
    push(L, result);
    return 1;
}

// same as call, but returns xmm0 as a float (getters that return float)
int lCallF(lua_State* L) {
    using Fn = float(__fastcall*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
    uintptr_t target = arg(L, 1);
    if (!inCode(target)) return luaL_error(L, "not a game function");
    auto fn = reinterpret_cast<Fn>(target);
    float result = 0.f;
    __try {
        result = fn(static_cast<uintptr_t>(luaL_optinteger(L, 2, 0)), static_cast<uintptr_t>(luaL_optinteger(L, 3, 0)),
                    static_cast<uintptr_t>(luaL_optinteger(L, 4, 0)), static_cast<uintptr_t>(luaL_optinteger(L, 5, 0)));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return luaL_error(L, "exception %08x", static_cast<unsigned>(GetExceptionCode()));
    }
    lua_pushnumber(L, result);
    return 1;
}

csh capstone() {
    static csh handle = 0;
    if (!handle) {
        cs_open(CS_ARCH_X86, CS_MODE_64, &handle);
        cs_option(handle, CS_OPT_DETAIL, CS_OPT_ON);
    }
    return handle;
}

std::string describe(uintptr_t target) {
    char buf[96]{};
    char out[48];
    if (inCode(target)) {
        std::snprintf(out, sizeof(out), " -> fn %08llx", static_cast<unsigned long long>(target - imageBase()));
        return out;
    }
    if (!readMem(target, buf, sizeof(buf) - 1)) return {};
    int n = 0;
    while (n < 80 && buf[n] >= 0x20 && buf[n] < 0x7f) n++;
    if (n >= 4 && buf[n] == 0) return std::string(" -> \"") + std::string(buf, n) + "\"";
    int w = 0;
    while (w < 40 && buf[w * 2] >= 0x20 && buf[w * 2] < 0x7f && buf[w * 2 + 1] == 0) w++;
    if (w >= 4 && buf[w * 2] == 0 && buf[w * 2 + 1] == 0) {
        std::string s;
        for (int i = 0; i < w; i++) s += buf[i * 2];
        return " -> L\"" + s + "\"";
    }
    std::snprintf(out, sizeof(out), " -> data %08llx", static_cast<unsigned long long>(target - imageBase()));
    return out;
}

std::string listing(uintptr_t start, size_t bytes, int maxInsns) {
    std::vector<uint8_t> code(bytes);
    size_t got = bytes;
    while (got && !readMem(start, code.data(), got)) got /= 2;
    std::string out;
    cs_insn* insn = cs_malloc(capstone());
    const uint8_t* p = code.data();
    size_t left = got;
    uint64_t addr = start;
    for (int i = 0; i < maxInsns && cs_disasm_iter(capstone(), &p, &left, &addr, insn); i++) {
        char line[256];
        int n = std::snprintf(line, sizeof(line), "%08llx  ", static_cast<unsigned long long>(insn->address - imageBase()));
        for (int b = 0; b < insn->size && b < 10; b++) n += std::snprintf(line + n, sizeof(line) - n, "%02x", insn->bytes[b]);
        for (int b = insn->size; b < 10; b++) n += std::snprintf(line + n, sizeof(line) - n, "  ");
        std::snprintf(line + n, sizeof(line) - n, "  %s %s", insn->mnemonic, insn->op_str);
        out += line;

        auto& x86 = insn->detail->x86;
        for (int o = 0; o < x86.op_count; o++) {
            auto& op = x86.operands[o];
            uintptr_t target = 0;
            if (op.type == X86_OP_MEM && op.mem.base == X86_REG_RIP) target = insn->address + insn->size + op.mem.disp;
            else if (op.type == X86_OP_IMM && (cs_insn_group(capstone(), insn, X86_GRP_CALL) || cs_insn_group(capstone(), insn, X86_GRP_JUMP)))
                target = static_cast<uintptr_t>(op.imm);
            if (target) {
                out += describe(target);
                break;
            }
        }
        out += '\n';
    }
    cs_free(insn, 1);
    return out;
}

int lDisasm(lua_State* L) {
    int count = static_cast<int>(std::min<lua_Integer>(luaL_optinteger(L, 2, 40), 2000));
    std::string text = listing(arg(L, 1), 15u * static_cast<size_t>(count), count);
    lua_pushstring(L, text.c_str());
    return 1;
}

int lDisFunc(lua_State* L) {
    DWORD64 base = 0;
    auto* entry = RtlLookupFunctionEntry(arg(L, 1), &base, nullptr);
    if (!entry) return luaL_error(L, "no function entry");
    size_t size = entry->EndAddress - entry->BeginAddress;
    std::string text = listing(static_cast<uintptr_t>(base + entry->BeginAddress), std::min<size_t>(size, 24000), 6000);
    lua_pushstring(L, text.c_str());
    return 1;
}

int lSleep(lua_State* L) {
    Sleep(static_cast<DWORD>(std::min<lua_Integer>(luaL_checkinteger(L, 1), 10000)));
    return 0;
}

constexpr size_t watchMax = 4096;
constexpr int watchDepth = 8;
std::atomic<size_t> watchCount{0};
uintptr_t watchHits[watchMax];
uintptr_t watchStacks[watchMax][watchDepth];

void unwind(const CONTEXT& from, uintptr_t* out) {
    CONTEXT c = from;
    for (int k = 0; k < watchDepth; k++) {
        out[k] = 0;
        DWORD64 base = 0;
        auto* fn = RtlLookupFunctionEntry(c.Rip, &base, nullptr);
        if (!fn) break;
        void* handler = nullptr;
        DWORD64 frame = 0;
        RtlVirtualUnwind(UNW_FLAG_NHANDLER, base, c.Rip, fn, &c, &handler, &frame, nullptr);
        if (!c.Rip) break;
        out[k] = static_cast<uintptr_t>(c.Rip);
    }
}

LONG CALLBACK onWatch(EXCEPTION_POINTERS* e) {
    if (e->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP || !(e->ContextRecord->Dr6 & 1)) return EXCEPTION_CONTINUE_SEARCH;
    size_t i = watchCount.fetch_add(1);
    if (i < watchMax) {
        watchHits[i] = reinterpret_cast<uintptr_t>(e->ExceptionRecord->ExceptionAddress);
        unwind(*e->ContextRecord, watchStacks[i]);
    }
    e->ContextRecord->Dr6 = 0;
    return EXCEPTION_CONTINUE_EXECUTION;
}

bool watchWrites = false;

void armThreads(uintptr_t address, bool on) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    THREADENTRY32 te{sizeof(te)};
    for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te)) {
        if (te.th32OwnerProcessID != GetCurrentProcessId() || te.th32ThreadID == GetCurrentThreadId()) continue;
        HANDLE t = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
        if (!t) continue;
        if (SuspendThread(t) != DWORD(-1)) {
            CONTEXT c{};
            c.ContextFlags = CONTEXT_DEBUG_REGISTERS;
            if (GetThreadContext(t, &c)) {
                // slot 0, break on read or write (RW0 = 11) or only on write (01), four bytes (LEN0 = 11)
                c.Dr0 = on ? address : 0;
                c.Dr7 = on ? (c.Dr7 & ~0xF0003ull) | 1ull | ((watchWrites ? 1ull : 3ull) << 16) | (3ull << 18) : c.Dr7 & ~0xF0003ull;
                SetThreadContext(t, &c);
            }
            ResumeThread(t);
        }
        CloseHandle(t);
    }
    CloseHandle(snap);
}

// instructions right after every access to a 4-byte value during the given time (hardware breakpoint, dev only)
// returns rip, count, "caller,caller,..." triples; a true third argument watches writes only
int lWatch(lua_State* L) {
    uintptr_t address = arg(L, 1);
    DWORD ms = static_cast<DWORD>(std::clamp<lua_Integer>(luaL_optinteger(L, 2, 2000), 50, 10000));
    watchWrites = lua_toboolean(L, 3);
    watchCount = 0;
    PVOID veh = AddVectoredExceptionHandler(1, onWatch);
    armThreads(address, true);
    Sleep(ms);
    armThreads(address, false);
    Sleep(50);
    RemoveVectoredExceptionHandler(veh);
    size_t n = std::min(watchCount.load(), watchMax);
    std::vector<size_t> order(n);
    for (size_t i = 0; i < n; i++) order[i] = i;
    std::stable_sort(order.begin(), order.end(), [](size_t a, size_t b) { return watchHits[a] < watchHits[b]; });
    lua_newtable(L);
    int out = 1;
    for (size_t i = 0; i < n;) {
        size_t j = i;
        while (j < n && watchHits[order[j]] == watchHits[order[i]]) j++;
        std::string stack;
        for (int k = 0; k < watchDepth && watchStacks[order[i]][k]; k++) {
            char tmp[24];
            std::snprintf(tmp, sizeof(tmp), "%s%llx", k ? "," : "", static_cast<unsigned long long>(watchStacks[order[i]][k]));
            stack += tmp;
        }
        lua_pushinteger(L, static_cast<lua_Integer>(watchHits[order[i]]));
        lua_rawseti(L, -2, out++);
        lua_pushinteger(L, static_cast<lua_Integer>(j - i));
        lua_rawseti(L, -2, out++);
        lua_pushstring(L, stack.c_str());
        lua_rawseti(L, -2, out++);
        i = j;
    }
    return 1;
}

int lLog(lua_State* L) {
    int n = lua_gettop(L);
    std::string s;
    for (int i = 1; i <= n; i++) {
        size_t len = 0;
        const char* t = luaL_tolstring(L, i, &len);
        if (i > 1) s += ' ';
        s.append(t, len);
        lua_pop(L, 1);
    }
    logger::info("explore: {}", s);
    return 0;
}

bool bareName(const std::string& name) {
    if (name.empty() || name.size() > 64) return false;
    for (char c : name)
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-' && c != '.') return false;
    return name.find("..") == std::string::npos;
}

int lOut(lua_State* L) {
    std::string name = luaL_checkstring(L, 1);
    size_t len = 0;
    const char* text = luaL_checklstring(L, 2, &len);
    if (!bareName(name)) return luaL_error(L, "bad file name");
    std::error_code ec;
    fs::create_directories(outDir, ec);
    std::ofstream(outDir / name, std::ios::binary | std::ios::trunc).write(text, static_cast<std::streamsize>(len));
    return 0;
}

int lRun(lua_State* L) {
    std::string name = luaL_checkstring(L, 1);
    if (!bareName(name)) return luaL_error(L, "bad script name");
    auto file = (scriptDir / (name + ".lua")).string();
    if (luaL_loadfile(L, file.c_str()) != LUA_OK || lua_pcall(L, 0, 0, 0) != LUA_OK) return lua_error(L);
    return 0;
}

void work(std::string name) {
    lua_State* L = luaL_newstate();
    lua_sethook(L, [](lua_State* s, lua_Debug*) { if (stopping) luaL_error(s, "stopped"); }, LUA_MASKCOUNT, 1000);
    luaL_requiref(L, "_G", luaopen_base, 1);
    luaL_requiref(L, LUA_STRLIBNAME, luaopen_string, 1);
    luaL_requiref(L, LUA_TABLIBNAME, luaopen_table, 1);
    luaL_requiref(L, LUA_MATHLIBNAME, luaopen_math, 1);
    lua_settop(L, 0);
    for (auto* banned : {"dofile", "loadfile", "load", "require", "collectgarbage"}) {
        lua_pushnil(L);
        lua_setglobal(L, banned);
    }

    static const luaL_Reg api[] = {
        {"base", lBase},       {"sections", lSections}, {"hex", lHex},         {"u8", readValue<uint8_t>},
        {"u16", readValue<uint16_t>}, {"u32", readValue<uint32_t>}, {"u64", readValue<uint64_t>},
        {"i32", readValue<int32_t>},  {"f32", readValue<float>},    {"f64", readValue<double>}, {"wf32", lWriteF32}, {"raw", lRaw},
        {"cstr", lCstr},       {"find", lFind},         {"findd", lFindData},  {"bytes", lBytes},
        {"xrefs", lXrefs},     {"callers", lCallers},   {"func", lFunc},       {"vtable", lVtable},
        {"heap", lHeap},       {"heapf", lHeapFloats}, {"heapi", lHeapInts}, {"heapr", lHeapRange}, {"scanstart", lScanStart}, {"scannext", lScanNext}, {"scanlist", lScanList},  {"call", lCall}, {"callf", lCallF}, {"scanf", lScanFloats}, {"floatsin", lFloatsIn},{"pointers", lPointers},      {"disasm", lDisasm},   {"disfunc", lDisFunc},
        {"sleep", lSleep},     {"watch", lWatch},     {"log", lLog},           {"out", lOut},         {"run", lRun},
        {nullptr, nullptr}};
    luaL_newlib(L, api);
    lua_setglobal(L, "rt");
    lua_pushcfunction(L, lLog);
    lua_setglobal(L, "print");

    lua_pushstring(L, name.c_str());
    lua_setglobal(L, "SCRIPT");
    lua_getglobal(L, "rt");
    lua_getfield(L, -1, "run");
    lua_pushstring(L, name.c_str());
    if (lua_pcall(L, 1, 0, 0) != LUA_OK) logger::error("explore: {}", lua_tostring(L, -1));
    lua_close(L);
    busy = false;
}

}

void run(const std::string& name) {
    if (scriptDir.empty()) {
        std::ifstream in(paths::dllDir() / L"Monchi.explore");
        std::string dir;
        if (!in || !std::getline(in, dir) || dir.empty()) {
            logger::warn("explore: no Monchi.explore marker next to the dll");
            return;
        }
        while (!dir.empty() && (dir.back() == '\r' || dir.back() == ' ')) dir.pop_back();
        scriptDir = dir;
        outDir = paths::root() / L"out";
    }
    if (name == "stop") {
        stop();
        return;
    }
    if (busy.exchange(true)) {
        logger::warn("explore: a script is still running");
        return;
    }
    stopping = false;
    std::thread(work, name).detach();
}

void stop() {
    if (!busy) return;
    stopping = true;
    // a long scan inside one call only notices the flag when it returns to Lua
    for (int i = 0; i < 300 && busy; i++) Sleep(50);
    logger::info("explore: {}", busy ? "script still busy" : "stopped");
}

}

#else

namespace explore {

void run(const std::string&) {}
void stop() {}

}

#endif
