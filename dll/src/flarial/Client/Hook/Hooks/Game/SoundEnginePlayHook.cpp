// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Hook/Hooks/Game/SoundEnginePlayHook.cpp. The by-value params struct (layout from the
// wrapper at vtable slot 16 that builds it and from play itself): std::string name at +0x00, Vec3 position at +0x20,
// volume at +0x2c, pitch at +0x30. play destroys the string, so everything is copied out before the original runs.
#include "SoundEnginePlayHook.hpp"
#include "../../../../Utils/Memory/Game/SignatureAndOffsetManager.hpp"
#include "Events/Game/SoundEnginePlayEvent.hpp"
#include <cmath>

namespace {

struct SoundParams {
    std::string name;
    Vec3<float> pos {};
    float volume = 0;
    float pitch = 0;
};

bool readParams(const uint8_t *p, SoundParams &out) {
    if (!p) return false;
    __try {
        auto size = *reinterpret_cast<const uint64_t *>(p + 0x10);
        auto capacity = *reinterpret_cast<const uint64_t *>(p + 0x18);
        if (capacity < 0xf || size > capacity || size > 0x400) return false;
        auto *chars = capacity >= 0x10 ? *reinterpret_cast<const char *const *>(p) : reinterpret_cast<const char *>(p);
        if (!chars) return false;
        out.name.assign(chars, size);
        auto *xyz = reinterpret_cast<const float *>(p + 0x20);
        out.pos = Vec3<float>(xyz[0], xyz[1], xyz[2]);
        out.volume = *reinterpret_cast<const float *>(p + 0x2c);
        out.pitch = *reinterpret_cast<const float *>(p + 0x30);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

}

uint64_t SoundEnginePlayHook::callback(void* SoundEngine, void* params) {
    SoundParams sound;
    if (readParams(static_cast<const uint8_t *>(params), sound) && !sound.name.empty()) {
        if (!std::isfinite(sound.pos.x) || !std::isfinite(sound.pos.y) || !std::isfinite(sound.pos.z)) sound.pos = Vec3<float>();
        auto event = nes::make_holder<SoundEnginePlayEvent>(sound.name, sound.pos, sound.volume, sound.pitch);
        eventMgr.trigger(event);
    }
    return funcOriginal(SoundEngine, params);
}

SoundEnginePlayHook::SoundEnginePlayHook() : Hook("SoundEnginePlayHook", GET_SIG_ADDRESS("SoundEngine::play")) {}

void SoundEnginePlayHook::enableHook() {
    this->autoHook((void *) callback, (void **) &funcOriginal);
}
