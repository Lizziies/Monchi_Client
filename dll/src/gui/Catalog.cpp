#include "Catalog.hpp"
#include "I18n.hpp"
#include "core/Log.hpp"
#include "modules/Manager.hpp"

#include <algorithm>
#include <set>

namespace gui {

namespace {

struct Spec {
    const char* name;
    const char* blurb;
    Section section;
    std::vector<const char*> members;
};

Spec one(const char* name, Section section) { return {name, "", section, {name}}; }

// The order inside a section is the order of the menu: what decides a fight comes first.
const std::vector<Spec>& specs() {
    using S = Section;
    static const std::vector<Spec> list = {
        one("Crystal Optimizer", S::Pvp),
        one("Low Latency", S::Pvp),
        {"Sprint & Sneak", "Toggle sprint and sneak, straight into the game's input", S::Pvp, {"Toggle Sprint", "Toggle Sneak"}},
        one("Hitbox", S::Pvp),
        {"Combat Info", "Reach, combo, hit ping, target and fight stats", S::Pvp,
         {"Reach Counter", "Combo Counter", "Hit Ping", "Target HUD", "Opponent Reach", "Hit Counter", "Hit Info", "Session Stats",
          "Cooldown Indicator", "Bow Charge"}},
        {"Item Counters", "Pots, totems, arrows and any item you pick", S::Pvp, {"Pot Counter", "Totem Counter", "Arrow Counter", "Item Counter"}},
        {"Hit Feedback", "Markers, sounds, particles and effects when you hit", S::Pvp,
         {"Hit Marker", "Hit Sound", "Hit Effects", "Damage Indicator", "Kill Effects", "Totem Pop", "Totem Helper"}},
        one("Low Health Indicator", S::Pvp),
        {"Aim & Input", "Movement keys, sensitivity, clicks and hotbar keys", S::Pvp,
         {"Null Movement", "Modern Keybind Handling", "Sens Multiplier", "Bow Sensitivity", "Snap Look", "CPS Limiter",
          "Disable Mouse Wheel", "Hotbar Keys"}},
        one("Kill Cleanup", S::Pvp),

        {"Keystrokes", "Keys, mouse buttons, CPS and mouse movement", S::Hud, {"Keystrokes", "CPS", "Mouse Strokes"}},
        one("FPS", S::Hud),
        one("Armor HUD", S::Hud),
        one("Potion HUD", S::Hud),
        {"Player Info", "Coordinates, direction, speed, health and XP", S::Hud,
         {"Coordinates", "Direction HUD", "Speed Display", "Health Display", "Experience Info", "Fall Predictor", "Look Angles", "Day Counter"}},
        {"Inventory HUD", "Inventory, armor bar, held item, item tracker, paperdoll and hunger", S::Hud,
         {"Inventory Viewer", "Held Item", "Item Tracker", "Paperdoll", "Durability Warning"}},
        {"Clock & Timers", "Clock, stopwatch, session time and pomodoro", S::Hud, {"Clock", "Stopwatch", "Session Timer", "Pomodoro"}},
        {"Game HUD", "Scoreboard, tab list and hotbar animation", S::Hud,
         {"Scoreboard", "Tab List", "Hotbar Animation", "Item Size"}},
        {"World Info", "What you look at, break progress, TNT and entities", S::Hud,
         {"Waila", "Break Progress", "TNT Timer", "Entity Counter", "Chunk Border", "Death Logger"}},
        {"Info Displays", "Ping, server, packs, memory and watermark", S::Hud,
         {"Ping Counter", "Server Display", "IP Display", "Pack Display", "Memory", "Stats HUD", "Watermark"}},

        one("Zoom", S::Visual),
        one("Fullbright", S::Visual),
        one("Custom Crosshair", S::Visual),
        one("Block Outline", S::Visual),
        one("Particle Filter", S::Visual),
        {"Camera", "Field of view, dynamic FOV, freelook and perspective", S::Visual,
         {"FOV Changer", "Java Dynamic FOV", "Freelook", "Auto Perspective", "Cinematic Camera"}},
        {"Clean View", "No hurt cam, no bobbing and no hand", S::Visual,
         {"No Hurt Cam", "No View Bobbing", "Hide Hand"}},
        one("View Model", S::Visual),
        {"Screen Filters", "Color, contrast, sharpen and blur", S::Visual,
         {"Saturation / Hue", "Brightness / Contrast", "Screen Tint", "Sharpen", "Color Filter", "Night Shift", "Blur",
          "Depth of Field", "Black Bars"}},
        one("Shader Packs", S::Visual),
        {"Nametags", "Your own nametag and health above heads", S::Visual,
         {"Third Person Nametag", "Health Above Head"}},
        {"Waypoints & Trails", "Waypoints and arrow trails", S::Visual, {"Waypoints", "Arrow Trail"}},

        {"Chat", "Better chat, hotkeys, logger, friend alerts and nick", S::Utility,
         {"Better Chat", "Command Hotkey", "Text Hotkey", "Message Logger", "Player Notifier", "Nick"}},
        {"Inventory", "Inventory lock and inventory hotkeys off", S::Utility, {"Inventory Lock", "Disable Inventory Hotkeys"}},
        one("Auto GG", S::Utility),
        one("Streamer Mode", S::Utility),
        one("Screenshot+", S::Utility),
        {"Tools", "Pack changer, profile and gamemode hotkeys, config sharing, scripts", S::Utility,
         {"Pack Changer", "Profile Hotkeys", "Gamemode Hotkeys", "Config Sharing", "Lua Scripts"}},
        {"Integrations", "Music and Monchi Online", S::Utility, {"Music", "Monchi Online"}},

        one("Performance Lock", S::Performance),
        one("Frame Limiter", S::Performance),
        {"Diagnostics", "Latency, lag analyzer, network monitor and debug menu", S::Performance,
         {"Latency Meter", "Lag Analyzer", "Network Monitor", "Debug Menu", "Background Load", "Mouse Sync", "Game Support"}},
        {"System", "Windows tweaks while you play and an automatic profile", S::Performance, {"System Boost", "Performance Mode", "Auto Profile"}},

        {"The Hive", "Requeue, map avoider, stats and leaderboard", S::Server, {"Hive Utils", "Hive Stats", "Hive Leaderboard"}},
        one("Zeqa Utils", S::Server),
        {"Match Tools", "Server profiles and a match summary", S::Server, {"Server Profiles", "Match Summary"}},

        {"Mini Games", "Snake, Flappy Heart and Block Game for the queue", S::Extras, {"Snake", "Flappy Heart", "Block Game"}},
        {"Fun", "Pet, petals, DVD screen, deepfry, upside down and PatarHD", S::Extras, {"Pet", "Petals", "DVD Screen", "Deepfry", "Upside Down", "PatarHD"}},
        one("20-20-20", S::Extras),
    };
    return list;
}

Section sectionFor(Category c) {
    switch (c) {
    case Category::Hud: return Section::Hud;
    case Category::Visual: return Section::Visual;
    case Category::Pvp: return Section::Pvp;
    case Category::Comfort: return Section::Utility;
    case Category::Performance: return Section::Performance;
    case Category::Server: return Section::Server;
    default: return Section::Extras;
    }
}

std::vector<Entry> build() {
    std::vector<Entry> out;
    std::set<const Module*> placed;
    for (auto& spec : specs()) {
        Entry e{spec.name, spec.blurb, spec.section, {}, spec.members.size() > 1};
        for (auto* name : spec.members) {
            Module* m = modules::find(name);
            if (!m) {
                logger::warn("menu: '{}' lists unknown module '{}'", spec.name, name);
                continue;
            }
            if (placed.insert(m).second) e.members.push_back(m);
        }
        if (!e.members.empty()) out.push_back(std::move(e));
    }
    int loose = 0;
    for (auto& m : modules::all()) {
        if (m->category() == Category::Client || m->replaced() || placed.count(m.get())) continue;
        out.push_back({m->name(), "", sectionFor(m->category()), {m.get()}, false});
        loose++;
    }
    if (loose) logger::info("menu: {} modules are not sorted in yet and are listed alone", loose);
    std::stable_sort(out.begin(), out.end(), [](const Entry& a, const Entry& b) { return int(a.section) < int(b.section); });
    return out;
}

}

bool locked(const Module& m) { return !m.available() || m.rule() == RuleLevel::Block; }

bool Entry::on() const {
    // tools that are always on (Config Sharing) have no switch and do not keep a group on
    return std::any_of(members.begin(), members.end(), [](Module* m) { return m->userEnabled() && !locked(*m) && !m->alwaysOn(); });
}

// a group switch parks the parts that were on and brings exactly those back
void Entry::toggle() const {
    if (!group) {
        Module& m = *members.front();
        if (!locked(m) && !m.alwaysOn()) m.setEnabled(!m.userEnabled());
        return;
    }
    if (on()) {
        for (auto* m : members)
            if (m->userEnabled() && !locked(*m)) {
                m->setParked(true);
                m->setEnabled(false);
            }
        return;
    }
    bool any = false;
    for (auto* m : members)
        if (m->parked() && !locked(*m)) {
            m->setParked(false);
            m->setEnabled(true);
            any = true;
        }
    if (any) return;
    for (auto* m : members)
        if (!locked(*m)) {
            m->setEnabled(true);
            return;
        }
}

int Entry::enabled() const {
    int n = 0;
    for (auto* m : members)
        if (m->userEnabled() && !locked(*m)) n++;
    return n;
}

int Entry::usable() const {
    int n = 0;
    for (auto* m : members)
        if (!locked(*m)) n++;
    return n;
}

bool Entry::hud() const {
    return std::any_of(members.begin(), members.end(), [](Module* m) { return m->isHud(); });
}

bool Entry::risky() const {
    return std::any_of(members.begin(), members.end(), [](Module* m) { return m->risky() || m->rule() == RuleLevel::Warn; });
}

const char* sectionName(Section s) {
    static const char* names[] = {"PvP", "HUD", "Visual", "Utility", "Performance", "Server", "Extras"};
    return i18n::tr(names[int(s)]);
}

const std::deque<Entry>& catalog() {
    static std::deque<Entry> list;
    static size_t count = static_cast<size_t>(-1);
    if (count != modules::all().size()) {
        // entries stay where they are (the menu keeps pointers to them); an entry that is already there takes over the
        // new members, so a module the Flarial core replaced after the menu was first opened is not left in its group
        for (auto& entry : build()) {
            auto old = std::find_if(list.begin(), list.end(), [&](const Entry& e) { return e.name == entry.name; });
            if (old == list.end()) list.push_back(std::move(entry));
            else old->members = std::move(entry.members);
        }
        count = modules::all().size();
    }
    return list;
}

const Entry* entryOf(const Module& m) {
    for (auto& e : catalog())
        for (auto* x : e.members)
            if (x == &m) return &e;
    return nullptr;
}

}
