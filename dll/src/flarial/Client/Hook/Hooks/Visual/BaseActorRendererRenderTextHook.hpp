// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Hook/Hooks/Visual/BaseActorRendererRenderTextHook.hpp: only the five argument form of
// renderText exists on 1.26.52; the role logo drawing is left out because it needs ViewRenderData, tessellator and
// texture group layouts that are not verified for this build.
#pragma once

#include "../Hook.hpp"
#include "../../../../SDK/Client/Render/Font.hpp"
#include "../../../../SDK/Client/Render/NameTagRenderObject.hpp"
#include "../../../../SDK/Client/Render/ScreenContext.hpp"
#include "../../../../SDK/Client/Render/ViewRenderData.hpp"
#include "../../../../Utils/Memory/Memory.hpp"
#include "../../../../Utils/Utils.hpp"

class BaseActorRendererRenderTextHook : public Hook {
    static void callback(ScreenContext* screenContext, ViewRenderData* viewData, NameTagRenderObject* tagData, Font* font, void* mesh);

public:
    typedef void(__fastcall* original)(ScreenContext*, ViewRenderData*, NameTagRenderObject*, Font*, void* mesh);

    static inline original funcOriginal = nullptr;

    BaseActorRendererRenderTextHook();

    void enableHook() override;
};
