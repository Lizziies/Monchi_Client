#include "Versions.hpp"
#include "Files.hpp"
#include "Game.hpp"

#include <windows.h>
#include <shlobj.h>

#include <algorithm>
#include <cwctype>

namespace versions {

namespace {

constexpr const wchar_t* exeName = L"Minecraft.Windows.exe";
constexpr int maxDepth = 5;
constexpr int maxEntries = 30000;

std::filesystem::path known(const KNOWNFOLDERID& id, const wchar_t* sub = L"") {
    PWSTR base = nullptr;
    if (FAILED(SHGetKnownFolderPath(id, 0, nullptr, &base))) return {};
    std::filesystem::path p = std::filesystem::path(base) / sub;
    CoTaskMemFree(base);
    return p;
}

std::string fileVersion(const std::filesystem::path& exe) {
    DWORD ignored = 0;
    DWORD size = GetFileVersionInfoSizeW(exe.c_str(), &ignored);
    if (!size) return {};
    std::vector<char> data(size);
    if (!GetFileVersionInfoW(exe.c_str(), 0, size, data.data())) return {};
    VS_FIXEDFILEINFO* info = nullptr;
    UINT len = 0;
    if (!VerQueryValueW(data.data(), L"\\", reinterpret_cast<void**>(&info), &len) || !info) return {};
    char raw[64];
    std::snprintf(raw, sizeof(raw), "%u.%u.%u.%u", HIWORD(info->dwFileVersionMS), LOWORD(info->dwFileVersionMS),
                  HIWORD(info->dwFileVersionLS), LOWORD(info->dwFileVersionLS));
    return game::readableVersion(raw);
}

bool hasWord(const std::filesystem::path& p, const std::wstring& word) {
    std::wstring s = p.wstring();
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return wchar_t(std::towlower(c)); });
    return s.find(word) != std::wstring::npos;
}

void walk(const std::filesystem::path& root, std::vector<Install>& out, int& budget) {
    std::error_code ec;
    if (!std::filesystem::is_directory(root, ec)) return;
    std::filesystem::recursive_directory_iterator it(root, std::filesystem::directory_options::skip_permission_denied, ec), end;
    for (; it != end && budget > 0; it.increment(ec), budget--) {
        if (ec) break;
        if (it.depth() >= maxDepth) {
            it.disable_recursion_pending();
            continue;
        }
        if (!it->is_regular_file(ec) || _wcsicmp(it->path().filename().c_str(), exeName) != 0) continue;
        Install i;
        i.exe = it->path();
        i.name = fileVersion(i.exe);
        i.preview = hasWord(i.exe, L"preview");
        if (i.name.empty()) i.name = files::narrow(i.exe.parent_path().filename().wstring());
        out.push_back(std::move(i));
    }
}

}

std::vector<Install> scan(const std::vector<std::filesystem::path>& extra) {
    std::vector<std::filesystem::path> roots = extra;
    for (auto& base : {known(FOLDERID_LocalAppData, L"Programs\\LeviLauncher"), known(FOLDERID_LocalAppData, L"LeviLauncher"),
                       known(FOLDERID_RoamingAppData, L"LeviLauncher"), known(FOLDERID_Documents, L"LeviLauncher"),
                       known(FOLDERID_LocalAppData, L"Monchi\\versions")})
        if (!base.empty()) roots.push_back(base);

    std::vector<Install> out;
    int budget = maxEntries;
    for (auto& r : roots) walk(r, out, budget);

    std::sort(out.begin(), out.end(), [](const Install& a, const Install& b) { return a.exe < b.exe; });
    out.erase(std::unique(out.begin(), out.end(), [](const Install& a, const Install& b) { return a.exe == b.exe; }), out.end());
    return out;
}

}
