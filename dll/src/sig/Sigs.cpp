#include "Sigs.hpp"
#include "Database.hpp"
#include "Version.hpp"
#include "Image.hpp"
#include "Scanner.hpp"
#include "core/Bg.hpp"
#include "core/Client.hpp"
#include "core/Http.hpp"
#include "core/Log.hpp"
#include "core/Paths.hpp"
#include "sdk/Memory.hpp"

#include <windows.h>
#include <json.hpp>

#include <atomic>
#include <cstdlib>
#include <format>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>
#include <thread>

using nlohmann::json;

namespace sigs {

static std::mutex lock;
static std::map<std::string, uintptr_t> addresses;
static std::map<std::string, int, std::less<>> offsets;
static Stats current;
static std::atomic<bool> changed{false};

static std::vector<std::string> versionKeys(std::string& display) {
    wchar_t exe[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    DWORD handle = 0;
    DWORD size = GetFileVersionInfoSizeW(exe, &handle);
    std::vector<std::string> keys;
    if (!size) return keys;

    std::vector<BYTE> data(size);
    VS_FIXEDFILEINFO* info = nullptr;
    UINT len = 0;
    if (!GetFileVersionInfoW(exe, 0, size, data.data()) ||
        !VerQueryValueW(data.data(), L"\\", reinterpret_cast<void**>(&info), &len) || !info)
        return keys;

    int a = HIWORD(info->dwFileVersionMS), b = LOWORD(info->dwFileVersionMS);
    int c = HIWORD(info->dwFileVersionLS), d = LOWORD(info->dwFileVersionLS);
    display = std::format("{}.{}.{}.{}", a, b, c, d);
    keys.push_back(std::format("{}.{}.{}", a, b, c / 100));
    keys.push_back(std::format("{}.{}.{}", a, b, c));
    keys.push_back(std::format("{}.{}", a, b));
    return keys;
}

static std::optional<json> readJson(const std::filesystem::path& p) {
    std::ifstream in(p);
    if (!in) return std::nullopt;
    auto j = json::parse(in, nullptr, false);
    if (j.is_discarded()) return std::nullopt;
    return j;
}

static void writeFile(const std::filesystem::path& p, const std::string& body) {
    std::ofstream out(p, std::ios::trunc | std::ios::binary);
    out << body;
}

static std::filesystem::path cacheDir() {
    auto p = paths::cache() / L"sigs";
    std::error_code ec;
    std::filesystem::create_directories(p, ec);
    return p;
}

static std::optional<json> embedded(const std::wstring& file) {
    // the generated .rc quotes each name (they start with a digit), and rc keeps the quotes as part of the name
    HRSRC res = FindResourceW(client::module(), file.c_str(), RT_RCDATA);
    if (!res) res = FindResourceW(client::module(), (L"\"" + file + L"\"").c_str(), RT_RCDATA);
    if (res) {
        HGLOBAL data = LoadResource(client::module(), res);
        const char* bytes = data ? static_cast<const char*>(LockResource(data)) : nullptr;
        DWORD size = SizeofResource(client::module(), res);
        if (bytes && size) {
            auto j = json::parse(bytes, bytes + size, nullptr, false);
            if (!j.is_discarded()) {
                return j;
            }
        }
    }
    return std::nullopt;
}

static std::optional<json> fetch(const std::string& file, std::string& source) {
    auto wfile = logger::widen(file);
    auto bundled = embedded(wfile);
    std::error_code ec;
    if (file == "index.json" || std::filesystem::exists(paths::dllDir() / L"Monchi.root", ec)) {
        if (auto j = readJson(paths::dllDir() / L"sigs" / wfile)) {
            source = "bundled";
            return j;
        }
    }
    if (auto body = http::get(L"raw.githubusercontent.com", http::repoRawPath(L"sigs/" + wfile))) {
        auto j = json::parse(*body, nullptr, false);
        if (!j.is_discarded()) {
            writeFile(cacheDir() / wfile, *body);
            source = "GitHub";
            return bundled ? supplement(std::move(j), *bundled) : j;
        }
    }
    if (auto j = readJson(cacheDir() / wfile)) {
        source = "Cache";
        if (bundled) return supplement(std::move(*j), *bundled);
        return j;
    }
    if (auto j = readJson(paths::dllDir() / L"sigs" / wfile)) {
        source = "bundled";
        return j;
    }
    auto j = embedded(wfile);
    if (j) source = "embedded";
    return j;
}

static std::string pickVersion(const std::vector<std::string>& keys, const json& index) {
    std::vector<std::string> known;
    if (index.contains("versions") && index["versions"].is_array())
        for (auto& v : index["versions"])
            if (v.is_string()) known.push_back(v.get<std::string>());

    return supportedVersion(keys, known);
}

struct Entry {
    std::vector<std::string> patterns;
    std::string rel = "none";
    int ripOffset = 0;
    int ripLength = 0;
    int add = 0;
    // a string the game contains, the function using it, or the vtable holding that function at slot
    std::string anchor;
    int occurrence = 0;
    int slot = 0;
    // a function read from a vtable found by another entry
    std::string vtable;
    int index = 0;

    std::string key() const {
        if (!anchor.empty()) return std::format("anchor:{}|{}|{}|{}", anchor, rel, slot, occurrence);
        return patterns.empty() ? "" : patterns.front();
    }
};

static void merge(const json& db, std::map<std::string, Entry>& sigsOut, std::map<std::string, int, std::less<>>& offsOut) {
    if (db.contains("sigs") && db["sigs"].is_object()) {
        for (auto& [name, v] : db["sigs"].items()) {
            if (v.is_null()) {
                sigsOut.erase(name);
                continue;
            }
            Entry e;
            if (v.is_string()) {
                e.patterns.push_back(v.get<std::string>());
            } else {
                if (v.contains("pattern")) e.patterns.push_back(v["pattern"].get<std::string>());
                if (v.contains("patterns"))
                    for (auto& p : v["patterns"]) e.patterns.push_back(p.get<std::string>());
                e.rel = v.value("rel", "none");
                e.ripOffset = v.value("ripOffset", 0);
                e.ripLength = v.value("ripLength", 0);
                e.add = v.value("add", 0);
                e.anchor = v.value("anchor", "");
                e.occurrence = v.value("occurrence", 0);
                e.slot = v.value("slot", 0);
                e.vtable = v.value("vtable", "");
                e.index = v.value("index", 0);
                if (!e.anchor.empty() && !v.contains("rel")) e.rel = "func";
            }
            sigsOut[name] = e;
        }
    }
    if (db.contains("offsets") && db["offsets"].is_object())
        for (auto& [name, v] : db["offsets"].items())
            if (v.is_number_integer()) offsOut[name] = v.get<int>();
}

static uintptr_t resolve(const Entry& e, uintptr_t hit) {
    if (e.rel == "call" || e.rel == "jmp") return scanner::resolveRip(hit, 1, 5) + e.add;
    if (e.rel == "lea" || e.rel == "mov") return scanner::resolveRip(hit, 3, 7) + e.add;
    if (e.rel == "rip") return scanner::resolveRip(hit, e.ripOffset, e.ripLength) + e.add;
    return hit + e.add;
}

static std::string scanCacheName() {
    wchar_t exe[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    WIN32_FILE_ATTRIBUTE_DATA fa{};
    GetFileAttributesExW(exe, GetFileExInfoStandard, &fa);
    return std::format("scan-{:x}-{:x}{:08x}.json", fa.nFileSizeLow, fa.ftLastWriteTime.dwHighDateTime,
                       fa.ftLastWriteTime.dwLowDateTime);
}

static void load() {
    std::string display = "unknown";
    auto keys = versionKeys(display);
    logger::info("minecraft {}", display);

    std::string indexSource;
    json index = fetch("index.json", indexSource).value_or(json::object());
    std::string version = pickVersion(keys, index);
    if (version.empty()) logger::warn("no compatible signatures for Minecraft {}", display);

    std::map<std::string, Entry> entries;
    std::map<std::string, int, std::less<>> offs;
    std::string source = "no signature file";

    std::vector<json> chain;
    std::string next = version;
    for (int depth = 0; !next.empty() && depth < 32; depth++) {
        std::string src;
        auto db = fetch(next + ".json", src);
        if (!db) {
            logger::warn("sig file {}.json missing", next);
            break;
        }
        if (depth == 0) source = std::format("{} ({})", next, src);
        chain.push_back(*db);
        next = db->value("inherits", "");
    }
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) merge(*it, entries, offs);

    uintptr_t base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    auto regions = scanner::codeRegions(base);
    auto cachePath = cacheDir() / logger::widen(scanCacheName());
    json cache = readJson(cachePath).value_or(json::object());
    json newCache = json::object();

    std::map<std::string, uintptr_t> found;
    image::Image img(base);
    for (auto& [name, e] : entries) {
        if (!e.vtable.empty()) continue;
        std::string key = e.key();
        if (cache.contains(name) && cache[name].value("pattern", "") == key && img.contains(base + cache[name].value("rva", (uint64_t)0))) {
            found[name] = base + cache[name].value("rva", (uint64_t)0);
            newCache[name] = cache[name];
            continue;
        }
        if (!e.anchor.empty()) {
            auto hits = e.rel == "vtable" ? image::anchorVtables(img, e.anchor, e.slot) : image::anchorFuncs(img, e.anchor);
            if (hits.empty() || size_t(e.occurrence) >= hits.size()) {
                logger::warn("sig {}: anchor found {} places", name, hits.size());
                continue;
            }
            if (hits.size() > 1 && e.occurrence == 0) logger::warn("sig {}: anchor is not unique ({} places), using the first", name, hits.size());
            uintptr_t addr = hits[size_t(e.occurrence)] + e.add;
            found[name] = addr;
            newCache[name] = {{"pattern", key}, {"rva", (uint64_t)(addr - base)}};
            continue;
        }
        for (auto& text : e.patterns) {
            auto p = scanner::parse(text);
            if (!p) {
                logger::warn("sig {}: bad pattern", name);
                continue;
            }
            auto hits = scanner::find(*p, regions, 2);
            if (hits.size() == 1) {
                uintptr_t addr = resolve(e, hits[0]);
                found[name] = addr;
                newCache[name] = {{"pattern", key}, {"rva", (uint64_t)(addr - base)}};
                break;
            }
            logger::warn("sig {}: {} hits", name, hits.size());
        }
    }
    for (auto& [name, e] : entries) {
        if (e.vtable.empty()) continue;
        auto from = found.find(e.vtable);
        if (from == found.end() || e.index < 0) {
            logger::warn("sig {}: vtable {} not found", name, e.vtable);
            continue;
        }
        uintptr_t fn = mem::pointer(from->second + uintptr_t(e.index) * 8);
        if (!img.inCode(fn)) {
            logger::warn("sig {}: slot {} of {} is not code", name, e.index, e.vtable);
            continue;
        }
        found[name] = fn + e.add;
    }
    writeFile(cachePath, newCache.dump());
    if (std::getenv("MONCHI_SIGCHECK"))
        for (auto& [name, e] : entries) {
            auto hit = found.find(name);
            if (hit == found.end()) logger::warn("sigcheck {}: not found", name);
            else logger::info("sigcheck {}: ok, rva {:x}", name, hit->second - base);
        }

    std::scoped_lock g(lock);
    addresses = std::move(found);
    offsets = std::move(offs);
    current.gameVersion = display;
    current.source = source;
    current.total = (int)entries.size();
    current.found = (int)addresses.size();
    changed = true;
    logger::info("sigs: {}/{} from {}", current.found, current.total, source);
}

void init() {
    bg::run([] {
        try {
            load();
        } catch (const std::exception& e) {
            logger::error("sig loading failed: {}", e.what());
        }
    });
}

bool takeChanged() { return changed.exchange(false); }

uintptr_t address(const std::string& name) {
    std::scoped_lock g(lock);
    auto it = addresses.find(name);
    if (it != addresses.end()) return it->second;
    // data that is read from memory rather than called ("PlayerStats", "MoveState", ...) is announced by the
    // signature file with "has.<Name>": 1 among the offsets
    auto has = offsets.find("has." + name);
    return has != offsets.end() && has->second > 0 ? 1 : 0;
}

// asked hundreds of times a frame by the live readers: the lookup takes the name as it is, without building a string
int offset(std::string_view name, int fallback) {
    std::scoped_lock g(lock);
    auto it = offsets.find(name);
    return it == offsets.end() ? fallback : it->second;
}

Stats stats() {
    std::scoped_lock g(lock);
    return current;
}

}
