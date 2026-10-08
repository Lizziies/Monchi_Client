#pragma once

#include <filesystem>
#include <string>

namespace files {

namespace fs = std::filesystem;

fs::path root();
fs::path bin();
fs::path dll();
// the Flarial core, loaded by the client from its own folder
fs::path core();
fs::path installedTag();
fs::path settings();
fs::path log();

std::string read(const fs::path& p);
bool write(const fs::path& p, const std::string& data);

std::string narrow(const std::wstring& w);
std::wstring widen(const std::string& s);

}
