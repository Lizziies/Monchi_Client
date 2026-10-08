// SPDX-License-Identifier: AGPL-3.0-only
// Monchi's view of Flarial's module manager, exported from MonchiFlarial.dll (see Modules.hpp).
#include "Modules.hpp"
#include "SettingsRecorder.hpp"
#include "Policy.hpp"
#include "Values.hpp"
#include "Defaults.hpp"

#include "Events/EventManager.hpp"
#include "Events/Listener.hpp"
#include "Events/Render/RenderEvent.hpp"
#include "Module/Manager.hpp"
#include "Utils/Logger/Logger.hpp"
#include "Utils/Memory/Game/SignatureAndOffsetManager.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace {

struct Recorded {
    std::vector<settingsRecorder::Item> items;
    std::vector<std::vector<const char*>> optionPointers;
};

std::vector<std::shared_ptr<Module>> list;
std::set<Module*> blocked;
// modules whose listener faulted (nes::listener_guard); off for the rest of the session, whatever Monchi asks
std::set<Module*> faulted;
std::mutex faultLock;
std::vector<void*> faults;

void onListenerFault(void* instance) {
    std::scoped_lock g(faultLock);
    faults.push_back(instance);
}

void takeFaults() {
    std::vector<void*> now;
    {
        std::scoped_lock g(faultLock);
        now.swap(faults);
    }
    for (void* p : now)
        for (auto& module : list)
            if (module.get() == p && faulted.insert(module.get()).second) {
                Logger::warn("flarial core: {} crashed and stays off for this session", module->name);
                if (module->isEnabled()) ModuleManager::queueToggle(module, false);
            }
}
// what each module needs from the game (generated, tools/flarial_sigs/re/module_deps.py); a need holds alternatives, each
// a group of signatures that must all resolve
struct Need {
    const char* module;
    const char* why;
    std::vector<std::vector<const char*>> alternatives;
};

const std::vector<Need> needs = {
#include "Needs.inc"
};

bool resolved(const char* sig) {
    return Mgr.getSigAddress(Utils::hash(sig)) != 0;
}

std::string missingFor(const std::string& module) {
    std::string out;
    for (const auto& need : needs) {
        if (module != need.module) continue;
        bool met = std::ranges::any_of(need.alternatives, [](const auto& group) {
            return std::ranges::all_of(group, resolved);
        });
        if (met) continue;
        for (const char* sig : need.alternatives.front())
            if (!resolved(sig)) out += (out.empty() ? "" : ", ") + std::string(sig);
    }
    return out;
}

std::map<Module*, Recorded> recorded;
std::set<Module*> wanted;
std::string scratch;

std::shared_ptr<Module> at(int index) {
    if (list.empty())
        for (auto& [hash, module] : ModuleManager::moduleMap)
            if (module) list.push_back(module);
    return index >= 0 && size_t(index) < list.size() ? list[size_t(index)] : nullptr;
}

// The movable elements are placed in Flarial's own editor, which Monchi does not show: the editor only writes the
// percentages the module reads back every frame, so two sliders give the same control.
void describe(Module* module) {
    module->settingsRender(0.f);
    if (module->name.starts_with("Movable ") && module->settings.getSettingByName<float>("percentageX") &&
        module->settings.getSettingByName<float>("percentageY")) {
        module->addHeader("Position");
        module->addSlider("Horizontal position", "0 = where the game puts it, 1 = far right", "percentageX", 1.f, 0.f, false);
        module->addSlider("Vertical position", "0 = where the game puts it, 1 = bottom", "percentageY", 1.f, 0.f, false);
    }
}

