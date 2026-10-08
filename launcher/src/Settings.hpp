#pragma once

#include <string>
#include <vector>
#include <filesystem>

struct Settings {
    bool beta = false;
    bool autoInject = true;
    bool closeAfterInject = false;
    std::string customDll;
    std::string pinned;
    int accent = -1;
    // the figure shown while the player's own skin is not known: 0 boy, 1 girl
    int figure = 1;
    std::vector<std::string> folders;

    static Settings load();
    static Settings load(const std::filesystem::path& path);
    void save() const;
    bool save(const std::filesystem::path& path) const;
};
