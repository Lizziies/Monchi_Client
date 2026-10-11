#pragma once

#include <imgui_internal.h>
#include <algorithm>
#include <vector>

namespace cosmetics::textures {

inline std::vector<ImTextureData*> retired;

inline void retire(ImTextureData* texture) {
    if (!texture || texture->WantDestroyNextFrame) return;
    texture->WantDestroyNextFrame = true;
    retired.push_back(texture);
}

inline void collect() {
    std::erase_if(retired, [](ImTextureData* texture) {
        if (texture->Status != ImTextureStatus_Destroyed) return false;
        ImGui::GetPlatformIO().Textures.find_erase(texture);
        ImGui::UnregisterUserTexture(texture);
        IM_DELETE(texture);
        return true;
    });
}

}
