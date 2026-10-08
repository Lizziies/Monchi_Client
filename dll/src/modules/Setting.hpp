#pragma once

#include <imgui.h>
#include <json.hpp>

#include <functional>
#include <string>
#include <vector>

enum class SettingType { Bool, Float, Int, Color, Choice, Key, Text };

struct Setting {
    std::string id;
    std::string label;
    SettingType type;

    bool b = false;
    float f = 0.f, fmin = 0.f, fmax = 1.f;
    int i = 0, imin = 0, imax = 100;
    ImVec4 color{1, 1, 1, 1};
    std::vector<std::string> choices;
    std::string text;
    std::string hint;
    // what an empty text field stands for; clicking into the empty field puts it there, ready to be changed
    std::string prefill;
    const char* format = "%.1f";
    bool hidden = false;
    bool style = false;
    std::function<bool()> visible;

    nlohmann::json save() const;
    void load(const nlohmann::json& j);
    bool shown() const { return !hidden && (!visible || visible()); }
};
