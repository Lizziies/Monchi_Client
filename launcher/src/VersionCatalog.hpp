#pragma once

#include <json.hpp>
#include <algorithm>
#include <array>
#include <charconv>
#include <string>
#include <vector>

namespace versions {
struct Download {
    std::string version;
    std::string name;
    bool preview = false;
    std::vector<std::string> urls;
    std::array<int, 4> parts{};
};

inline bool parseVersion(const std::string& value, std::array<int, 4>& parts) {
    size_t start = 0;
    for (int i = 0; i < 4; ++i) {
        auto end = value.find('.', start);
        if ((i < 3 && end == std::string::npos) || (i == 3 && end != std::string::npos)) return false;
        if (end == std::string::npos) end = value.size();
        auto result = std::from_chars(value.data() + start, value.data() + end, parts[i]);
        if (result.ec != std::errc{} || result.ptr != value.data() + end || parts[i] < 0) return false;
        start = end + 1;
    }
    return parts[0] == 1;
}

inline bool microsoftPackage(const std::string& url, bool preview) {
    auto begin = url.starts_with("https://") ? 8u : url.starts_with("http://") ? 7u : 0u;
    if (!begin) return false;
    auto slash = url.find('/', begin);
    auto host = url.substr(begin, slash - begin);
    if (host != "assets1.xboxlive.com" && host != "assets2.xboxlive.com") return false;
    auto file = url.substr(url.find_last_of('/') + 1);
    return file.starts_with(preview ? "Microsoft.MinecraftWindowsBeta_" : "Microsoft.MinecraftUWP_") &&
           file.ends_with("_x64__8wekyb3d8bbwe.msixvc");
}

inline std::vector<Download> parseCatalog(const std::string& body) {
    std::vector<Download> out;
    auto j = nlohmann::json::parse(body, nullptr, false);
    if (!j.is_object()) return out;
    for (const char* channel : {"release", "preview"}) {
        if (!j.contains(channel) || !j[channel].is_object()) continue;
        for (auto& [version, links] : j[channel].items()) {
            Download d;
            if (!parseVersion(version, d.parts) || d.parts < std::array<int, 4>{1, 21, 120, 0} || !links.is_array()) continue;
            d.version = version;
            d.name = version.substr(0, version.find_last_of('.'));
            d.preview = std::string(channel) == "preview";
            for (auto& link : links)
                if (link.is_string() && microsoftPackage(link.get<std::string>(), d.preview)) d.urls.push_back(link.get<std::string>());
            if (!d.urls.empty()) out.push_back(std::move(d));
        }
    }
    std::sort(out.begin(), out.end(), [](auto& a, auto& b) { return a.parts > b.parts; });
    return out;
}
}
