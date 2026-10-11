#include "Downgrade.hpp"
#include "Files.hpp"
#include "Game.hpp"
#include "Net.hpp"
#include "../res/resource.h"
#include <windows.h>

namespace versions {
namespace {
bool deploy(const std::filesystem::path& package, const Download& version, bool validate, std::string& error) {
    auto dir = files::root() / L"versions";
    auto script = dir / L"install.ps1";
    auto request = dir / L"request.json";
    auto errors = dir / L"install-error.txt";
    auto resource = FindResourceW(nullptr, MAKEINTRESOURCEW(IDR_VERSION_INSTALL), RT_RCDATA);
    auto data = resource ? LoadResource(nullptr, resource) : nullptr;
    if (!data || !files::write(script, std::string(static_cast<const char*>(LockResource(data)), SizeofResource(nullptr, resource)))) {
        error = "Could not prepare version installation";
        return false;
    }
    nlohmann::json j = {{"package", files::narrow(package.wstring())}, {"version", version.version},
        {"preview", version.preview}, {"validateOnly", validate}, {"backup", files::narrow((files::root() / L"backups").wstring())},
        {"error", files::narrow(errors.wstring())}};
    std::error_code ec;
    std::filesystem::remove(errors, ec);
    if (!files::write(request, j.dump())) { error = "Could not prepare version installation"; return false; }
    wchar_t system[MAX_PATH]{};
    GetSystemDirectoryW(system, MAX_PATH);
    auto exe = std::filesystem::path(system) / L"WindowsPowerShell/v1.0/powershell.exe";
    std::wstring command = L"\"" + exe.wstring() + L"\" -NoProfile -NonInteractive -ExecutionPolicy Bypass -File \"" + script.wstring() + L"\" -Request \"" + request.wstring() + L"\"";
    STARTUPINFOW si{sizeof(si)};
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        error = "Could not start Windows package installation";
        return false;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    if (code) {
        error = files::read(errors);
        if (error.empty()) error = "Windows could not install this Minecraft version";
    }
    return code == 0;
}
}

std::vector<Download> catalog(std::string& error) {
    auto body = net::get("https://cdn.jsdelivr.net/gh/MinecraftBedrockArchiver/GdkLinks@latest/urls.min.json", 15000);
    auto result = body ? parseCatalog(*body) : std::vector<Download>{};
    if (result.empty()) error = "Could not load Minecraft versions";
    return result;
}

bool install(const Download& version, const std::function<void(const char*, float)>& progress,
             const std::atomic<bool>& cancel, std::string& error) {
    if (game::running()) { error = "Close Minecraft before switching versions"; return false; }
    auto dir = files::root() / L"versions";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (!deploy({}, version, true, error)) return false;
    auto package = dir / files::widen((version.preview ? "preview-" : "release-") + version.version + ".msixvc");
    bool downloaded = false;
    for (auto& url : version.urls) {
        if (cancel) break;
        if (!microsoftPackage(url, version.preview)) continue;
        downloaded = net::download(url, package, [&](float p) { progress("Downloading Minecraft", p); }, [&] { return cancel.load(); });
        if (downloaded) break;
    }
    if (!downloaded || cancel) {
        std::filesystem::remove(package, ec);
        error = cancel ? "Version download cancelled" : "Minecraft download failed";
        return false;
    }
    progress("Backing up worlds and installing", -1.f);
    bool installed = deploy(package, version, false, error);
    std::filesystem::remove(package, ec);
    return installed;
}
}
