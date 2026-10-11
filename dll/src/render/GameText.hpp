#pragma once

#include <imgui.h>
#include <string>

namespace gameText {

bool canDraw(ImFont* font, const std::string& value);

ImVec2 size(ImFont* font, float height, const std::string& value);
ImVec2 draw(ImDrawList* dl, ImFont* font, float height, ImVec2 at, ImU32 color, const std::string& value,
            float shadow = 0.f);

}
