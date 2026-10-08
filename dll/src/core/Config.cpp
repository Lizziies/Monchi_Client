#include "Config.hpp"
#include "ModuleConfig.hpp"
#include "Bg.hpp"
#include "modules/SelfTest.hpp"
#include "Log.hpp"
#include "Paths.hpp"
#include "gui/Theme.hpp"
#include "modules/Manager.hpp"

#include <json.hpp>

#include <atomic>
#include <cctype>
#include <fstream>
#include <mutex>
#include <windows.h>

using nlohmann::json;

namespace config {

static std::string active = "default";
static std::atomic<bool> dirty{false};
static bool loading = false;
static ModuleConfig storedModules;
static std::mutex fileLock;
static std::string lastText;
static std::filesystem::path lastFile;
static uint64_t generation = 0;
static uint64_t written = 0;
static uint64_t lastAutosave = 0;

static std::string clean(const std::string& name) {
    std::string out;
    for (unsigned char c : name)
        if (std::isalnum(c) || c == ' ' || c == '_' || c == '-' || c >= 0x80) out += char(c);
    if (out.empty()) out = "default";
    return out.substr(0, 64);
}

static std::filesystem::path fileFor(const std::string& name) {
    return paths::configs() / logger::widen(clean(name) + ".json");
}

static json read(const std::filesystem::path& p) {
    std::ifstream in(p);
    if (!in) return json::object();
    auto j = json::parse(in, nullptr, false);
    return j.is_object() ? j : json::object();
}

static bool write(const std::filesystem::path& p, const std::string& text) {
    auto tmp = p;
    tmp += L".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        out << text;
        out.close();
        if (!out) return false;
    }
    return MoveFileExW(tmp.c_str(), p.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}

static json snapshot() {
    json mods = storedModules.snapshot();
    for (auto& m : modules::all()) {
        if (m->replaced()) continue;
        auto saved = m->save();
        // a save taken before the core is up comes from Monchi's own module and must not drop what the core saved
        auto old = mods.find(m->name());
        if (old != mods.end() && old->is_object() && old->contains("flarial") && saved.is_object() && !saved.contains("flarial")) saved["flarial"] = (*old)["flarial"];
        mods[m->name()] = std::move(saved);
    }
    storedModules.load(mods);
    return {{"theme", theme::save()}, {"modules", mods}};
}

// Older snapshots never overwrite newer ones, and a file is only touched when its content changed.
static void store(const std::filesystem::path& file, const std::string& profile, const json& j, uint64_t gen) {
    std::string text = j.dump(2);
    std::scoped_lock g(fileLock);
    if (gen < written) return;
    if (text == lastText && file == lastFile) { written = gen; return; }
    if (!write(file, text) || !write(paths::root() / L"settings.json", json{{"profile", profile}}.dump(2))) {
        dirty = true;
        logger::warn("config: could not save profile {}", profile);
        return;
    }
    written = gen;
    lastText = std::move(text);
    lastFile = file;
}

static void apply(const json& j) {
    loading = true;
    try {
        if (j.contains("theme") && j["theme"].is_object()) theme::load(j["theme"]);
    } catch (const std::exception& e) {
        logger::warn("config: theme ignored ({})", e.what());
    }
    auto mods = j.contains("modules") && j["modules"].is_object() ? j["modules"] : json::object();
    storedModules.load(mods);
    for (auto& m : modules::all()) {
        std::string key = m->name();
        if (!mods.contains(key)) key = m->formerName();
        if (key.empty() || !mods.contains(key)) continue;
        try {
            m->load(mods[key]);
        } catch (const std::exception& e) {
            logger::warn("config: settings of {} ignored ({})", m->name(), e.what());
        }
    }
    loading = false;
}

void load() {
    auto settings = read(paths::root() / L"settings.json");
    active = clean(settings.contains("profile") && settings["profile"].is_string() ? settings["profile"].get<std::string>() : "default");
    bool firstRun = !std::filesystem::exists(fileFor(active));
    apply(read(fileFor(active)));
    if (firstRun) {
        for (auto& m : modules::all())
            if (m->defaultEnabled()) m->setEnabled(true);
        save();
    }
    dirty = false;
    logger::info("config '{}' loaded", active);
}

json stored(const std::string& module) {
    return storedModules.get(module);
}

void save() {
    if (selftest::active()) return;
    dirty = false;
    store(fileFor(active), active, snapshot(), ++generation);
}

// The snapshot is taken here on the render thread, the slow part (formatting and disk) runs in the background.
void saveLater() {
    if (selftest::active()) return;
    dirty = false;
    auto j = snapshot();
    auto file = fileFor(active);
    uint64_t gen = ++generation;
    bg::run([j = std::move(j), file, profile = active, gen] { store(file, profile, j, gen); });
}

void saveIfDirty() {
    if (dirty) saveLater();
}

void tick() {
    uint64_t now = GetTickCount64();
    if (now - lastAutosave < 1000) return;
    lastAutosave = now;
    saveIfDirty();
}

void markDirty() {
    if (!loading) dirty = true;
}

void requestSave() { dirty = true; }

const std::string& profile() { return active; }

std::vector<std::string> profiles() {
    std::vector<std::string> out;
    std::error_code ec;
    for (auto& e : std::filesystem::directory_iterator(paths::configs(), ec)) {
        if (e.path().extension() == L".json") out.push_back(logger::narrow(e.path().stem().wstring()));
    }
    if (out.empty()) out.push_back(active);
    return out;
}

void switchProfile(const std::string& name) {
    if (name.empty() || name == active) return;
    save();
    active = clean(name);
    auto j = read(fileFor(name));
    if (!j.empty()) apply(j);
    theme::applyStyle();
    save();
}

void deleteProfile(const std::string& name) {
    if (name == active) return;
    std::error_code ec;
    std::filesystem::remove(fileFor(name), ec);
}

}
