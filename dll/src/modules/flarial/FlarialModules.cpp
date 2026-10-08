#include "sdk/Game.hpp"
#include "FlarialModules.hpp"
#include "core/Config.hpp"
#include "core/Log.hpp"
#include "core/Guard.hpp"
#include "core/ModuleMigration.hpp"
#include "flarial/Bridge/Input.hpp"
#include "flarial/Bridge/Modules.hpp"
#include "flarial/Bridge/NativeText.hpp"
#include "flarial/Bridge/NameStyles.hpp"
#include "flarial/Bridge/ScreenRefresh.hpp"
#include "flarial/Bridge/WorldMesh.hpp"
#include "gui/Widgets.hpp"
#include "I18n.hpp"
#include "modules/Manager.hpp"
#include "modules/Module.hpp"
#include "render/Ui.hpp"
#include "server/Rules.hpp"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace flarialModules {

namespace {

struct Api {
    int (*screen)() = nullptr;
    bool (*hotbar)(float*) = nullptr;
    bool (*slotGrids)(float*) = nullptr;
    void (*releaseSlotGrids)() = nullptr;
    bool (*sneak)(int) = nullptr;
    bool (*sneakApplied)() = nullptr;
    unsigned (*attacks)(uintptr_t*, unsigned) = nullptr;
    bool (*zoom)(float) = nullptr;
    bool (*perspective)(int) = nullptr;
    unsigned (*perspectiveCalls)(bool) = nullptr;
    bool (*hideCrosshair)(bool) = nullptr;
    bool (*hideHud)(bool) = nullptr;
    void (*localNametag)(const char*) = nullptr;
    void (*tick)() = nullptr;
    void (*playerPose)(double, float, float, float, float, float, bool) = nullptr;
    MonchiFlarialConfig config = nullptr;
    MonchiFlarialConfig defaults = nullptr;
    MonchiFlarialLoadConfig loadConfig = nullptr;
    MonchiFlarialModuleCount count = nullptr;
    MonchiFlarialModuleAt at = nullptr;
    MonchiFlarialSetEnabled setEnabled = nullptr;
    MonchiFlarialSetBlocked setBlocked = nullptr;
    MonchiFlarialMissing missing = nullptr;
    MonchiFlarialRecord record = nullptr;
    MonchiFlarialSettingCount settingCount = nullptr;
    MonchiFlarialSettingAt settingAt = nullptr;
    MonchiFlarialGet get = nullptr;
    MonchiFlarialSet set = nullptr;
    MonchiFlarialPress press = nullptr;
    MonchiFlarialReleaseModules release = nullptr;
    MonchiFlarialCapture capture = nullptr;

