#include "Levi.hpp"
#include "FileHash.hpp"
#include "Files.hpp"
#include "Net.hpp"

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

namespace levi {

namespace {

constexpr const char* url = "https://github.com/LiteLDev/LeviLauncher/releases/download/v0.3.13/LeviLauncher.exe";
constexpr const char* digest = "0d6a7513f8f64bdcf2c2026dfe922f06fe5e2159e2e4fc7c4fca702ece8da38c";

std::filesystem::path own() { return files::fs::path(files::root() / L"tools" / L"LeviLauncher.exe"); }

std::filesystem::path under(const KNOWNFOLDERID& id, const wchar_t* sub) {
    PWSTR base = nullptr;
    if (FAILED(SHGetKnownFolderPath(id, 0, nullptr, &base))) return {};
    std::filesystem::path p = std::filesystem::path(base) / sub;
    CoTaskMemFree(base);
    return p;
}

bool looksLikeExe(const std::filesystem::path& p) {
    std::error_code ec;
    auto size = std::filesystem::file_size(p, ec);
    return !ec && size == 34385528 && files::sha256(p) == digest;
}

}

std::filesystem::path find() {
    std::error_code ec;
    if (looksLikeExe(own())) return own();
    for (auto& candidate : {under(FOLDERID_LocalAppData, L"Programs\\LeviLauncher\\LeviLauncher.exe"),
                            under(FOLDERID_ProgramFiles, L"LeviLauncher\\LeviLauncher.exe")})
        if (!candidate.empty() && looksLikeExe(candidate)) return candidate;
    return {};
}

bool install(const std::function<void(float)>& progress, std::string& error) {
    std::error_code ec;
    std::filesystem::create_directories(own().parent_path(), ec);
    auto part = own();
    part += L".part";
    if (!net::download(url, part, progress, {}, 34385528)) {
        error = "Download failed";
        return false;
    }
    if (!looksLikeExe(part)) {
        std::filesystem::remove(part, ec);
        error = "Checksum does not match";
        return false;
    }
    std::filesystem::remove(own(), ec);
    std::filesystem::rename(part, own(), ec);
    if (ec) {
        error = "Could not save LeviLauncher";
        return false;
    }
    return true;
}

bool open() {
    auto exe = find();
    if (exe.empty()) return false;
    auto dir = exe.parent_path();
    return reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", exe.c_str(), nullptr, dir.c_str(), SW_SHOWNORMAL)) > 32;
}

}
