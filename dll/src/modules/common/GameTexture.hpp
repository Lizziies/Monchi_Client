#pragma once

#include "Image.hpp"
#include "Options.hpp"

#include <json.hpp>

#include <windows.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

// A texture the way the game picks it: from the active global resource packs, the top one first, then from the game's
// own packs. Reads files, so it belongs on a background job.
namespace gametex {

namespace fs = std::filesystem;

inline bool readFile(const fs::path& path, std::vector<uint8_t>& out) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return false;
    auto length = f.tellg();
    if (length <= 0 || length > 64 * 1024 * 1024) return false;
    out.resize(size_t(length));
    f.seekg(0);
    return bool(f.read(reinterpret_cast<char*>(out.data()), std::streamsize(out.size())));
}

// The game keeps its own textures in archives: a 16-byte header with the entry count at +8, then 256-byte entries
// (name length, name, and in the last eight bytes the offset behind the entry table and the length).
inline bool fromArchive(const fs::path& archive, const std::string& name, std::vector<uint8_t>& out) {
    std::vector<uint8_t> data;
    if (!readFile(archive, data) || data.size() < 16) return false;
    auto number = [&](size_t at) {
        uint32_t v = 0;
        std::memcpy(&v, data.data() + at, sizeof(v));
        return v;
    };
    if (number(0) != 0xb125277d || number(4) != 0x267052a0) return false;
    size_t count = number(8);
    if (count > (data.size() - 16) / 256) return false;
    size_t payload = 16 + count * 256;
    for (size_t i = 0; i < count; i++) {
        size_t entry = 16 + i * 256;
        size_t length = data[entry];
        if (length != name.size() || std::memcmp(data.data() + entry + 1, name.data(), length) != 0) continue;
        size_t offset = number(entry + 248), size = number(entry + 252);
        if (offset > data.size() - payload || size > data.size() - payload - offset) return false;
        out.assign(data.begin() + payload + offset, data.begin() + payload + offset + size);
        return true;
    }
    return false;
}

struct Pack {
    fs::path folder;
    std::string name;
};

inline void scan(const fs::path& root, std::map<std::string, Pack>& packs, int depth) {
    std::error_code ec;
    for (auto& e : fs::directory_iterator(root, ec)) {
        if (!e.is_directory(ec)) continue;
        auto manifest = e.path() / "manifest.json";
        if (!fs::exists(manifest, ec)) {
            if (depth > 0) scan(e.path(), packs, depth - 1);
            continue;
        }
        std::ifstream in(manifest);
        auto j = nlohmann::json::parse(in, nullptr, false, true);
        if (j.is_discarded() || !j.contains("header")) continue;
        std::string uuid = j["header"].value("uuid", "");
        auto shown = e.path().filename().u8string();
        if (!uuid.empty()) packs.try_emplace(uuid, Pack{e.path(), std::string(shown.begin(), shown.end())});
    }
}

inline std::vector<Pack> activePacks() {
    std::vector<Pack> out;
    fs::path file = mcopt::newest(L"minecraftpe\\global_resource_packs.json");
    if (file.empty()) return out;
    std::ifstream in(file);
    auto list = nlohmann::json::parse(in, nullptr, false, true);
    if (!list.is_array()) return out;
    std::map<std::string, Pack> packs;
    fs::path com = file.parent_path().parent_path();
    fs::path shared = mcopt::mojang() / "Shared" / "games" / "com.mojang";
    for (auto root : {com / "resource_packs", com / "development_resource_packs", shared / "resource_packs", shared / "development_resource_packs"})
        scan(root, packs, 1);
    for (auto& p : list)
        if (auto it = packs.find(p.value("pack_id", "")); it != packs.end()) out.push_back(it->second);
    return out;
}

// folder like "textures/ui", name like "cross_hair.png"; source says which pack it came from, empty for the game's own
inline bool load(const std::string& folder, const std::string& name, int maxSize, img::Pixels& out, std::string& source) {
    std::vector<uint8_t> bytes;
    for (auto& pack : activePacks()) {
        if (!readFile(pack.folder / folder / name, bytes) || !img::load(bytes, maxSize, out)) continue;
        source = pack.name;
        return true;
    }
    wchar_t exe[32768]{};
    DWORD length = GetModuleFileNameW(nullptr, exe, DWORD(std::size(exe)));
    if (!length || length >= std::size(exe)) return false;
    fs::path root = fs::path(exe).parent_path() / L"data" / L"resource_packs";
    std::vector<fs::path> own{root / L"vanilla"};
    std::error_code ec;
    for (auto& e : fs::directory_iterator(root, ec))
        if (e.path().filename().wstring().starts_with(L"vanilla_")) own.push_back(e.path());
    source.clear();
    for (auto& pack : own) {
        if (readFile(pack / folder / name, bytes) && img::load(bytes, maxSize, out)) return true;
        if (fromArchive(pack / L"__brarchive" / (folder + ".brarchive"), name, bytes) && img::load(bytes, maxSize, out)) return true;
    }
    return false;
}

}
