#pragma once

#include <array>

namespace nativeStartup {

template <class Resolve>
const char* missingBinding(Resolve&& resolve) {
    constexpr std::array required{
        "ScreenView::setupAndRender",
        "ClientInstance::getLocalPlayerIndex",
    };
    for (const char* name : required)
        if (!resolve(name)) return name;
    return nullptr;
}

}
