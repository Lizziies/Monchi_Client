#include "hook/FreeCamera.hpp"
#include "hook/OwnNametag.hpp"
#include "core/Guard.hpp"
#include "sdk/Memory.hpp"
#include "sig/Sigs.hpp"
#include "sig/Version.hpp"
#include "modules/common/Text.hpp"

#include <array>
#include <cstdlib>
#include <iostream>

namespace {
uintptr_t gate = 0;
bool cameraFound = false, enableSucceeds = false;
std::array<uint8_t, 4> bodyBytes{0xf3, 0x0f, 0x11, 0x00};
std::array<uint8_t, 6> headBytes{0xf3, 0x44, 0x0f, 0x11, 0x08, 0x7f};
int calls = 0;
using Update = void (*)(void*, void*, void*);
Update detour = nullptr;
void original(void* a, void* b, void* c) {
    if (a != reinterpret_cast<void*>(1) || b != reinterpret_cast<void*>(2) || c != reinterpret_cast<void*>(3)) std::abort();
    ++calls;
}
void check(bool ok, const char* message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(1); }
}
}

namespace sigs {
uintptr_t address(const std::string& name) {
    if (name == "OwnNametagGate") return gate;
    if (!cameraFound) return 0;
    if (name == "CameraYaw") return reinterpret_cast<uintptr_t>(bodyBytes.data());
    if (name == "CameraHeadYaw") return reinterpret_cast<uintptr_t>(headBytes.data());
    return 0x10000;
}
}
namespace hook {
bool create(const char*, void*, void* callback, void** previous) {
    detour = reinterpret_cast<Update>(callback);
    *previous = reinterpret_cast<void*>(original);
    return true;
}
bool enableAll() { return enableSucceeds; }
}
namespace logger {
void write(std::string_view, std::string_view) {}
}
namespace guard {
bool run(const char*, void (*fn)(void*), void* context) { fn(context); return true; }
void report(const char*, const char*) { std::abort(); }
}

int main() {
    const std::string section = "\xc2\xa7";
    const ImU32 base = IM_COL32(255, 255, 255, 80);
    auto tag = text::colored(section + "d" + section + "lPrincess" + section + "r Player", base);
    check(tag.size() == 2 && tag[0].text == "Princess" && tag[1].text == " Player", "format markers must not become HUD glyphs");
    check(tag[0].color == IM_COL32(255, 85, 255, 80) && tag[0].bold, "server title must retain its color, weight and alpha");
    check(tag[1].color == base && !tag[1].bold && !tag[1].italic, "reset must restore the base style");
    auto styled = text::colored(section + "l" + section + "o" + section + "aName", base);
    check(styled.size() == 1 && styled[0].bold && styled[0].italic, "Bedrock color changes must preserve text styles");
    auto material = text::colored(section + "mR" + section + "nC", base);
    check(material.size() == 2 && material[0].color == IM_COL32(151, 22, 7, 80) && material[1].color == IM_COL32(180, 104, 77, 80), "Bedrock material color codes");
    check(sigs::supportedVersion({"1.26.0", "1.26.52", "1.26"}, {"1.26.52"}) == "1.26.52", "known packaged version must match");
    check(sigs::supportedVersion({"1.26.60"}, {"1.26.52"}).empty(), "a future game version must not use older offsets");
    check(sigs::supportedVersion({"1.26.10"}, {"1.26.52"}).empty(), "an older game version must not use newer offsets");
    check(sigs::supportedVersion({}, {"1.26.52"}).empty(), "unknown game version must not use arbitrary offsets");
    check(!freecam::set(true), "missing camera hook must refuse activation");
    cameraFound = true;
    check(!freecam::set(true), "failed hook enable must refuse activation");
    detour(reinterpret_cast<void*>(1), reinterpret_cast<void*>(2), reinterpret_cast<void*>(3));
    check(calls == 1, "failed enable must preserve native update and arguments");
    enableSucceeds = true;
    check(freecam::set(true), "hook enable must be retried after failure");
    check(bodyBytes[0] == 0x90 && bodyBytes[3] == 0x90 && headBytes[4] == 0x90 && headBytes[5] == 0x7f,
          "body and head stores must be patched at their complete instruction lengths");
    detour(reinterpret_cast<void*>(1), reinterpret_cast<void*>(2), reinterpret_cast<void*>(3));
    check(calls == 1, "active freelook must suppress native player rotation");
    freecam::set(false);
    check(bodyBytes == std::array<uint8_t, 4>{0xf3, 0x0f, 0x11, 0x00} &&
          headBytes == std::array<uint8_t, 6>{0xf3, 0x44, 0x0f, 0x11, 0x08, 0x7f}, "release must restore both angle stores");
    detour(reinterpret_cast<void*>(1), reinterpret_cast<void*>(2), reinterpret_cast<void*>(3));
    check(calls == 2, "release must resume native rotation");
    headBytes[0] = 0xcc;
    check(!freecam::set(true), "unexpected head-store bytes must refuse activation");
    check(bodyBytes[0] == 0xf3 && headBytes[0] == 0xcc, "failed head patch must roll back the body without overwriting another owner");
    detour(reinterpret_cast<void*>(1), reinterpret_cast<void*>(2), reinterpret_cast<void*>(3));
    check(calls == 3, "partial patch failure must not suppress native updates");

    check(!ownNametag::show(true), "missing nametag gate must refuse activation");
    gate = reinterpret_cast<uintptr_t>(VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    check(gate != 0, "test allocation");
    const std::array<uint8_t, 6> branch{0x0f, 0x84, 0x20, 0, 0, 0};
    const std::array<uint8_t, 6> nops{0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
    std::array<uint8_t, 6> current{};
    mem::write(gate, nops);
    check(!ownNametag::show(true), "another client's patch must not be adopted");
    mem::write(gate, branch);
    DWORD previous = 0;
    VirtualProtect(reinterpret_cast<void*>(gate), 4096, PAGE_EXECUTE_READ, &previous);
    check(ownNametag::show(true), "native gate must open");
    mem::read(gate, current);
    check(current == nops, "gate bytes");
    ownNametag::show(false);
    mem::read(gate, current);
    check(current == branch, "disable must restore original branch");
    MEMORY_BASIC_INFORMATION region{};
    VirtualQuery(reinterpret_cast<void*>(gate), &region, sizeof(region));
    check(region.Protect == PAGE_EXECUTE_READ, "patch must restore page protection");
    check(ownNametag::show(true), "reactivation");
    auto changed = branch;
    changed[0] = 0xcc;
    mem::nextFrame();
    mem::write(gate, changed);
    check(!ownNametag::show(true), "external changes must invalidate the active patch");
    ownNametag::show(false);
    mem::read(gate, current);
    check(current == changed, "disable must not overwrite another owner's change");

    mem::nextFrame();
    mem::write(gate, branch);
    check(ownNametag::show(true), "gate opens again after the other owner left");
    VirtualProtect(reinterpret_cast<void*>(gate), 4096, PAGE_NOACCESS, &previous);
    mem::nextFrame();
    ownNametag::show(false);
    VirtualProtect(reinterpret_cast<void*>(gate), 4096, PAGE_EXECUTE_READ, &previous);
    mem::nextFrame();
    mem::read(gate, current);
    check(current == nops, "a restore that could not run leaves the patch in place");
    ownNametag::show(false);
    mem::nextFrame();
    mem::read(gate, current);
    check(current == branch, "a failed restore must stay owned and succeed on the next try");
    VirtualFree(reinterpret_cast<void*>(gate), 0, MEM_RELEASE);
    std::cout << "native binding lifecycle checks passed\n";
}