bool describeGuarded(Module* module) {
    __try {
        describe(module);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void record(Module* module) {
    settingsRecorder::items.clear();
    settingsRecorder::active = true;
    bool ok = false;
    try {
        ok = describeGuarded(module);
    } catch (...) {
    }
    settingsRecorder::active = false;
    if (!ok) {
        Logger::warn("flarial core: the settings of {} could not be read", module->name);
        settingsRecorder::items.clear();
    }
    auto& r = recorded[module];
    r.items = std::move(settingsRecorder::items);
    settingsRecorder::items.clear();
    r.optionPointers.clear();
    for (auto& item : r.items) {
        auto& ptrs = r.optionPointers.emplace_back();
        for (auto& o : item.options) ptrs.push_back(o.c_str());
    }
}

// A blocked module is switched off without touching its saved state, and switched on again once the block lifts.
void enforce() {
    takeFaults();
    for (auto& module : list)
        if (blocked.contains(module.get()) && module->isEnabled()) module->onDisable();
}

// Recording and enforcing happen inside the core's frame, where Flarial's own settings page would draw: its
// scroll view and Direct2D clip calls are only valid there.
class Pass : public Listener {
public:
    Pass() { Listen(this, RenderEvent, &Pass::onRender); }
    ~Pass() { Deafen(this, RenderEvent, &Pass::onRender); }

    void onRender(RenderEvent&) {
        enforce();
        for (Module* m : wanted) record(m);
        wanted.clear();
    }
};

std::unique_ptr<Pass> pass;

void ensurePass() {
    nes::listener_guard::onFault = onListenerFault;
    if (!pass) pass = std::make_unique<Pass>();
}

template <class T>
SettingType<T>* findSetting(Module* m, const std::string& key) {
    auto it = m->settings.settings.find(key);
    return it == m->settings.settings.end() ? nullptr : dynamic_cast<SettingType<T>*>(it->second.get());
}

const settingsRecorder::Item* item(int index, int s, Module** owner) {
    auto module = at(index);
    if (!module) return nullptr;
    auto it = recorded.find(module.get());
    if (it == recorded.end() || s < 0 || size_t(s) >= it->second.items.size()) return nullptr;
    *owner = module.get();
    return &it->second.items[size_t(s)];
}

// Each kind keeps its value either under a key (strings: color "<key>Col", "<key>Opacity", "<key>RGB") or in a
// variable of the module the recorder pointed at.
bool read(Module* m, const settingsRecorder::Item& it, int part, MonchiFlarialValue* out) {
    using K = settingsRecorder::Kind;
    if (!out || part < 0 || part > (it.kind == K::Color ? 2 : it.kind == K::RangeSlider ? 1 : 0)) return false;
    *out = {};
    switch (it.kind) {
    case K::Toggle:
        if (it.ptr) out->flag = *static_cast<bool*>(it.ptr);
        else if (auto* v = findSetting<bool>(m, it.key)) out->flag = v->value;
        else return false;
        return true;
    case K::Slider:
        if (it.ptr) out->number = *static_cast<float*>(it.ptr);
        else if (auto* v = findSetting<float>(m, it.key)) out->number = v->value;
        else return false;
        return true;
    case K::SliderInt:
        if (auto* v = findSetting<int>(m, it.key)) out->number = float(v->value);
        else if (auto* f = findSetting<float>(m, it.key)) out->number = f->value;
        else return false;
        return true;
    case K::RangeSlider:
        if (auto* v = findSetting<float>(m, part == 0 ? it.key : it.key2)) out->number = v->value;
        else return false;
        return true;
    case K::TextBox:
    case K::Dropdown:
    case K::Keybind:
        if (it.ptr) scratch = *static_cast<std::string*>(it.ptr);
        else if (auto* v = findSetting<std::string>(m, it.key)) scratch = v->value;
        else return false;
        out->text = scratch.c_str();
        return true;
    case K::Color:
        if (part == 0) {
            if (it.ptr) scratch = *static_cast<std::string*>(it.ptr);
            else if (auto* v = findSetting<std::string>(m, it.key + "Col")) scratch = v->value;
            else return false;
            out->text = scratch.c_str();
        } else if (part == 1) {
            if (it.ptr2) out->number = *static_cast<float*>(it.ptr2);
            else if (auto* v = findSetting<float>(m, it.key + "Opacity")) out->number = v->value;
            else return false;
        } else {
            if (it.ptr3) out->flag = *static_cast<bool*>(it.ptr3);
            else if (auto* v = findSetting<bool>(m, it.key + "RGB")) out->flag = v->value;
            else return false;
        }
        return true;
    default:
        return false;
    }
}

template <class T>
bool put(Module* m, void* ptr, const std::string& key, const T& value) {
    if (ptr) {
        *static_cast<T*>(ptr) = value;
        return true;
    }
    if (auto* v = findSetting<T>(m, key)) {
        v->value = value;
        return true;
    }
    return false;
}

bool write(Module* m, const settingsRecorder::Item& it, int part, const MonchiFlarialValue& in) {
    using K = settingsRecorder::Kind;
    if (part < 0 || part > (it.kind == K::Color ? 2 : it.kind == K::RangeSlider ? 1 : 0)) return false;
    if (!std::isfinite(in.number)) return false;
    switch (it.kind) {
    case K::Toggle: return put(m, it.ptr, it.key, in.flag);
    case K::Slider: return put(m, it.ptr, it.key, std::clamp(in.number, it.min, it.max));
    case K::SliderInt:
        if (put(m, nullptr, it.key, int(std::lround(std::clamp(in.number, it.min, it.max))))) return true;
        return put(m, nullptr, it.key, std::round(std::clamp(in.number, it.min, it.max)));
    case K::RangeSlider: return put(m, nullptr, part == 0 ? it.key : it.key2, std::clamp(in.number, it.min, it.max));
    case K::TextBox:
    case K::Dropdown:
    case K::Keybind: return in.text && put(m, it.ptr, it.key, std::string(in.text));
    case K::Color:
        if (part == 0) return in.text && put(m, it.ptr, it.key + "Col", std::string(in.text));
        if (part == 1) return put(m, it.ptr2, it.key + "Opacity", std::clamp(in.number, 0.f, 1.f));
        return put(m, it.ptr3, it.key + "RGB", in.flag);
    default: return false;
    }
}

}

bool coreNeedsDraw() {
    if (!wanted.empty()) return true;
    for (const auto& [hash, module] : ModuleManager::moduleMap)
        if (module && module->isEnabled() &&
            (module->name == "Coordinates" || module->name == "Force Coordinates" ||
             module->name == "Subtitles" || module->name == "Movable Bossbar")) return true;
    return false;
}

extern "C" {

__declspec(dllexport) unsigned monchiFlarialAbi() { return 1; }

__declspec(dllexport) int monchiFlarialModuleCount() {
    if (!ModuleManager::initialized) return 0;
    at(-1);
    ensurePass();
    return int(list.size());
}

__declspec(dllexport) bool monchiFlarialModuleAt(int index, MonchiFlarialModule* out) {
    auto m = at(index);
    if (!m) return false;
    *out = {m->name.c_str(), m->description.c_str(), m->isEnabled()};
    return true;
}

// the signatures this module's hooks and calls need that the running game does not have; empty when it can work
__declspec(dllexport) const char* monchiFlarialMissing(int index) {
    auto m = at(index);
    if (!m) return "";
    scratch = faulted.contains(m.get()) ? "Module faulted; restart Minecraft before trying again." : missingFor(m->name);
    return scratch.c_str();
}

__declspec(dllexport) void monchiFlarialSetEnabled(int index, bool on) {
    auto m = at(index);
    if (!m || m->isEnabled() == on || (on && (blocked.contains(m.get()) || faulted.contains(m.get())))) return;
    ModuleManager::queueToggle(m, on);
}

__declspec(dllexport) void monchiFlarialSetBlocked(int index, bool block) {
    auto m = at(index);
    if (!m) return;
    modulePolicy::set(m->name, block);
    if (block) {
        blocked.insert(m.get());
        if (m->isEnabled()) m->onDisable();
    }
    else blocked.erase(m.get());
}

__declspec(dllexport) void monchiFlarialRecord(int index) {
    if (auto m = at(index)) wanted.insert(m.get());
}

__declspec(dllexport) int monchiFlarialSettingCount(int index) {
    auto m = at(index);
    if (!m) return -1;
    auto it = recorded.find(m.get());
    return it == recorded.end() ? -1 : int(it->second.items.size());
}

__declspec(dllexport) bool monchiFlarialSettingAt(int index, int s, MonchiFlarialSetting* out) {
    Module* owner = nullptr;
    auto* it = item(index, s, &owner);
    if (!it) return false;
    auto& ptrs = recorded[owner].optionPointers[size_t(s)];
    *out = {int(it->kind), it->label.c_str(), it->subtext.c_str(), it->min, it->max, it->visible, int(ptrs.size()), ptrs.data()};
    return true;
}

__declspec(dllexport) bool monchiFlarialGet(int index, int s, int part, MonchiFlarialValue* out) {
    Module* owner = nullptr;
    auto* it = item(index, s, &owner);
    return it && read(owner, *it, part, out);
}

__declspec(dllexport) bool monchiFlarialSet(int index, int s, int part, const MonchiFlarialValue* value) {
    Module* owner = nullptr;
    auto* it = item(index, s, &owner);
    return it && value && write(owner, *it, part, *value);
}

__declspec(dllexport) void monchiFlarialPress(int index, int s) {
    Module* owner = nullptr;
    auto* it = item(index, s, &owner);
    if (it && it->action) it->action();
}

__declspec(dllexport) const char* monchiFlarialDefaults(int index) {
    auto m = at(index);
    if (!m) return nullptr;
    auto it = moduleDefaults::snapshots.find(m.get());
    return it == moduleDefaults::snapshots.end() ? nullptr : it->second.c_str();
}

__declspec(dllexport) const char* monchiFlarialConfig(int index) {
    auto m = at(index);
    if (!m) return nullptr;
    scratch = m->settings.ToJson();
    return scratch.c_str();
}

__declspec(dllexport) bool monchiFlarialLoadConfig(int index, const char* text) {
    auto m = at(index);
    if (!m || !text) return false;
    auto data = json::parse(text, nullptr, false);
    if (!data.is_object()) return false;
    for (auto& [key, setting] : m->settings.settings) {
        if (key == "enabled" || !data.contains(key)) continue;
        auto current = setting->ToJson();
        auto& value = data[key];
        if (!compatibleValue(current, value)) return false;
    }
    for (auto& [key, setting] : m->settings.settings)
        if (key != "enabled" && data.contains(key)) setting->FromJson(data[key]);
    recorded.erase(m.get());
    return true;
}

// drops every reference into Flarial's modules; Monchi calls it before it stops the core
__declspec(dllexport) void monchiFlarialReleaseModules() {
    for (auto& m : list) modulePolicy::set(m->name, true);
    pass.reset();
    wanted.clear();
    recorded.clear();
    blocked.clear();
    list.clear();
}

}
