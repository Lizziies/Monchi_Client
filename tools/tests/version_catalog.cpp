#include "VersionCatalog.hpp"
#include <fstream>
#include <iostream>
#include <iterator>

int main(int argc, char** argv) {
    auto valid = "http://assets1.xboxlive.com/path/Microsoft.MinecraftUWP_1.26.5203.0_x64__8wekyb3d8bbwe.msixvc";
    if (!versions::microsoftPackage(valid, false)) return 1;
    for (auto url : {"https://assets1.xboxlive.com.evil.test/Microsoft.MinecraftUWP_x64__8wekyb3d8bbwe.msixvc",
                     "file:///Minecraft.msixvc", "http://user@assets1.xboxlive.com/a.msixvc",
                     "https://assets1.xboxlive.com/a.exe"})
        if (versions::microsoftPackage(url, false)) return 2;
    nlohmann::json input = {{"release", {{"1.26.52.3", {valid}}, {"1.26.9.0", {valid}},
        {"1.21.110.0", {valid}}, {"../escape", {valid}}, {"1.26.51.0", {42}}, {"1.26.50.0", false}}}};
    auto list = versions::parseCatalog(input.dump());
    if (list.size() != 2 || list.front().version != "1.26.52.3" || list.back().name != "1.26.9") return 3;
    if (!versions::parseCatalog("invalid").empty()) return 4;
    if (argc > 1) {
        std::ifstream file(argv[1]);
        std::string body((std::istreambuf_iterator<char>(file)), {});
        auto live = versions::parseCatalog(body);
        if (live.empty()) return 5;
        std::cout << live.size() << " versions, newest " << live.front().version << '\n';
    }
    return 0;
}
