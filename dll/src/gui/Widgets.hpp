#pragma once

#include "modules/Setting.hpp"

#include <imgui.h>

#include <string>
#include <vector>

namespace widgets {

bool toggle(const char* id, bool& value, bool enabled = true);
ImVec2 switchSize();
void drawSwitch(ImDrawList* dl, ImVec2 p, float a, float alpha);
bool setting(Setting& s);
bool button(const char* label, ImVec2 size = {0, 0}, bool primary = false);
bool keyCapture(const char* id, int& vk);
void sectionTitle(const char* text);
bool row(const char* label, float& v, float lo, float hi, const char* fmt);
bool row(const char* label, bool& v);
bool row(const char* label, ImVec4& c);
void hint(const char* text);
bool dropdown(const char* id, ImVec2 anchorMin, ImVec2 anchorMax, const std::vector<std::string>& choices, int& sel, bool hovered, bool clicked);
float dropdownOpen(const char* id);
bool dropdownEscaped();
void closePopups();

std::string keyName(int vk);
bool capturingKey();

}
