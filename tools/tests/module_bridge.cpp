#include "core/ModuleConfig.hpp"
#include "core/ModuleMigration.hpp"
#include "flarial/Bridge/Policy.hpp"
#include "flarial/Bridge/Values.hpp"

int main() {
    using nlohmann::json;
    ModuleConfig cache;
    json stored = {{"Zoom", {{"enabled", true}}},
                   {"Item Physics", {{"enabled", true}, {"flarial", {{"speed", 2.5f}}}}}};
    cache.load(stored);
    auto snapshot = cache.snapshot();
    snapshot["Zoom"]["enabled"] = false;
    if (snapshot["Item Physics"] != stored["Item Physics"]) return 1;
    if (cache.get("Item Physics") != stored["Item Physics"]) return 2;
    cache.load(json::object());
    if (!cache.get("Item Physics").empty()) return 3;

    if (modulePolicy::allowed("Freelook")) return 4;
    modulePolicy::set("Freelook", false);
    if (!modulePolicy::allowed("Freelook")) return 5;
    modulePolicy::set("Freelook", true);
    if (modulePolicy::allowed("Freelook")) return 6;

    if (compatibleValue(json(true), json("true"))) return 7;
    if (compatibleValue(json(1.f), json::array())) return 8;
    if (!compatibleValue(json(1.f), json(2))) return 9;
    if (compatibleValue(json("key"), json(false))) return 10;
    auto original = json{{"enabled", true}, {"favorite", true}, {"settings", {{"extra", 3}, {"key", 8}}},
                         {"flarial", {{"scale", 2}}}};
    auto merged = preserveModuleSettings(original, json{{"enabled", false}, {"settings", {{"key", 9}}}});
    if (merged["enabled"] != false || merged["settings"]["extra"] != 3 || merged["settings"]["key"] != 9 ||
        merged.contains("favorite") || merged["flarial"]["scale"] != 2) return 11;
    if (preserveModuleSettings(nullptr, original) != original) return 12;
    return 0;
}