    explicit operator bool() const { return count != nullptr; }
};

Api api;
// the window thread reads these; shutdown clears the pointer and waits until no call is inside the core
std::atomic<MonchiFlarialKey> keyFn{nullptr};
std::atomic<MonchiFlarialMouse> mouseFn{nullptr};
std::atomic<MonchiFlarialHideChat> hideChatFn{nullptr};
std::atomic<bool> privacyChat{false};
std::atomic<bool> replacedChat{false};
std::atomic<MonchiFlarialHideScoreboard> hideScoreboardFn{nullptr};
std::atomic<MonchiFlarialRefreshScreen> refreshScreenFn{nullptr};
std::atomic<MonchiFlarialNativeText> nativeTextFn{nullptr};
std::atomic<MonchiFlarialNativeRows> nativeRowsFn{nullptr};
std::atomic<float (*)(const char*, float)> nativeWidthFn{nullptr};
std::atomic<MonchiFlarialWorldMesh> worldMeshFn{nullptr};
std::atomic<MonchiFlarialNameStyles> nameStylesFn{nullptr};
std::atomic<MonchiFlarialWorldMeshPlace> worldMeshPlaceFn{nullptr};
std::atomic<MonchiFlarialWorldMeshState> worldMeshStateFn{nullptr};
std::atomic<int> keyCalls{0};
bool adopted = false;
bool released = false;

template <class T>
T fn(HMODULE core, const char* name) {
    return reinterpret_cast<T>(GetProcAddress(core, name));
}

bool connect() {
    HMODULE core = GetModuleHandleW(L"MonchiFlarial.dll");
    if (!core) return false;
    Api a;
    a.screen = fn<int (*)()>(core, "monchiFlarialScreen");
    a.hotbar = fn<bool (*)(float*)>(core, "monchiFlarialHotbar");
    a.slotGrids = fn<bool (*)(float*)>(core, "monchiFlarialSlotGrids");
    a.releaseSlotGrids = fn<void (*)()>(core, "monchiFlarialReleaseSlotGrids");
    a.sneak = fn<bool (*)(int)>(core, "monchiFlarialSneak");
    a.sneakApplied = fn<bool (*)()>(core, "monchiFlarialSneakApplied");
    a.attacks = fn<unsigned (*)(uintptr_t*, unsigned)>(core, "monchiFlarialAttacks");
    a.zoom = fn<bool (*)(float)>(core, "monchiFlarialZoom");
    a.perspective = fn<bool (*)(int)>(core, "monchiFlarialPerspective");
    a.perspectiveCalls = fn<unsigned (*)(bool)>(core, "monchiFlarialPerspectiveCalls");
    a.hideCrosshair = fn<bool (*)(bool)>(core, "monchiFlarialHideCrosshair");
    a.hideHud = fn<bool (*)(bool)>(core, "monchiFlarialHideHud");
    a.localNametag = fn<void (*)(const char*)>(core, "monchiFlarialLocalNametag");
    a.tick = fn<void (*)()>(core, "monchiFlarialTick");
    a.playerPose = fn<void (*)(double, float, float, float, float, float, bool)>(core, "monchiFlarialPlayerPoseV2");
    a.config = fn<MonchiFlarialConfig>(core, "monchiFlarialConfig");
    a.defaults = fn<MonchiFlarialConfig>(core, "monchiFlarialDefaults");
    a.loadConfig = fn<MonchiFlarialLoadConfig>(core, "monchiFlarialLoadConfig");
    a.count = fn<MonchiFlarialModuleCount>(core, "monchiFlarialModuleCount");
    a.at = fn<MonchiFlarialModuleAt>(core, "monchiFlarialModuleAt");
    a.setEnabled = fn<MonchiFlarialSetEnabled>(core, "monchiFlarialSetEnabled");
    a.setBlocked = fn<MonchiFlarialSetBlocked>(core, "monchiFlarialSetBlocked");
    a.missing = fn<MonchiFlarialMissing>(core, "monchiFlarialMissing");
    a.record = fn<MonchiFlarialRecord>(core, "monchiFlarialRecord");
    a.settingCount = fn<MonchiFlarialSettingCount>(core, "monchiFlarialSettingCount");
    a.settingAt = fn<MonchiFlarialSettingAt>(core, "monchiFlarialSettingAt");
    a.get = fn<MonchiFlarialGet>(core, "monchiFlarialGet");
    a.set = fn<MonchiFlarialSet>(core, "monchiFlarialSet");
    a.press = fn<MonchiFlarialPress>(core, "monchiFlarialPress");
    a.release = fn<MonchiFlarialReleaseModules>(core, "monchiFlarialReleaseModules");
    a.capture = fn<MonchiFlarialCapture>(core, "monchiFlarialCapture");
    if (!a.config || !a.loadConfig || !a.count || !a.at || !a.setEnabled || !a.setBlocked || !a.record || !a.settingCount || !a.settingAt || !a.get || !a.set || !a.press ||
        !a.release || !a.capture || !a.missing)
        return false;
    api = a;
    keyFn = fn<MonchiFlarialKey>(core, "monchiFlarialKey");
    mouseFn = fn<MonchiFlarialMouse>(core, "monchiFlarialMouse");
    hideChatFn = fn<MonchiFlarialHideChat>(core, "monchiFlarialHideChat");
    hideScoreboardFn = fn<MonchiFlarialHideScoreboard>(core, "monchiFlarialHideScoreboard");
    refreshScreenFn = fn<MonchiFlarialRefreshScreen>(core, "monchiFlarialRefreshScreen");
    nativeTextFn = fn<MonchiFlarialNativeText>(core, "monchiFlarialNativeText");
    nativeRowsFn = fn<MonchiFlarialNativeRows>(core, "monchiFlarialNativeRows");
    nativeWidthFn = fn<float (*)(const char*, float)>(core, "monchiFlarialNativeWidth");
    worldMeshFn = fn<MonchiFlarialWorldMesh>(core, "monchiFlarialWorldMesh");
    nameStylesFn = fn<MonchiFlarialNameStyles>(core, "monchiFlarialNameStyles");
    worldMeshPlaceFn = fn<MonchiFlarialWorldMeshPlace>(core, "monchiFlarialWorldMeshPlace");
    worldMeshStateFn = fn<MonchiFlarialWorldMeshState>(core, "monchiFlarialWorldMeshState");
    return true;
}

// the core calls these "Makes the Minecraft Coordinates movable", which hides that they move the game's own display
std::string describe(const std::string& name, const char* original) {
    if (name == "Movable Coordinates")
        return "Moves the game's own coordinates display (the world setting \"Show Coordinates\"). Monchi's Coordinates module is separate: drag it in the HUD editor.";
    if (name == "Movable Day Counter")
        return "Moves the game's own day counter display (the world setting \"Show Days Played\"). Monchi's Day Counter module is separate: drag it in the HUD editor.";
    if (name == "Movable Title")
        return "Moves the title and subtitle text that servers and commands show in the middle of the screen. Not the Minecraft logo in the menus. Works while you play, not in menus or the inventory.";
    if (name == "Movable Hotbar")
        return "Moves the game's hotbar. Works while you play, not in menus or the inventory.";
    if (name == "Movable Bossbar")
        return "Moves the boss bar at the top of the screen. Works while you play.";
    if (name == "Movable Scoreboard")
        return "Moves the game's own scoreboard on the side of the screen. Works while you play.";
    return original ? original : "";
}

// Flarial names whose feature Monchi already has under another name; equal names count as the same feature too
const std::map<std::string, std::string>& aliases() {
    static const std::map<std::string, std::string> map = {
        {"FreeLook", "Freelook"},
        {"Ping", "Ping Counter"},
        {"Combo", "Combo Counter"},
        {"Nametag", "Third Person Nametag"},
        {"Movable Paperdoll", "Paperdoll"},
        {"DirectionHUD", "Direction HUD"},
        {"PotionHUD", "Potion HUD"},
        {"SnapLook", "Snap Look"},
        {"Hive Statistics", "Hive Stats"},
        {"Java Debug Menu", "Debug Menu"},
        {"Java Hotkeys", "Disable Inventory Hotkeys"},
        {"Low Health", "Low Health Indicator"},
        {"Modern Handling", "Modern Keybind Handling"},
        {"Depth Of Field", "Depth of Field"},
        {"Block Break Indicator", "Break Progress"},
        {"Meds", "20-20-20"},
    };
    return map;
}

// Flarial's own menu and placeholder modules: never shown, kept off in the core. ClickGUI's key would otherwise open
// Flarial's menu on top of Monchi's now that keys are forwarded.
bool internal(const std::string& name) {
    for (const char* n : {"Skin Stealer", "MC GUI Scale", "ClickGUI", "cgui", "main", "name here", "Movable ", "Lewis", "Doom", "Better Hunger Bar", "Clear Scoreboard", "Faster Inventory", "Raw Input Buffer", "Movable Hotbar", "Weather Changer", "Pack Changer", "Saturation", "Motion Blur", "Compact Chat", "Movable Chat", "Movable Scoreboard", "Clear Chat", "Animations", "Render Option", "Mumble Link"})

        if (name == n) return true;
    return false;
}

// legit, but against the rules of many servers (MODULES.md: server-rules); they start off and carry the warning
bool serverRules(const std::string& name) {
    for (const char* n : {"Item Use Delay Fix", "Faster Inventory"})
        if (name == n) return true;
    return false;
}

Module* existing(const std::string& flarialName) {
    if (Module* m = modules::find(flarialName)) return m;
    auto it = aliases().find(flarialName);
    return it == aliases().end() ? nullptr : modules::find(it->second);
}

Category categoryFor(const std::string& name) {
    if (name == "PatarHD") return Category::Fun;
    for (const char* hud : {"Counter", "HUD", "Display", "Keystrokes", "Strokes", "CPS", "FPS", "Memory", "Clock", "Stopwatch", "Coordinates",
                            "Day", "Ping", "Speed", "Paperdoll", "Indicator"})
        if (name.find(hud) != std::string::npos) return Category::Hud;
    if (name.find("Hive") != std::string::npos || name.find("Zeqa") != std::string::npos) return Category::Server;
    return Category::Visual;
}

std::string hexOf(ImVec4 c) {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%02x%02x%02x", int(c.x * 255.f + 0.5f), int(c.y * 255.f + 0.5f), int(c.z * 255.f + 0.5f));
    return buf;
}

ImVec4 colorOf(const char* hex, float alpha) {
    std::string h = hex ? hex : "";
    if (!h.empty() && h[0] == '#') h.erase(0, 1);
    unsigned v = h.size() >= 6 ? std::strtoul(h.substr(0, 6).c_str(), nullptr, 16) : 0xffffffu;
    return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f, alpha};
}

