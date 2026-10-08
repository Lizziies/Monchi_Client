#include "Look.hpp"

namespace look {
int previewCalls = 0;

void figure(ImDrawList*, const char* id, ImVec2 min, ImVec2 max, ImVec4) {
    ++previewCalls;
    ImGui::SetCursorScreenPos(min);
    ImGui::InvisibleButton(id, {max.x - min.x, max.y - min.y});
}
const std::vector<Entry>& entries() {
    static const std::vector<Entry> list{{"a", "Wings", "wings", true}, {"b", "Cape", "cape", false}};
    return list;
}
void toggle(const std::string&) {}
bool hasSkin() { return false; }
void standIn(int) {}
bool usingStandIn() { return true; }
bool skinFromGameFiles() { return false; }
bool skinLocked() { return false; }
bool gameOwnsSettings() { return false; }
bool settingsMissing() { return false; }

}
