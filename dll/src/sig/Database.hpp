#pragma once
#include <json.hpp>

namespace sigs {

inline nlohmann::json supplement(nlohmann::json remote, const nlohmann::json& bundled) {
    for (const char* area : {"sigs", "offsets"}) {
        if (!bundled.contains(area) || !bundled[area].is_object()) continue;
        if (!remote.contains(area)) remote[area] = bundled[area];
        else if (remote[area].is_object())
            for (auto& [name, value] : bundled[area].items())
                if (!remote[area].contains(name)) remote[area][name] = value;
    }
    return remote;
}

}
