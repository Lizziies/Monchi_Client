#include "NativeStartup.hpp"

#include <string_view>

int main() {
    using nativeStartup::missingBinding;
    if (missingBinding([](const char*) { return 1; })) return 1;
    auto missingUi = missingBinding([](const char* name) {
        return std::string_view(name) != "ScreenView::setupAndRender";
    });
    if (!missingUi || std::string_view(missingUi) != "ScreenView::setupAndRender") return 2;
    auto missingPlayer = missingBinding([](const char* name) {
        return std::string_view(name) != "ClientInstance::getLocalPlayerIndex";
    });
    if (!missingPlayer || std::string_view(missingPlayer) != "ClientInstance::getLocalPlayerIndex") return 3;
    return 0;
}
