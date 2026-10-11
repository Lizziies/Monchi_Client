#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <cstdint>

namespace net {

std::optional<std::string> get(const std::string& url, int timeoutMs = 8000);
bool download(const std::string& url, const std::filesystem::path& to, const std::function<void(float)>& progress, const std::function<bool()>& cancelled = {}, uint64_t maxBytes = 0);

}
