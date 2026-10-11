#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include <utility>

namespace update {

struct Release {
    std::string tag;
    std::string notes;
    std::string dllUrl;
    std::string coreUrl;
    std::string launcherUrl;
    std::string sumsUrl;
    std::string signatureUrl;
};

std::optional<Release> latest(bool beta, int timeoutMs = 8000);
bool newer(const std::string& candidate, const std::string& current);

std::string installedTag();
bool replaceFiles(const std::vector<std::pair<std::filesystem::path, std::filesystem::path>>& files, std::string& error);
bool installDll(const Release& r, const std::function<void(float)>& progress, std::string& error);
bool swapLauncher(const Release& r, const std::function<void(float)>& progress, std::string& error);
void cleanup();

}
