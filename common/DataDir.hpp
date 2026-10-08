#pragma once

#include <filesystem>

// %LOCALAPPDATA%\Monchi, shared by client and launcher. Data from before the rename (%LOCALAPPDATA%\Mochi) is moved
// over the first time, so settings, configs and logs carry over. Empty when the folder cannot be resolved.
std::filesystem::path dataDir();