class FlarialModule : public Module {
public:
    FlarialModule(int index, std::string name, std::string description, const Module* replaces = nullptr)
        : Module(replaces ? replaces->name() : name, description, replaces ? replaces->category() : categoryFor(name), {"flarial"}),
          index_(index), migrated_(replaces ? replaces->save() : nlohmann::json::object()) {
        sub(replaces ? replaces->sub() : std::string("Flarial"));
        if (this->name() == "Java View Bobbing") require(unsigned(game::Domain::Player), {"LocalPlayer"});
        if (serverRules(this->name()) || (replaces && replaces->risky())) markRisky();
    }

    int index() const { return index_; }
    // the core's switch as last seen; a toggle queued from Monchi lands a frame later and must not be undone
    bool seen = false;
    int toldBlocked = -1;
    nlohmann::json initialConfig;
    std::optional<bool> pending;
    double requestedAt = 0;
    int asked = 0;

    void onEnable() override {
        pending = true;
        asked = 0;
        requestedAt = ui::time();
        if (api) api.setEnabled(index_, true);
    }

    void onDisable() override {
        pending = false;
        asked = 0;
        requestedAt = ui::time();
        if (api) api.setEnabled(index_, false);
    }

    nlohmann::json save() const override {
        auto saved = preserveModuleSettings(migrated_, Module::save());
        auto previous = config::stored(name());
        saved = preserveModuleSettings(previous, std::move(saved));
        if (previous.contains("flarial")) saved["flarial"] = previous["flarial"];
        if (api) {
            guard::call("flarial config snapshot", [&] {
                const char* text = api.config(index_);
                auto data = text ? nlohmann::json::parse(text, nullptr, false) : nlohmann::json();
                if (data.is_object()) saved["flarial"] = std::move(data);
            });
        }
        return saved;
    }

