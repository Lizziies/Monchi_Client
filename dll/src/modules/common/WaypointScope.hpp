#pragma once

#include <string_view>

namespace waypointScope {

inline bool matches(std::string_view saved, std::string_view current, bool onlyCurrent) {
    if (!onlyCurrent) return true;
    if (current.empty()) return false;
    return saved.empty() || saved == current;
}

}
