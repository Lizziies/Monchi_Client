#include "Paths.hpp"
#include "DataDir.hpp"

#include <windows.h>
#include <shlobj.h>

#include <fstream>

namespace paths {

static fs::path rootDir;
static fs::path moduleDir;

static fs::path ensure(const fs::path& p) {
    std::error_code ec;
    fs::create_directories(p, ec);
    return p;
}

void init(void* module) {
    wchar_t buf[MAX_PATH]{};
    GetModuleFileNameW(static_cast<HMODULE>(module), buf, MAX_PATH);
    moduleDir = fs::path(buf).parent_path();

    std::wifstream override(moduleDir / L"Monchi.root");
    std::wstring custom;
    if (override && std::getline(override, custom) && !custom.empty()) {
        rootDir = fs::path(custom);
        ensure(rootDir);
        return;
    }

    rootDir = dataDir();
    if (rootDir.empty()) rootDir = moduleDir / L"MonchiData";
    ensure(rootDir);
}

const fs::path& root() { return rootDir; }
const fs::path& dllDir() { return moduleDir; }
fs::path configs() { return ensure(rootDir / L"configs"); }
fs::path logs() { return ensure(rootDir / L"logs"); }
fs::path cache() { return ensure(rootDir / L"cache"); }
fs::path scripts() { return ensure(rootDir / L"scripts"); }

}