    void load(const nlohmann::json& saved) override {
        migrated_ = saved;
        if (api && saved.contains("flarial") && saved["flarial"].is_object()) {
            auto text = saved["flarial"].dump();
            if (!api.loadConfig(index_, text.c_str())) logger::warn("flarial settings rejected for {}", name());
        }
        Module::load(saved);
    }

    void resetSettings(const std::function<bool(const Setting&)>& which) override {
        Module::resetSettings(which);
        if (!api.defaults) return;
        guard::call("flarial settings reset", [&] {
            const char* text = api.defaults(index_);
            auto data = text ? nlohmann::json::parse(text, nullptr, false) : nlohmann::json();
            if (!data.is_object()) return;
            for (auto it = data.begin(); it != data.end();) {
                const auto& key = it.key();
                std::string id = key.starts_with("keybind") ? "key" : key;
                Setting setting{id, key, SettingType::Text};
                if (key == "enabled" || key == "favorite" || !which(setting)) it = data.erase(it);
                else ++it;
            }
            auto value = data.dump();
            if (api.loadConfig(index_, value.c_str())) {
                shadows_.clear();
                slots_.clear();
                config::markDirty();
            }
        });
    }

    void drawSettings() override {
        if (!api) {
            widgets::hint("The Flarial core is not running, so this module's settings are not available.");
            return;
        }
        // the core describes the settings during its next frame, so they show one frame after opening
        api.record(index_);
        int n = api.settingCount(index_);
        if (n < 0) return;
        size_t schema = 0;
        for (int s = 0; s < n; s++) {
            MonchiFlarialSetting info{};
            if (!api.settingAt(index_, s, &info)) continue;
            std::string shape = std::to_string(info.kind) + ":" + (info.label ? info.label : "") +
                ":" + std::to_string(info.min) + ":" + std::to_string(info.max);
            for (int o = 0; o < info.optionCount; o++) shape += info.options[o] ? info.options[o] : "";
            schema ^= std::hash<std::string>{}(shape) + size_t(s) + (schema << 6) + (schema >> 2);
        }
        if (int(slots_.size()) != n || schema != schema_) {
            build(n);
            schema_ = schema;
        }
        for (int s = 0; s < n; s++) draw(s);
    }

private:
    struct Slot {
        int first = -1;
        int count = 0;
    };

