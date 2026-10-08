// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Hook/Hooks/Render/BgfxFrameExtractorInsertHook.cpp. The public build reads the overlay
// colour from xmm0 or xmm8 where the sigs match; on 1.26.52 the colour is already stored into a local array of the
// extractor's frame (static, 0x5ee97ca and the three neighbours), so each hook sits behind those four stores and edits
// the array through rbp. The address is the cached sig plus a fixed distance and is only hooked when the instruction
// found there is the one read in the image.
#include "BgfxFrameExtractorInsertHook.hpp"
#include "../../../../Utils/Memory/Game/SignatureAndOffsetManager.hpp"
#include "../../../../Utils/Memory/Memory.hpp"
#include <safetyhook.hpp>
#include <cstring>
#include <atomic>
#include <cmath>
#include "Module/Manager.hpp"

namespace {

SafetyHookMid legacyHook;
SafetyHookMid batchedHook;
SafetyHookMid actorHook;
SafetyHookMid materialHook;

// Hurt overlays are red with positive alpha; white and other material overlays are left alone.
template<ptrdiff_t Disp>
void recolorBody(SafetyHookContext &ctx) {
    auto module = ModuleManager::getModule("Hurt Color");
    if (!ctx.rbp || !module || !module->isEnabled()) return;
    // The actor shader identifies self versus opponents; do not recolor its input first.
    if (GET_SIG_ADDRESS("ActorShaderManager::setupShaderParameter")) return;
    auto* target = reinterpret_cast<MCCColor*>(ctx.rbp + Disp);
    MCCColor color = *target;
    if (color.r != 1.f || color.g != 0.f || color.b != 0.f || !(color.a > 0.f && color.a <= 1.f)) return;
    auto event = nes::make_holder<HurtColorEvent>(&color);
    eventMgr.trigger(event);
    if (!std::isfinite(color.r) || !std::isfinite(color.g) || !std::isfinite(color.b) || !std::isfinite(color.a)) return;
    color.r = std::clamp(color.r, 0.f, 1.f);
    color.g = std::clamp(color.g, 0.f, 1.f);
    color.b = std::clamp(color.b, 0.f, 1.f);
    color.a = std::clamp(color.a, 0.f, 1.f);
    *target = color;
}

template<ptrdiff_t Disp>
bool recolorGuarded(SafetyHookContext& ctx) {
    __try {
        recolorBody<Disp>(ctx);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

template<ptrdiff_t Disp>
void recolor(SafetyHookContext& ctx) {
    static std::atomic<bool> failed{false};
    if (failed || recolorGuarded<Disp>(ctx)) return;
    if (!failed.exchange(true)) Logger::warn("Hurt Color: overlay hook faulted and is bypassed for this session");
}

template<size_t N>
bool install(SafetyHookMid &slot, uintptr_t sig, ptrdiff_t distance, const uint8_t (&expected)[N], safetyhook::MidHookFn callback, const char *name) {
    if (!sig) return false;
    uintptr_t at = sig + distance;
    if (std::memcmp(reinterpret_cast<void *>(at), expected, N) != 0) {
        Logger::custom(fg(fmt::color::crimson), "Hook", "{}: unexpected code at the hook address", name);
        return false;
    }
    slot = safetyhook::create_mid(at, callback);
    if (!slot) {
        Logger::custom(fg(fmt::color::crimson), "Hook", "Failed to hook {}", name);
        return false;
    }
    Logger::custom(fg(fmt::color::deep_sky_blue), "Hook", "Hooked {}", name);
    return true;
}

}

void BgfxFrameExtractorInsertHook::enableHook() {
    // mov qword [rbp+0x808], 1 behind the stores into [rbp+0xaa0]
    static constexpr uint8_t legacy[] = {0x48, 0xC7, 0x85, 0x08, 0x08, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00};
    // mov rcx, [rbp+0x7f0] behind the stores into [rbp+0x4c0]
    static constexpr uint8_t batched[] = {0x48, 0x8B, 0x8D, 0xF0, 0x07, 0x00, 0x00};
    // mov qword [rbp+0x1820], 1 behind the stores into [rbp+0x1bc8]
    static constexpr uint8_t actor[] = {0x48, 0xC7, 0x85, 0x20, 0x18, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00};
    // mov qword [rbp+0x258], 1 behind the stores into [rbp+0x330]
    static constexpr uint8_t material[] = {0x48, 0xC7, 0x85, 0x58, 0x02, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00};

    install(legacyHook, GET_SIG_ADDRESS("BgfxFrameExtractor::_insertWriteOverlayUniform"), 0x44, legacy, recolor<0xAA0>, "BgfxFrameExtractor::_insertWriteOverlayUniform");
    install(batchedHook, address, 0x46, batched, recolor<0x4C0>, "BgfxFrameExtractor::_insertWriteOverlayUniformBatched");
    install(actorHook, GET_SIG_ADDRESS("BgfxFrameExtractor::_insertWriteOverlayUniformActor2630"), 0x4D, actor, recolor<0x1BC8>, "BgfxFrameExtractor::_insertWriteOverlayUniformActor2630");
    install(materialHook, GET_SIG_ADDRESS("BgfxFrameExtractor::_insertWriteOverlayUniformMaterial2630"), 0x23, material, recolor<0x330>, "BgfxFrameExtractor::_insertWriteOverlayUniformMaterial2630");
}

BgfxFrameExtractorInsertHook::BgfxFrameExtractorInsertHook()
        : Hook(
        "BgfxFrameExtractor::_insertWriteOverlayUniform",
        GET_SIG_ADDRESS("BgfxFrameExtractor::_insertWriteOverlayUniformBatched")
) {}
