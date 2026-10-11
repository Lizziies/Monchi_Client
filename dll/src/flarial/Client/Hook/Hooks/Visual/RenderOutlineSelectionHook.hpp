// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Hook/Hooks/Visual/RenderOutlineSelectionHook.hpp: the block position arrives as a pointer
// (the game passes a BlockPos by reference) and is handed on untouched.
#pragma once

#include "../Hook.hpp"
#include "../../../../Utils/Memory/Memory.hpp"
#include "../../../Module/Manager.hpp"
#include "../../../../Utils/Memory/Game/SignatureAndOffsetManager.hpp"

class RenderOutlineSelectionHook : public Hook {
private:
    static void OutlineSelectioCallback(LevelRendererPlayer *obj, ScreenContext *scn, void *block, void *region, Vec3<int> *pos);

public:
    typedef void(__thiscall *original)(LevelRendererPlayer *obj, ScreenContext *scn, void *block, void *region, Vec3<int> *pos);

    static inline original funcOriginal = nullptr;

    RenderOutlineSelectionHook();

    void enableHook() override;
};