    Setting& shadow(SettingType type, std::string label) {
        shadows_.push_back(Setting{"s" + std::to_string(shadows_.size()), std::move(label), type});
        return shadows_.back();
    }

    void build(int n) {
        shadows_.clear();
        slots_.assign(size_t(n), {});
        for (int s = 0; s < n; s++) {
            MonchiFlarialSetting info{};
            if (!api.settingAt(index_, s, &info)) continue;
            auto& slot = slots_[size_t(s)];
            slot.first = int(shadows_.size());
            std::string label = info.label ? info.label : "";
            switch (info.kind) {
            case MfToggle: shadow(SettingType::Bool, label); break;
            case MfSlider: {
                auto& f = shadow(SettingType::Float, label);
                f.fmin = info.min;
                f.fmax = info.max;
                f.format = info.max - info.min > 20.f ? "%.0f" : "%.2f";
                break;
            }
            case MfSliderInt: {
                auto& i = shadow(SettingType::Int, label);
                i.imin = int(info.min);
                i.imax = int(info.max);
                break;
            }
            case MfRangeSlider:
                for (const char* end : {" (min)", " (max)"}) {
                    auto& f = shadow(SettingType::Float, label + end);
                    f.fmin = info.min;
                    f.fmax = info.max;
                }
                break;
            case MfTextBox:
            case MfKeybind: shadow(SettingType::Text, label); break;
            case MfDropdown: {
                auto& c = shadow(SettingType::Choice, label);
                for (int o = 0; o < info.optionCount; o++) c.choices.push_back(info.options[o] ? info.options[o] : "");
                break;
            }
            case MfColor:
                shadow(SettingType::Color, label);
                shadow(SettingType::Bool, label + " " + i18n::tr("rainbow"));
                break;
            default: break;
            }
            slot.count = int(shadows_.size()) - slot.first;
        }
    }

    MonchiFlarialValue value(int s, int part) {
        MonchiFlarialValue v{};
        api.get(index_, s, part, &v);
        return v;
    }

    void put(int s, int part, MonchiFlarialValue v) {
        if (api.set(index_, s, part, &v)) config::markDirty();
    }

    void draw(int s) {
        MonchiFlarialSetting info{};
        if (!api.settingAt(index_, s, &info) || !info.visible) return;
        auto& slot = slots_[size_t(s)];
        Setting* first = slot.count > 0 ? &shadows_[size_t(slot.first)] : nullptr;
        if (!first && info.kind >= MfToggle) return;
        switch (info.kind) {
        case MfHeader:
            if (info.label && *info.label) widgets::sectionTitle(info.label);
            return;
        case MfText:
            if (info.label && *info.label) widgets::hint(info.label);
            return;
        case MfButton:
            if (widgets::button(info.label ? info.label : "")) {
                api.press(index_, s);
                config::markDirty();
            }
            return;
        case MfToggle:
            first->b = value(s, 0).flag;
            if (widgets::setting(*first)) put(s, 0, {0.f, first->b, nullptr});
            return;
        case MfSlider:
            first->f = value(s, 0).number;
            if (widgets::setting(*first)) put(s, 0, {first->f, false, nullptr});
            return;
        case MfSliderInt:
            first->i = int(value(s, 0).number);
            if (widgets::setting(*first)) put(s, 0, {float(first->i), false, nullptr});
            return;
        case MfRangeSlider:
            for (int part = 0; part < 2; part++) {
                Setting& r = shadows_[size_t(slot.first + part)];
                r.f = value(s, part).number;
                if (widgets::setting(r)) put(s, part, {r.f, false, nullptr});
            }
            return;
        case MfTextBox:
        case MfKeybind: {
            auto v = value(s, 0);
            first->text = v.text ? v.text : "";
            if (widgets::setting(*first)) put(s, 0, {0.f, false, first->text.c_str()});
            return;
        }
        case MfDropdown: {
            auto v = value(s, 0);
            std::string now = v.text ? v.text : "";
            auto it = std::find(first->choices.begin(), first->choices.end(), now);
            first->i = it == first->choices.end() ? 0 : int(it - first->choices.begin());
            if (widgets::setting(*first) && first->i < int(first->choices.size())) put(s, 0, {0.f, false, first->choices[size_t(first->i)].c_str()});
            return;
        }
        case MfColor: {
            Setting& rainbow = shadows_[size_t(slot.first + 1)];
            first->color = colorOf(value(s, 0).text, value(s, 1).number);
            rainbow.b = value(s, 2).flag;
            if (widgets::setting(*first)) {
                std::string hex = hexOf(first->color);
                put(s, 0, {0.f, false, hex.c_str()});
                put(s, 1, {first->color.w, false, nullptr});
            }
            if (widgets::setting(rainbow)) put(s, 2, {0.f, rainbow.b, nullptr});
            return;
        }
        default: return;
        }
    }

