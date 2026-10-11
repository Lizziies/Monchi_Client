#pragma once

#include "imgui.h"
#include <string>

namespace itemicon {
bool draw(ImDrawList* dl, ImVec2 at, float size, const std::string& name);
}
