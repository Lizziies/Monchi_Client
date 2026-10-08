#pragma once

#include "Options.hpp"
#include "I18n.hpp"

#include <json.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

// The game writes the global resource packs to global_resource_packs.json whenever they change. Packs a
// server or a world forces on top are not in there.
namespace packlist {

namespace fs = std::filesystem;

inline std::string clean(std::string s) {
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        if (s.compare(i, 2, "\xC2\xA7") == 0 && i + 2 < s.size()) {
            i += 2;
            continue;
        }
        out += s[i];
    }
    return out;
}

inline std::string langValue(const fs::path& dir, const std::string& key) {
    for (auto name : {"en_US.lang", "en_GB.lang"}) {
        std::ifstream in(dir / "texts" / name);
        std::string line;
        while (std::getline(in, line)) {
            if (line.compare(0, key.size() + 1, key + "=") != 0) continue;
            std::string v = line.substr(key.size() + 1);
            size_t cut = v.find('\t');
            if (cut != std::string::npos) v.resize(cut);
            while (!v.empty() && (v.back() == '\r' || v.back() == ' ')) v.pop_back();
            return v;
        }
    }
    return key;
}

inline void scan(const fs::path& root, std::map<std::string, std::string>& names, int depth) {
    std::error_code ec;
    for (auto& e : fs::directory_iterator(root, ec)) {
        if (!e.is_directory(ec)) continue;
        auto manifest = e.path() / "manifest.json";
        if (!fs::exists(manifest, ec)) {
            if (depth > 0) scan(e.path(), names, depth - 1);
            continue;
        }
        std::ifstream in(manifest);
        auto j = nlohmann::json::parse(in, nullptr, false, true);
        if (j.is_discarded() || !j.contains("header")) continue;
        auto& h = j["header"];
        std::string uuid = h.value("uuid", "");
        std::string name = h.value("name", "");
        if (uuid.empty()) continue;
        if (name.rfind("pack.", 0) == 0) name = langValue(e.path(), name);
        names[uuid] = clean(name);
    }
}

inline std::vector<std::string> active() {
    using clock = std::chrono::steady_clock;
    static std::vector<std::string> shown;
    static fs::file_time_type seen{};
    static fs::path source;
    static clock::time_point checked{};
    static bool incomplete = false;

    auto now = clock::now();
    if (now - checked < std::chrono::seconds(incomplete ? 10 : 2) && checked != clock::time_point{}) return shown;
    checked = now;

    std::error_code ec;
    fs::path file = mcopt::newest(L"minecraftpe\\global_resource_packs.json");
    if (file.empty()) return shown;
    auto time = fs::last_write_time(file, ec);
    if (!incomplete && file == source && time == seen) return shown;
    source = file;
    seen = time;

    std::ifstream in(file);
    auto list = nlohmann::json::parse(in, nullptr, false, true);
    if (!list.is_array()) return shown;

    std::map<std::string, std::string> names;
    fs::path com = file.parent_path().parent_path();
    for (auto root : {com / "resource_packs", com / "development_resource_packs", mcopt::mojang() / "Shared" / "games" / "com.mojang" / "resource_packs",
                      mcopt::mojang() / "Shared" / "games" / "com.mojang" / "development_resource_packs"})
        scan(root, names, 1);

    shown.clear();
    incomplete = false;
    for (auto& p : list) {
        std::string id = p.value("pack_id", "");
        auto it = names.find(id);
        if (it != names.end() && !it->second.empty()) {
            shown.push_back(it->second);
        } else {
            shown.push_back(i18n::fmt("Unknown pack ({})", id.substr(0, 8)));
            incomplete = true;
        }
    }
    return shown;
}

}