    size_t schema_ = 0;
    int index_;
    nlohmann::json migrated_;
    std::deque<Setting> shadows_;
    std::vector<Slot> slots_;
};

std::vector<FlarialModule*> standIns;
std::vector<int> duplicates;
std::vector<std::string> lockedNames;

void adopt() {
    int n = api.count();
    if (n <= 0) return;
    adopted = true;
    int added = 0;
    int retained = 0;
    for (int i = 0; i < n; i++) {
        MonchiFlarialModule info{};
        if (!api.at(i, &info) || !info.name) continue;
        std::string name = info.name;
        api.setBlocked(i, true);
        if (internal(name)) {
            api.setBlocked(i, true);
            continue;
        }
        Module* own = existing(name);
        // Monchi's own View Model has no hand transform on 1.26.52 (no signature), the core's runs on the renderItem hook
        // Monchi's Freelook patches camera stores whose patterns match nothing on 1.26.52.3; the core's runs on _updatePlayer
        bool coreOwns = name == "Nametag" || name == "View Model" || name == "FreeLook" || name == "Movable Paperdoll";
        if (own && !coreOwns) {
            api.setBlocked(i, true);
            retained++;
            continue;
        }
        auto saved = own ? own->save() : config::stored(name);
        // Monchi's own module knows nothing of the core's settings. They were saved under the same name by the module
        // that replaced it last time; without them every start would put the core's defaults back.
        if (own) {
            auto before = config::stored(own->name());
            if (before.is_object() && before.contains("flarial")) saved["flarial"] = before["flarial"];
        }
        std::string lacking = api.missing(i);
        if (!lacking.empty()) {
            api.setBlocked(i, true);
        }
        auto module = std::make_unique<FlarialModule>(i, name, describe(name, info.description), own);
        if (own) {
            own->replace();
            duplicates.push_back(i);
        }
        auto* raw = module.get();
        if (!lacking.empty()) {
            raw->setUnavailable({lacking});
            lockedNames.push_back(name);
        }
        modules::adopt(std::move(module));
        raw->initialConfig = std::move(saved);
        raw->seen = info.enabled;
        standIns.push_back(raw);
        added++;
    }
    // the server rules have to reach the new modules before any of them is switched on
    rules::refresh();
    for (auto* m : standIns) {
        MonchiFlarialModule info{};
        api.at(m->index(), &info);
        api.setBlocked(m->index(), m->rule() == RuleLevel::Block || !m->available());
        if (m->initialConfig.is_object() && !m->initialConfig.empty()) m->load(m->initialConfig);
        else m->setEnabled(info.enabled && !m->risky());
        m->initialConfig = nullptr;
    }
    logger::info("flarial core: {} modules added, {} of them replace Monchi's own", added, duplicates.size());
    logger::info("module ownership: {} existing Monchi implementations retained; Nametag, Paperdoll, View Model and Freelook use Minecraft rendering through Flarial", retained);
    auto join = [](const std::vector<std::string>& names) {
        std::string out;
        for (auto& n : names) out += (out.empty() ? "" : "; ") + n;
        return out;
    };
    if (!lockedNames.empty()) logger::info("flarial core: not available on this version: {}", join(lockedNames));
}

}

bool hideHud(bool hide) { return api.hideHud && api.hideHud(hide); }
bool zoom(float factor) { return api.zoom && api.zoom(factor); }
bool perspective(int view) { return api.perspective && api.perspective(view); }
unsigned perspectiveCalls(bool changed) { return api.perspectiveCalls ? api.perspectiveCalls(changed) : 0; }
bool hideCrosshair(bool hide) { return api.hideCrosshair && api.hideCrosshair(hide); }

bool hotbar(float* rect) { return api.hotbar && api.hotbar(rect); }
bool slotGrids(float* rects) { return !released && api.slotGrids && api.slotGrids(rects); }
bool sneak(int state) { return api.sneak && api.sneak(state); }
bool sneakApplied() { return api.sneakApplied && api.sneakApplied(); }
unsigned attacks(uintptr_t* out, unsigned capacity) { return api.attacks ? api.attacks(out, capacity) : 0; }

