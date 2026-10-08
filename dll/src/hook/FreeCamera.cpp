// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial UpdatePlayerHook and Freelook; see vendor/flarial/UPSTREAM.json.
#include "FreeCamera.hpp"
#include "CodePatch.hpp"
#include "Hook.hpp"
#include "core/Guard.hpp"
#include "core/Log.hpp"
#include "sig/Sigs.hpp"

#include <atomic>

namespace freecam {
namespace {
using Update = void (*)(void*, void*, void*);
Update original = nullptr;
std::atomic<bool> detached{false};
bool installed = false;
uintptr_t yaw = 0, headYaw = 0;
constexpr std::array<uint8_t, 4> yawStore{0xf3, 0x0f, 0x11, 0x00};
constexpr std::array<uint8_t, 5> headStore{0xf3, 0x44, 0x0f, 0x11, 0x08};
constexpr std::array<uint8_t, 4> yawNops{0x90, 0x90, 0x90, 0x90};
constexpr std::array<uint8_t, 5> headNops{0x90, 0x90, 0x90, 0x90, 0x90};

void update(void* a, void* b, void* c) {
    guard::call("camera player update", [&] {
        if (!detached.load(std::memory_order_acquire)) original(a, b, c);
    });
}
}

using codePatch::Result;

// true once none of our patches is left; a failed restore stays ours and is tried again on the next call
bool restore() {
    if (yaw && codePatch::replace(yaw, yawNops, yawStore) != Result::Failed) yaw = 0;
    if (headYaw && codePatch::replace(headYaw, headNops, headStore) != Result::Failed) headYaw = 0;
    return !yaw && !headYaw;
}

bool set(bool active) {
    static bool warned = false;
    if (!active) {
        // the player update stays off while an angle store is still disabled, it would only half work
        if (!restore()) {
            if (!warned) logger::warn("freelook: the camera angle stores could not be restored yet, retrying");
            warned = true;
            return false;
        }
        warned = false;
        detached.store(false, std::memory_order_release);
        return false;
    }
    if (detached.load(std::memory_order_acquire)) return true;
    auto yawAt = sigs::address("CameraYaw");
    auto headAt = sigs::address("CameraHeadYaw");
    if (!yawAt || !headAt) return false;
    if (!installed) {
        auto address = sigs::address("CameraUpdatePlayer");
        if (!address) return false;
        if (!original && !hook::create("CameraUpdatePlayer", reinterpret_cast<void*>(address), update, &original)) return false;
        if (!hook::enableAll()) return false;
        installed = true;
    }
    // Bedrock 1.26's head-angle store includes a REX prefix and is five bytes,
    // whereas the body-angle store is four. Never split either instruction.
    detached.store(true, std::memory_order_release);
    if (codePatch::replace(yawAt, yawStore, yawNops) != Result::Applied) {
        detached.store(false, std::memory_order_release);
        return false;
    }
    yaw = yawAt;
    if (codePatch::replace(headAt, headStore, headNops) != Result::Applied) {
        if (restore()) detached.store(false, std::memory_order_release);
        return false;
    }
    headYaw = headAt;
    return true;
}
}
