#pragma once

#include <optional>
#include <string>

namespace http {

std::optional<std::string> get(const std::wstring& host, const std::wstring& path, int timeoutMs = 4000);
std::wstring repoRawPath(const std::wstring& file);

}
