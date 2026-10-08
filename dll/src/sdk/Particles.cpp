#include "Particles.hpp"
#include "core/Guard.hpp"
#include "core/Log.hpp"
#include "hook/Hook.hpp"
#include "sig/Sigs.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <map>
#include <mutex>

namespace particles {

namespace {

// HashedString as the game lays it out: FNV-1 (64 bit) of the text, the text as a std::string, a pointer it uses for
// comparisons (checked on 1.26.52.3: "minecraft:totem_particle" is built with hash 0x276279f51d3e1e78 at 0x476dea1).
struct Hashed {
    uint64_t hash;
    char text[16];
    size_t size;
    size_t capacity;
    const void* last;
};

// 1.26.52.3, 0x2130bc0: (level, out handle, const HashedString& name, const Vec3& position, MolangVariableMap by
// value). Sixteen callers, among them the one for emitters on entities (0x211b950) and the totem. It looks the effect
// up by name (0x21529c0) and creates it (0x216b830, which returns at once when the lookup found nothing). The
// variable map is destroyed by the function itself, so it must always run: an effect is kept from being created by
// handing it a name no effect has, not by skipping the call.
using Spawn = void* (*)(void*, void*, const Hashed*, const float*, void*);
Spawn original = nullptr;
std::atomic<bool> wanted{false};
std::atomic<bool> installed{false};
bool tried = false;

std::mutex lock;
std::map<std::string, Seen> names;
std::vector<std::string> blocked;
bool blockAll = false;
std::atomic<unsigned> total{0};

constexpr uint64_t fnv1(const char* s) {
    uint64_t h = 0xcbf29ce484222325ull;
    for (; *s; ++s) h = (h * 0x100000001b3ull) ^ uint64_t(static_cast<unsigned char>(*s));
    return h;
}

constexpr char nothing[] = "monchi:none";
const Hashed none{fnv1(nothing), "monchi:none", sizeof(nothing) - 1, 15, nullptr};

// reads the name out of the game's string without treating it as one of ours
bool nameOf(const Hashed* h, char* out, size_t room) {
    __try {
        size_t n = h->size;
        if (!n || n >= room || h->capacity < n) return false;
        const char* from = h->capacity > 15 ? *reinterpret_cast<const char* const*>(h->text) : h->text;
        if (!from) return false;
        std::memcpy(out, from, n);
        out[n] = 0;
        return true;
    } __except (1) {
        return false;
    }
}

bool decide(const char* name) {
    std::scoped_lock g(lock);
    if (names.size() > 400) names.clear();
    auto& s = names[name];
    if (s.name.empty()) s.name = name;
    s.asked++;
    bool stop = blockAll || std::find(blocked.begin(), blocked.end(), s.name) != blocked.end();
    if (stop) s.blocked++;
    return stop;
}

void* spawn(void* level, void* out, const Hashed* name, const float* at, void* variables) {
    if (wanted.load(std::memory_order_relaxed) && name) {
        char text[128];
        bool stop = false;
        if (nameOf(name, text, sizeof(text))) guard::call("particle filter", [&] { stop = decide(text); });
        if (stop) {
            total.fetch_add(1, std::memory_order_relaxed);
            name = &none;
        }
    }
    return original(level, out, name, at, variables);
}

}

bool use(bool on) {
    wanted = on;
    if (!on || installed) return installed;
    if (tried) return false;
    tried = true;
    uintptr_t target = sigs::address("ParticleEffect");
    if (!target) return false;
    if (!hook::create("ParticleEffect", reinterpret_cast<void*>(target), spawn, &original)) return false;
    hook::enableAll();
    installed = true;
    logger::info("particles: named effects go through the filter");
    return true;
}

bool hooked() { return installed; }

void block(std::vector<std::string> list, bool all) {
    std::scoped_lock g(lock);
    if (blockAll == all && blocked == list) return;
    blocked = std::move(list);
    blockAll = all;
}

std::vector<Seen> seen() {
    std::scoped_lock g(lock);
    std::vector<Seen> out;
    for (auto& [name, s] : names) out.push_back(s);
    std::sort(out.begin(), out.end(), [](const Seen& a, const Seen& b) { return a.asked > b.asked; });
    return out;
}

unsigned blockedTotal() { return total.load(); }

}
