#pragma once

#include <json.hpp>

inline nlohmann::json preserveModuleSettings(const nlohmann::json& previous, nlohmann::json current) {
    if (!current.is_object()) return previous;
    if (!previous.is_object()) return current;
    auto saved = previous;
    if (current.contains("enabled"))
        for (const char* key : {"favorite", "parked"})
            if (!current.contains(key)) saved.erase(key);
    for (auto& [key, value] : current.items()) {
        if (key == "settings" && value.is_object() && saved.contains(key) && saved[key].is_object()) {
            for (auto& [id, setting] : value.items()) saved[key][id] = setting;
        } else saved[key] = value;
    }
    return saved;
}
