#pragma once

#include <imgui.h>
#include <cstring>

inline ImFont* addOwnedFont(ImFontAtlas& atlas, const void* data, int size, float pixels, ImFontConfig config) {
    auto* bytes = IM_ALLOC(size);
    if (!bytes) return nullptr;
    std::memcpy(bytes, data, size);
    config.FontDataOwnedByAtlas = true;
    return atlas.AddFontFromMemoryTTF(bytes, size, pixels, &config, atlas.GetGlyphRangesDefault());
}
