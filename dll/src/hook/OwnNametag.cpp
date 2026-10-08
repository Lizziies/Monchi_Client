// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial NametagModifier; see vendor/flarial/UPSTREAM.json.
#include "OwnNametag.hpp"
#include "CodePatch.hpp"
#include "sdk/Memory.hpp"
#include "sig/Sigs.hpp"

#include <array>
#include <cstring>

namespace ownNametag {
namespace {
uintptr_t patched = 0;
std::array<uint8_t, 6> original{};
constexpr std::array<uint8_t, 6> visibleBytes{0x90, 0x90, 0x90, 0x90, 0x90, 0x90};

}

using codePatch::Result;

bool show(bool visible) {
    if (!visible) {
        // a restore that fails stays ours and is retried; bytes someone else changed are left to them
        if (patched && codePatch::replace(patched, visibleBytes, original) != Result::Failed) patched = 0;
        return false;
    }
    if (patched) {
        std::array<uint8_t, 6> current{};
        if (mem::read(patched, current) && current == visibleBytes) return true;
        patched = 0;
        return false;
    }
    uintptr_t at = sigs::address("OwnNametagGate");
    std::array<uint8_t, 6> bytes{};
    // The upstream signature starts at the six-byte conditional jump that skips self tags.
    // Refuse a different instruction or a gate already modified by another client.
    if (!at || !mem::read(at, bytes) || bytes[0] != 0x0f || bytes[1] != 0x84) return false;
    if (codePatch::replace(at, bytes, visibleBytes) != Result::Applied) return false;
    original = bytes;
    patched = at;
    return true;
}
}
