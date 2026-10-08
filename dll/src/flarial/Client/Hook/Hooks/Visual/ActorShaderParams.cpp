// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Hook/Hooks/Visual/ActorShaderParams.cpp, see the header. Argument roles (static,
// ActorShaderManager::setupShaderParameter at 0x1eb5e30): arguments 4, 5, 7 and 6 reach the shader parameter writer 0x1eb70f0
// as the overlay colour (+0x10), the change colour (+0x30), the glint colour (+0x38) and the second change colour (+0x48),
// 8..11 are two uv offsets and two rotations, 12 the glint uv scale, 13 the uv animation, 14 the brightness, 15 is a flag the
// public build does not have and 16 the light emission.
#include "ActorShaderParams.hpp"
#include "Events/Render/ActorShaderParamsEvent.hpp"
#include "Module/Manager.hpp"
#include "Events/Render/HurtColorEvent.hpp"

namespace {

bool readColor(const void *p, float *out) {
    if (!p) return false;
    __try {
        auto *f = static_cast<const float *>(p);
        out[0] = f[0];
        out[1] = f[1];
        out[2] = f[2];
        out[3] = f[3];
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void editColors(MCCColor& co, MCCColor& cc, MCCColor& cg, void* entity, bool useGlint, bool useHurt) {
    if (useGlint) {
        auto event = nes::make_holder<ActorShaderParamsEvent>(&co, &cc, &cg);
        eventMgr.trigger(event);
    }
    if (useHurt && co.r == 1.f && co.g == 0.f && co.b == 0.f && co.a > 0.f) {
        auto* local = SDK::clientInstance ? SDK::clientInstance->getLocalPlayer() : nullptr;
        if (local && entity) {
            auto event = nes::make_holder<HurtColorEvent>(&co, true, entity == local);
            eventMgr.trigger(event);
        }
    }

}
bool guardedColors(MCCColor& co, MCCColor& cc, MCCColor& cg, void* entity, bool glint, bool hurt) {
    __try { editColors(co, cc, cg, entity, glint, hurt); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

void *pick(void *original, MCCColor *edited, const float *seen) {
    if (edited->r == seen[0] && edited->g == seen[1] && edited->b == seen[2] && edited->a == seen[3]) return original;
    return edited;
}

}

ActorShaderParamsHook::ActorShaderParamsHook(): Hook("Actor Shader Params Hook", GET_SIG_ADDRESS("ActorShaderManager::setupShaderParameter")) {
}

void ActorShaderParamsHook::enableHook() {
    this->autoHook((void *) ActorShaderParamsCallback, (void **) &funcOriginal);
}

void ActorShaderParamsHook::ActorShaderParamsCallback(
    void *screenContext,
    void *entityContext,
    void *entity,
    void *overlay,
    void *changeColor,
    void *changeColor2,
    void *glintColor,
    uint64_t uvOffset1,
    uint64_t uvOffset2,
    uint64_t uvRot1,
    uint64_t uvRot2,
    uint64_t glintUVScale,
    uint64_t uvAnim,
    uint64_t brightness,
    uint64_t flag,
    uint64_t lightEmission
) {
    auto glint = ModuleManager::getModule("Glint Color");
    auto hurt = ModuleManager::getModule("Hurt Color");
    const bool useGlint = glint && glint->isEnabled();
    const bool useHurt = hurt && hurt->isEnabled();
    if (!useGlint && !useHurt)
        return funcOriginal(screenContext, entityContext, entity, overlay, changeColor, changeColor2, glintColor, uvOffset1, uvOffset2, uvRot1, uvRot2, glintUVScale, uvAnim, brightness, flag, lightEmission);
    float o[4], c[4], g[4];
    if (!readColor(overlay, o) || !readColor(changeColor, c) || !readColor(glintColor, g))
        return funcOriginal(screenContext, entityContext, entity, overlay, changeColor, changeColor2, glintColor, uvOffset1, uvOffset2, uvRot1, uvRot2, glintUVScale, uvAnim, brightness, flag, lightEmission);

    MCCColor co, cc, cg;
    co.r = o[0]; co.g = o[1]; co.b = o[2]; co.a = o[3];
    cc.r = c[0]; cc.g = c[1]; cc.b = c[2]; cc.a = c[3];
    cg.r = g[0]; cg.g = g[1]; cg.b = g[2]; cg.a = g[3];

    if (!guardedColors(co, cc, cg, entity, useGlint, useHurt))
        return funcOriginal(screenContext, entityContext, entity, overlay, changeColor, changeColor2, glintColor, uvOffset1, uvOffset2, uvRot1, uvRot2, glintUVScale, uvAnim, brightness, flag, lightEmission);

    return funcOriginal(screenContext, entityContext, entity, pick(overlay, &co, o), pick(changeColor, &cc, c), changeColor2, pick(glintColor, &cg, g), uvOffset1, uvOffset2, uvRot1, uvRot2, glintUVScale, uvAnim, brightness, flag, lightEmission);
}
