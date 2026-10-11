#pragma once

#include <filesystem>

namespace paths {

namespace fs = std::filesystem;

void init(void* module);

const fs::path& root();
const fs::path& dllDir();
fs::path configs();
fs::path logs();
fs::path cache();
fs::path scripts();

}
