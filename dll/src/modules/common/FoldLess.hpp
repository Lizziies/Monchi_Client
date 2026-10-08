#pragma once

#include <algorithm>
#include <cctype>
#include <string_view>

namespace text {
inline bool foldLess(std::string_view a, std::string_view b) {
    return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(), [](unsigned char x, unsigned char y) {
        return std::tolower(x) < std::tolower(y);
    });
}
}
