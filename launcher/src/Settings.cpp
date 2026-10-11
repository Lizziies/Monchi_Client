#include "Settings.hpp"
#include "Files.hpp"

#include <json.hpp>
#include <windows.h>
#include <type_traits>

using nlohmann::json;

Settings Settings::load() {
    return load(files::settings());
}

Settings Settings::load(const std::filesystem::path& path) {
    Settings s;
    auto j = json::parse(files::read(path), nullptr, false);
    if (j.is_discarded() || !j.is_object()) return s;
    auto read = [&](const char* key, auto& value) {
        auto it = j.find(key);
        if (it == j.end()) return;
        try { value = it->get<std::decay_t<decltype(value)>>(); }
        catch (const json::exception&) {}
    };
    read("beta", s.beta);
    read("autoInject", s.autoInject);
    read("closeAfterInject", s.closeAfterInject);
    read("customDll", s.customDll);
    read("pinned", s.pinned);
    read("accent", s.accent);
    read("figure", s.figure);
    if (s.figure < 0 || s.figure > 1) s.figure = 1;
    if (s.accent < -1 || s.accent > 7) s.accent = -1;
    if (j.contains("folders") && j["folders"].is_array())
        for (auto& f : j["folders"])
            if (f.is_string()) s.folders.push_back(f.get<std::string>());
    return s;
}

void Settings::save() const {
    save(files::settings());
}

bool Settings::save(const std::filesystem::path& path) const {
    json j = {
        {"beta", beta},
        {"autoInject", autoInject},
        {"closeAfterInject", closeAfterInject},
        {"customDll", customDll},
        {"pinned", pinned},
        {"accent", accent},
        {"figure", figure},
        {"folders", folders},
    };
    auto part = path;
    part += L".part";
    bool ok = files::write(part, j.dump(2)) &&
        MoveFileExW(part.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    if (!ok) {
        std::error_code ec;
        std::filesystem::remove(part, ec);
    }
    return ok;
}