int screen() {
    int value = 0;
    if (!released && api.screen) guard::call("native screen", [&] { value = api.screen(); });
    return value;
}

void sync() {
    if (released) return;
    if (!api) {
        static double next = 0.0;
        if (ui::time() < next) return;
        next = ui::time() + 1.0;
        if (!connect()) return;
    }
    if (!adopted) adopt();
    if (api.tick) api.tick();
    if (api.playerPose) {
        const auto& state = game::state();
        api.playerPose(state.time, state.player.yaw, state.player.pitch, state.player.pos.x, state.player.pos.y, state.player.pos.z, state.inWorld && state.screen == game::Screen::None);
    }
    api.capture(ui::capturing());
    if (api.localNametag) api.localNametag(game::state().inWorld ? game::state().player.name.c_str() : "");
    for (auto* m : standIns) {
        bool blocked = m->rule() == RuleLevel::Block || !m->available();
        // told to the core when it changes, not every frame; the core keeps enforcing it on its side
        if (int(blocked) != m->toldBlocked) {
            bool freed = m->toldBlocked == 1 && !blocked;
            api.setBlocked(m->index(), blocked);
            m->toldBlocked = int(blocked);
            // the core switches a blocked module off on its side; once the server is left it is asked for again,
            // or the module stayed off for good although it still showed as on
            if (freed && m->enabled()) m->onEnable();
        }
        MonchiFlarialModule info{};
        if (!api.at(m->index(), &info)) continue;
        if (!info.enabled && (m->seen || m->pending.value_or(false))) {
            std::string problem = api.missing(m->index());
            if (!problem.empty()) {
                m->pending.reset();
                m->setUnavailable({problem});
                m->setEnabled(false);
                m->seen = false;
                continue;
            }
        }
        if (m->pending) {
            if (info.enabled != *m->pending) {
                // the core applies switches in its own frame, which does not always run while a menu is open
                if (ui::time() - m->requestedAt < 4.0 || ui::capturing()) continue;
                // during the start the core's frame may not run for a while; the wish is repeated before it is dropped
                if (!blocked && m->asked < 8) {
                    m->asked++;
                    m->requestedAt = ui::time();
                    api.setEnabled(m->index(), *m->pending);
                    continue;
                }
                logger::warn("flarial switch was not applied for {}", m->name());
                m->pending.reset();
                m->setEnabled(false);
                continue;
            }
            m->pending.reset();
        }
        if (info.enabled == m->seen) continue;
        m->seen = info.enabled;
        // switched inside the core (Flarial's own key for the module); the stand-in follows
        if (!blocked && !info.enabled && m->enabled()) logger::warn("flarial core switched {} off by itself", m->name());
        if (!blocked && info.enabled != m->enabled()) m->setEnabled(info.enabled);
    }
}

bool key(void* hwnd, unsigned msg, unsigned long long wp, long long lp) {
    keyCalls++;
    auto f = keyFn.load();
    bool eaten = false;
    if (f) guard::call("flarial key", [&] { eaten = f(hwnd, msg, wp, lp); });
    keyCalls--;
    return eaten;
}

bool mouse(const MouseEvent& ev) {
    auto f = mouseFn.load();
    if (!f) return false;
    int button = 4, action = ev.wheel > 0 ? 0x78 : 0x88;
    if (!ev.wheel) {
        switch (ev.button) {
        case MouseButton::Left: button = 1; break;
        case MouseButton::Right: button = 2; break;
        case MouseButton::Middle: button = 3; break;
        case MouseButton::X1: button = 5; break;
        case MouseButton::X2: button = 6; break;
        default: return false;
        }
        action = ev.down ? 1 : 0;
    }
    POINT p{};
    GetCursorPos(&p);
    ScreenToClient(GetForegroundWindow(), &p);
    keyCalls++;
    bool cancelled = false;
    guard::call("flarial mouse", [&] { cancelled = f(button, action, p.x, p.y, ev.dx, ev.dy); });
    keyCalls--;
    return cancelled;
}

void hideChat(bool hide) {
    replacedChat = hide;
    if (auto f = hideChatFn.load()) guard::call("flarial hide chat", [&] { f(hide || privacyChat.load()); });
}

