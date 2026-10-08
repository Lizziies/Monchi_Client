#include "DataDir.hpp"

#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

std::filesystem::path dataDir() {
#ifdef _WIN32
    PWSTR local = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local))) return {};
    std::filesystem::path base(local);
    CoTaskMemFree(local);
    auto now = base / L"Monchi", before = base / L"Mochi";
    std::error_code ec;
    if (!std::filesystem::exists(now, ec) && std::filesystem::exists(before, ec)) MoveFileExW(before.c_str(), now.c_str(), 0);
    return now;
#else
    const char* home = std::getenv("HOME");
    return std::filesystem::path(home ? home : ".") / ".monchi";
#endif
}
