#pragma once

#include <filesystem>
#include <functional>
#include <string>

namespace levi {

std::filesystem::path find();
bool install(const std::function<void(float)>& progress, std::string& error);
bool open();

}
