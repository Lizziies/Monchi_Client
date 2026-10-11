#pragma once

#include <functional>
#include <string>
#include <vector>

// While active, Flarial's Module::add* calls (normally drawing its own settings page) only describe the setting,
// so Monchi can show it with its own widgets. A setting is either stored under a key in the module's settings or
// bound to a variable the module owns (ptr).
namespace settingsRecorder {

enum class Kind { Header, Text, Button, Toggle, Slider, SliderInt, RangeSlider, TextBox, Dropdown, Color, Keybind };

struct Item {
    Kind kind;
    std::string label;
    std::string subtext;
    std::string key;
    std::string key2;
    void* ptr = nullptr;
    void* ptr2 = nullptr;
    void* ptr3 = nullptr;
    float min = 0.f;
    float max = 100.f;
    bool zerosafe = true;
    bool visible = true;
    std::vector<std::string> options;
    std::function<void()> action;
};

inline bool active = false;
inline std::vector<Item> items;

inline bool take(Item item) {
    if (!active) return false;
    items.push_back(std::move(item));
    return true;
}

}
