#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace versions {

struct Install {
    std::filesystem::path exe;
    std::string name;
    bool preview = false;
};

std::vector<Install> scan(const std::vector<std::filesystem::path>& extra);

}
