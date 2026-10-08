#pragma once
#include "VersionCatalog.hpp"
#include <atomic>
#include <functional>

namespace versions {
std::vector<Download> catalog(std::string& error);
bool install(const Download& version, const std::function<void(const char*, float)>& progress,
             const std::atomic<bool>& cancel, std::string& error);
}
