#pragma once

#include <windows.h>

#include <atomic>
#include <filesystem>
#include <optional>
#include <string>

namespace game {

std::optional<DWORD> running();
std::string installedVersion();
// folder of the Store install, empty when there is none
std::filesystem::path installFolder();
std::string readableVersion(const std::string& raw);
std::wstring runningPath(DWORD pid);
bool launch();
bool launchExe(const std::filesystem::path& exe, std::string& error);
bool waitReady(DWORD pid, int timeoutMs);
bool injected(DWORD pid);
bool inject(DWORD pid, const std::filesystem::path& dll, std::string& error);

}
