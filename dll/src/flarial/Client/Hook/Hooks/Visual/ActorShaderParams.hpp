// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Hook/Hooks/Visual/ActorShaderParams.hpp: on 1.26.52 setupShaderParameter takes sixteen
// arguments, the public callback only fifteen. Every stack slot is eight bytes, so the callback keeps them as plain
// integers and hands each one on bit for bit.
#pragma once

#include "../Hook.hpp"
#include "../../../../Utils/Memory/Memory.hpp"
#include "../../../../Utils/Utils.hpp"
#include "SDK/Client/Render/BaseActorRenderContext.hpp"

class ActorShaderParamsHook : public Hook {
private:
    static void ActorShaderParamsCallback(
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
    );

public:
    typedef void (__thiscall *ActorShaderParamsOriginal)(
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
    );

    static inline ActorShaderParamsOriginal funcOriginal = nullptr;

    ActorShaderParamsHook();

    void enableHook() override;
};
