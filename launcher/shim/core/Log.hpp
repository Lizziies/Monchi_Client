#pragma once

#include "Files.hpp"

#include <string>

namespace logger {

template <class... Args>
void warn(const char*, Args&&...) {}

inline std::string narrow(const std::wstring& w) { return files::narrow(w); }

}
