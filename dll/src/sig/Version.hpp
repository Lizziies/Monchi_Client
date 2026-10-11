#pragma once

#include <string>
#include <vector>

namespace sigs {
inline std::string supportedVersion(const std::vector<std::string>& keys, const std::vector<std::string>& known) {
    for (const auto& key : keys)
        for (const auto& version : known)
            if (version == key) return version;
    return {};
}
}
