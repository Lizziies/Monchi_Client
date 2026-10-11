#pragma once

#include <cstdint>
#include <string>

namespace sigs {

void init();
bool takeChanged();

uintptr_t address(const std::string& name);
int offset(std::string_view name, int fallback = -1);

struct Stats {
    std::string gameVersion = "unknown";
    std::string source = "–";
    int found = 0;
    int total = 0;
};
Stats stats();

}
