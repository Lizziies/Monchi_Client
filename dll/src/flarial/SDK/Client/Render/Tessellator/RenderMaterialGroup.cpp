// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/SDK/Client/Render/Tessellator/RenderMaterialGroup.cpp: no call through a missing material group.
#include "RenderMaterialGroup.hpp"

#include <Utils/Utils.hpp>
#include <Utils/Memory/Memory.hpp>
#include <Utils/Memory/Game/SignatureAndOffsetManager.hpp>

mce::MaterialPtr* mce::RenderMaterialGroup::createUI(const HashedString& materialName) {
    static auto uiRenderMaterialGroup = Memory::getOffsetFromSig<void*>(GET_SIG_ADDRESS("mce::RenderMaterialGroup::ui"), 3);
    // 1.26.52 is left without this signature on purpose: its slot 1 now takes (group, out shared_ptr, name), not (group, name)
    if (!uiRenderMaterialGroup) return nullptr;

    return Memory::CallVFunc<1, MaterialPtr*, const HashedString&>(uiRenderMaterialGroup, materialName);
}
