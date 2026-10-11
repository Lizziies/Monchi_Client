#pragma once

#include <imgui.h>

namespace draw {

float approach(float current, float target, float speed);
void setMotion(bool on);
bool motion();
float easeOutBack(float t);
float easeOutCubic(float t);
float easeInOutCubic(float t);
float easeInOutSine(float t);

void heart(ImDrawList* dl, ImVec2 center, float size, ImU32 color);
void sparkle(ImDrawList* dl, ImVec2 center, float size, ImU32 color);
void glow(ImDrawList* dl, ImVec2 min, ImVec2 max, float rounding, ImU32 color, float spread);
void gradientRect(ImDrawList* dl, ImVec2 min, ImVec2 max, ImU32 left, ImU32 right, float rounding);
void pill(ImDrawList* dl, ImVec2 min, ImVec2 max, ImU32 color);
void textCentered(ImDrawList* dl, ImFont* font, float size, ImVec2 center, ImU32 color, const char* text);

}
