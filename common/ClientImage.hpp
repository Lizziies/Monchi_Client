#pragma once

#include <string>
#include <string_view>

inline bool isClientImage(std::wstring_view name) {
    std::wstring lower(name);
    for (auto& c : lower)
        if (c >= L'A' && c <= L'Z') c += L'a' - L'A';
    if (lower == L"monchi.dll" || lower == L"mochi.dll") return true;
    return lower.ends_with(L".dll") && (lower.starts_with(L"monchi-") || lower.starts_with(L"mochi-"));
}
