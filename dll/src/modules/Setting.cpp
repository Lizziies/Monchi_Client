#include "Setting.hpp"

#include <algorithm>

using nlohmann::json;

json Setting::save() const {
    switch (type) {
    case SettingType::Bool: return b;
    case SettingType::Float: return f;
    case SettingType::Int:
    case SettingType::Choice:
    case SettingType::Key: return i;
    case SettingType::Color: return json::array({color.x, color.y, color.z, color.w});
    case SettingType::Text: return text;
    }
    return nullptr;
}

void Setting::load(const json& j) {
    try {
        switch (type) {
        case SettingType::Bool: b = j.get<bool>(); break;
        case SettingType::Float: f = std::clamp(j.get<float>(), fmin, fmax); break;
        case SettingType::Int: i = std::clamp(j.get<int>(), imin, imax); break;
        case SettingType::Choice: i = std::clamp(j.get<int>(), 0, (int)choices.size() - 1); break;
        case SettingType::Key: i = j.get<int>(); break;
        case SettingType::Color:
            if (j.is_array() && j.size() == 4) color = {j[0], j[1], j[2], j[3]};
            break;
        case SettingType::Text: text = j.get<std::string>(); break;
        }
    } catch (const json::exception&) {
    }
}
