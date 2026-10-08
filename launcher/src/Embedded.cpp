#include "Embedded.hpp"
#include "Build.hpp"
#include "Files.hpp"
#include "Update.hpp"

#include "../res/resource.h"

#include <thread>
#include <windows.h>

#include <cstdint>
#include <cstring>
#include <string_view>

namespace embedded {

namespace {

std::string_view resource(int id) {
    HRSRC found = FindResourceW(nullptr, MAKEINTRESOURCEW(id), RT_RCDATA);
    if (!found) return {};
    HGLOBAL loaded = LoadResource(nullptr, found);
    if (!loaded) return {};
    return {static_cast<const char*>(LockResource(loaded)), SizeofResource(nullptr, found)};
}

std::string fingerprint(std::string_view a, std::string_view b, std::string_view c, std::string_view d) {
    uint64_t h = 1469598103934665603ull;
    for (std::string_view part : {a, b, c, d})
        for (unsigned char c : part) h = (h ^ c) * 1099511628211ull;
    char buf[48];
    std::snprintf(buf, sizeof(buf), "%s-%016llx", build::version, static_cast<unsigned long long>(h));
    return buf;
}

bool safe(const std::string& rel) {
    if (rel.empty() || rel[0] == '/' || rel.find("..") != std::string::npos || rel.find(':') != std::string::npos || rel.find('\\') != std::string::npos) return false;
    return rel.size() > 5 && (rel.ends_with(".json") || rel.ends_with(".png"));
}

template <class T>
bool take(std::string_view& in, T& out) {
    if (in.size() < sizeof(T)) return false;
    std::memcpy(&out, in.data(), sizeof(T));
    in.remove_prefix(sizeof(T));
    return true;
}

void unpack(std::string_view pack, const files::fs::path& root) {
    uint32_t count = 0;
    if (pack.size() < 4 || std::memcmp(pack.data(), "MCOS", 4) != 0) return;
    pack.remove_prefix(4);
    if (!take(pack, count)) return;
    for (uint32_t i = 0; i < count; i++) {
        uint16_t len = 0;
        uint32_t size = 0;
        if (!take(pack, len) || pack.size() < len) return;
        std::string rel(pack.substr(0, len));
        pack.remove_prefix(len);
        if (!take(pack, size) || pack.size() < size) return;
        std::string_view data = pack.substr(0, size);
        pack.remove_prefix(size);
        if (!safe(rel)) continue;
        auto path = root / files::fs::path(files::widen(rel));
        std::error_code ec;
        files::fs::create_directories(path.parent_path(), ec);
        files::write(path, std::string(data));
    }
}

}

// Windows checks a program file the first time it is loaded as code. For files it has never seen that takes over half
// a second on an idle PC and up to 16 seconds inside the starting game, with Monchi's frame standing still that long.
// Loading them once here, while the game is not even up, moves
// that wait out of the game; the second load is immediate.
void warm(std::vector<files::fs::path> libraries) {
    std::thread([libraries = std::move(libraries)] {
        for (auto& file : libraries)
            if (HMODULE seen = LoadLibraryExW(file.c_str(), nullptr, DONT_RESOLVE_DLL_REFERENCES)) FreeLibrary(seen);
    }).detach();
}

bool present() { return !resource(IDR_CLIENT).empty(); }

void cosmetics() {
    std::error_code ec;
    if (files::fs::exists(files::root() / L"cosmetics" / L"index.json", ec)) return;
    unpack(resource(IDR_COSMETICS), files::root() / L"cosmetics");
}

bool install(std::string& error) {
    auto dll = resource(IDR_CLIENT);
    if (dll.empty()) return true;
    auto pack = resource(IDR_COSMETICS);
    auto core = resource(IDR_CORE);
    auto signatures = resource(IDR_SIG_PACK);
    unpack(signatures, files::bin() / L"sigs");

    auto marker = files::bin() / L"embedded.txt";
    std::string want = fingerprint(dll, pack, core, signatures);
    std::error_code ec;
    if (update::newer(update::installedTag(), build::version) && files::fs::exists(files::dll(), ec)) return true;
    if (files::read(marker) == want && files::fs::exists(files::dll(), ec) &&
        (core.empty() || files::fs::exists(files::core(), ec))) return true;
    std::vector<std::pair<files::fs::path, files::fs::path>> staged;
    auto stage = [&](std::string_view data, const files::fs::path& to) {
        auto part = to;
        part += L".part";
        staged.emplace_back(part, to);
        return files::write(part, std::string(data));
    };
    bool ok = stage(dll, files::dll()) && (core.empty() || stage(core, files::core())) &&
        stage(build::version, files::installedTag()) && stage(want, marker);
    if (ok) ok = update::replaceFiles(staged, error);
    else error = "Could not save the client version";
    for (auto& [part, target] : staged) files::fs::remove(part, ec);
    if (!ok) return false;
    unpack(pack, files::root() / L"cosmetics");
    warm({files::dll(), files::core()});
    return true;
}

}