void privateChat(bool hide) {
    privacyChat = hide;
    if (auto f = hideChatFn.load()) guard::call("flarial private chat", [&] { f(hide || replacedChat.load()); });
}

std::string proof(const Module& m) {
    auto* f = dynamic_cast<const FlarialModule*>(&m);
    if (!f) return {};
    if (!api) return "core not loaded";
    MonchiFlarialModule info{};
    if (!api.at(f->index(), &info)) return "the core does not know this module";
    std::string problem;
    guard::call("flarial proof", [&] { problem = api.missing(f->index()); });
    if (!problem.empty()) return "core: missing " + problem;
    return info.enabled ? "core: switched on, its hooks are bound" : "core: switched off";
}

bool refreshScreen() {
    auto f = refreshScreenFn.load();
    if (f) guard::call("flarial refresh screen", [&] { f(); });
    return f != nullptr;
}

void nameStyles(const std::vector<NameStyle>& styles) {
    auto f = nameStylesFn.load();
    if (!f) return;
    std::vector<MonchiNameStyle> out;
    out.reserve(styles.size());
    for (auto& s : styles) out.push_back({s.name.c_str(), s.color, s.prefix.c_str()});
    guard::call("flarial name styles", [&] { f(out.data(), int(out.size())); });
}

void worldMeshPlace(int place) {
    if (auto f = worldMeshPlaceFn.load()) guard::call("flarial world mesh place", [&] { f(place); });
}

bool worldMeshReady() { return worldMeshFn.load() && worldMeshStateFn.load(); }

void worldMesh(const std::vector<WorldBatch>& batches) {
    auto f = worldMeshFn.load();
    if (!f) return;
    static_assert(sizeof(WorldVertex) == sizeof(MonchiWorldVertex));
    std::vector<MonchiWorldBatch> out;
    out.reserve(batches.size());
    for (auto& b : batches) out.push_back({b.texture.c_str(), reinterpret_cast<const MonchiWorldVertex*>(b.vertices.data()), int(b.vertices.size())});
    guard::call("flarial world mesh", [&] { f(out.data(), int(out.size())); });
}

int worldMeshState() {
    int state = 2;
    if (auto f = worldMeshStateFn.load()) guard::call("flarial world mesh state", [&] { state = f(); });
    return state;
}

bool nativeTextReady() { return nativeTextFn.load() && nativeRowsFn.load(); }

bool nativeText(const std::vector<NativeLine>& lines) {
    auto f = nativeTextFn.load();
    if (!f || !nativeTextReady()) return false;
    std::vector<MonchiNativeLine> out;
    out.reserve(lines.size());
    for (auto& l : lines) out.push_back({l.text.c_str(), l.x, l.y, l.width, l.lineHeight, l.color});
    guard::call("flarial native text", [&] { f(out.data(), int(out.size())); });
    return true;
}

float nativeWidth(const std::string& text, float lineHeight) {
    auto f = nativeWidthFn.load();
    float w = 0.f;
    if (f) guard::call("flarial native width", [&] { w = f(text.c_str(), lineHeight); });
    return w;
}

int nativeRows(const std::string& text, float width, float lineHeight) {
    int rows = 0;
    if (auto f = nativeRowsFn.load()) guard::call("flarial native rows", [&] { rows = f(text.c_str(), width, lineHeight); });
    return rows;
}

void hideScoreboard(bool hide) {
    if (auto f = hideScoreboardFn.load()) guard::call("flarial hide scoreboard", [&] { f(hide); });
}

void shutdown() {
    if (auto f = hideChatFn.load()) f(false);
    if (auto f = hideScoreboardFn.load()) f(false);
    hideChatFn = nullptr;
    hideScoreboardFn = nullptr;
    refreshScreenFn = nullptr;
    nativeTextFn = nullptr;
    nativeRowsFn = nullptr;
    nativeWidthFn = nullptr;
    if (auto f = worldMeshFn.load()) f(nullptr, 0);
    if (auto f = nameStylesFn.load()) f(nullptr, 0);
    nameStylesFn = nullptr;
    worldMeshPlaceFn = nullptr;
    worldMeshFn = nullptr;
    worldMeshStateFn = nullptr;
    keyFn = nullptr;
    mouseFn = nullptr;
    for (int i = 0; i < 200 && keyCalls.load() > 0; i++) Sleep(1);
    if (api.releaseSlotGrids) api.releaseSlotGrids();
    if (api) api.release();
    api = {};
    released = true;
}

}
