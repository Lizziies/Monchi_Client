#pragma once

#include <imgui.h>
#include <json.hpp>

#include <string>
#include <vector>

struct Theme {
    std::string name;
    ImVec4 bg;
    ImVec4 surface;
    ImVec4 surfaceHover;
    ImVec4 accent;
    ImVec4 accent2;
    ImVec4 text;
    ImVec4 textDim;
    ImVec4 ok;
    ImVec4 warn;
    ImVec4 off;
    float rounding = 14.f;
    float opacity = 0.94f;
    float animSpeed = 1.f;
    bool gradient = true;
    bool sparkles = true;
    bool hearts = true;
    ImVec4 border{0.f, 0.f, 0.f, 0.f};
};

namespace theme {

struct Accent {
    const char* name;
    ImVec4 accent;
    ImVec4 accent2;
};

Theme& current();
const std::vector<Theme>& presets();
const std::vector<Accent>& accents();
void use(const Theme& t);
void applyStyle();

void setFade(float f);
float fade();
ImU32 col(const ImVec4& c, float alpha = 1.f);
ImVec4 mix(const ImVec4& a, const ImVec4& b, float t);
ImVec4 border();

nlohmann::json save();
void load(const nlohmann::json& j);

std::string exportCode();
bool importCode(const std::string& code);

}
