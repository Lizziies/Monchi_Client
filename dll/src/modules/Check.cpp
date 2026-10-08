#include "Check.hpp"
#include "Manager.hpp"
#include "core/Build.hpp"
#include "core/Guard.hpp"
#include "core/Paths.hpp"
#include "hook/GameInput.hpp"
#include "modules/flarial/FlarialModules.hpp"
#include "render/Ui.hpp"
#include "sdk/Effects.hpp"
#include "sdk/Game.hpp"
#include "sig/Sigs.hpp"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <fstream>
#include <future>
#include <sstream>
#include <map>

namespace check {

namespace {

double last = 0.0;
double inWorld = 0.0;
std::future<void> pending;

// What the game has delivered since the start. A module that only reads the game (a counter, a HUD line, a sound on a
// hit) can work no better than its source: an event that never arrived although the player did the thing shows that
// everything built on it is dead.
struct Seen {
    std::array<int, 12> events{};
    int hurtCrystal = 0;
    float healthMin = 1e9f, healthMax = -1.f;
    int othersMax = 0, tabMax = 0, boardMax = 0, effectsMax = 0, shotsMax = 0, chatMax = 0;
    int targetEntity = 0, targetBlock = 0, namedBlock = 0;
    bool heldSeen = false, armorSeen = false, sprint = false, sneak = false, use = false, light = false, moved = false;
    int dimension = 0;
    std::string lastServer, lastWorld, lastHeld, lastBlock;
} seen;

// highest cost a module was seen with, in milliseconds per frame (its work each frame plus what it draws)
std::map<std::string, float> peak;

void watch() {
    for (auto& m : modules::all())
        if (m->enabled() && m->costMs > 0.f) {
            float& p = peak[m->name()];
            p = std::max(p, m->costMs);
        }
    auto& st = game::state();
    if (!st.inWorld) return;
    for (auto& e : game::events()) {
        size_t k = size_t(e.kind);
        if (k < seen.events.size()) seen.events[k]++;
        if (e.kind == game::EventKind::Hit && e.crystal) seen.hurtCrystal++;
    }
    auto& p = st.player;
    if (p.statsKnown) {
        seen.healthMin = std::min(seen.healthMin, p.health);
        seen.healthMax = std::max(seen.healthMax, p.health);
    }
    seen.othersMax = std::max(seen.othersMax, int(st.others.size()));
    seen.tabMax = std::max(seen.tabMax, int(st.tab.size()));
    seen.boardMax = std::max(seen.boardMax, int(st.scoreboard.lines.size()));
    seen.effectsMax = std::max(seen.effectsMax, int(p.effects.size()));
    seen.shotsMax = std::max(seen.shotsMax, int(st.shots.size()));
    seen.chatMax = std::max(seen.chatMax, int(st.chat.size()));
    if (st.target.kind == game::Target::Kind::Entity) seen.targetEntity++;
    if (st.target.kind == game::Target::Kind::Block) {
        seen.targetBlock++;
        if (!st.target.name.empty()) {
            seen.namedBlock++;
            seen.lastBlock = st.target.name;
        }
    }
    if (!p.held().empty()) {
        seen.heldSeen = true;
        seen.lastHeld = p.held().name;
    }
    for (auto& a : p.armor) seen.armorSeen = seen.armorSeen || !a.empty();
    seen.sprint = seen.sprint || p.sprinting;
    seen.sneak = seen.sneak || p.sneaking;
    seen.use = seen.use || p.usingItem;
    seen.light = seen.light || st.light.valid();
    seen.moved = seen.moved || std::abs(p.vel.x) + std::abs(p.vel.z) > 0.01f;
    seen.dimension = p.dimension;
    if (!st.server.empty()) seen.lastServer = st.server;
    if (!st.world.name.empty()) seen.lastWorld = st.world.name;
}

void data(std::ostream& out) {
    static const char* names[] = {"hit dealt", "hurt", "kill", "death", "totem pop", "swing", "bow release", "item use", "chat line", "respawn", "hit confirm", "sound"};
    out << "\nWhat the game delivered so far (a 0 next to something you did means everything built on it is dead):\n";
    for (size_t i = 0; i < seen.events.size(); i++) out << std::format("  event {}: {}\n", names[i], seen.events[i]);
    out << std::format("  hits on end crystals: {}\n", seen.hurtCrystal);
    if (seen.healthMax >= 0.f) out << std::format("  own health seen between {:.1f} and {:.1f}\n", seen.healthMin, seen.healthMax);
    else out << "  own health: never read\n";
    out << std::format("  other entities at once (most): {}  |  tab list entries (most): {}  |  scoreboard lines (most): {}\n", seen.othersMax, seen.tabMax, seen.boardMax);
    out << std::format("  potion effects at once (most): {}  |  projectiles at once (most): {}  |  chat lines kept (most): {}\n", seen.effectsMax, seen.shotsMax, seen.chatMax);
    out << std::format("  frames aiming at an entity: {}  |  at a block: {}  |  with the block's name: {} (last '{}')\n", seen.targetEntity, seen.targetBlock, seen.namedBlock, seen.lastBlock);
    out << std::format("  held item read: {} (last '{}')  |  armor read: {}  |  light grid: {}\n", seen.heldSeen ? "yes" : "no", seen.lastHeld, seen.armorSeen ? "yes" : "no", seen.light ? "yes" : "no");
    out << std::format("  seen moving: {}  |  sprinting: {}  |  sneaking: {}  |  using an item: {}\n", seen.moved ? "yes" : "no", seen.sprint ? "yes" : "no", seen.sneak ? "yes" : "no", seen.use ? "yes" : "no");
    out << std::format("  last server '{}', last world '{}', dimension {}\n", seen.lastServer, seen.lastWorld, seen.dimension);
    auto hid = gameinput::hiddenClicks();
    out << std::format("  mouse presses Monchi kept from the game: {} while its menu was open, {} while the window had no focus, {} still held when the menu closed, {} taken by a module, {} by the click limiter\n",
                       hid[0], hid[1], hid[2], hid[3], hid[4]);
    auto shift = gameinput::shiftClickCount();
    out << std::format("  shift-clicks: {}, of them reached the game without the click or the shift: {}\n", shift[0], shift[1]);
}
bool first = false;

std::string line(Module& m, const std::map<std::string, std::vector<fx::Proof>>& effects) {
    std::string out = m.name() + (m.enabled() ? ": on" : ": off");
    if (!m.available()) return out + " | GREY, not available on this version";
    if (!m.enabled()) return out;
    if (m.rule() == RuleLevel::Block) return out + " | BLOCKED by the rules of this server";

    std::string core = flarialModules::proof(m);
    if (!core.empty()) return out + " | " + core + (core.find("missing") != std::string::npos || core.find("off") != std::string::npos ? " -> NOT ACTIVE" : " -> bound, effect not counted");

    auto it = effects.find(m.name());
    if (it != effects.end()) {
        int works = 0, waiting = 0, broken = 0, absent = 0;
        for (auto& p : it->second) {
            std::string state;
            if (!p.found) { state = "not bound on this version, this part of the module does nothing"; absent++; }
            else if (!p.installed) { state = "found, not hooked"; broken++; }
            else if (p.option) { state = "the game's option points at Monchi's value (nothing to count, needs a look in the game)"; works++; }
            else if (!p.calls) { state = "hooked, the game has not run it yet"; waiting++; }
            else if (!p.changed) { state = std::format("hooked, run {} times, value never replaced", p.calls); waiting++; }
            else { state = std::format("run {} times, replaced {} times", p.calls, p.changed); works++; }
            out += std::format(" | {} ({}): {}", p.label, p.sig, state);
        }
        // a part that is not bound is one setting of the module; what the module draws or does elsewhere is not judged here
        out += broken ? " -> BROKEN" : absent && !works && !waiting ? " -> ONE PART MISSING, rest not counted" : absent ? " -> ONE PART MISSING" : works && !waiting ? " -> WORKS" : works ? " -> WORKS IN PART" : " -> NOT SEEN YET";
        return out;
    }

    if (std::string own = m.proof(); !own.empty())
        return out + " | " + own + (own.find(" 0 times") != std::string::npos ? " -> NOT SEEN YET" : " -> MODULE REPORT");

    std::string missing;
    for (auto& s : m.sigs())
        if (!sigs::address(s) && !fx::available(s)) missing += (missing.empty() ? "" : ", ") + s;
    if (!missing.empty()) return out + " | missing: " + missing + " -> BROKEN";
    if (!m.sigs().empty()) return out + " | reads the game through bound signatures -> bound, effect not counted";
    return out + (m.isHud() ? " | draws on the screen only" : " | no game hook of its own");
}

}

static std::string report() {
    std::map<std::string, std::vector<fx::Proof>> effects;
    for (auto& p : fx::proofs())
        if (p.by) effects[p.by].push_back(p);
    SYSTEMTIME t;
    GetLocalTime(&t);
    std::ostringstream out;
    out << std::format("Monchi {} module check, {:02}:{:02}:{:02}, {:.0f} s in a world, server '{}'\n", build::version, t.wHour, t.wMinute, t.wSecond, inWorld,
                       game::state().server);
    out << "WORKS = the game ran the hooked code and the module's value replaced the game's own. NOT SEEN YET = hooked, but the game has not\n"
           "run that code while the module asked for a change (for example no end crystal nearby). BROKEN = found but the hook is missing. ONE PART MISSING = one setting of the module asks for a game function that is not bound on this version.\n\n";
    for (auto& m : modules::all()) {
        out << line(*m, effects);
        if (m->enabled()) out << std::format(" | cost {:.0f} us a frame, highest {:.0f} us", m->costMs * 1000.f, peak.count(m->name()) ? peak[m->name()] * 1000.f : 0.f);
        out << "\n";
    }
    std::vector<std::pair<float, std::string>> costly;
    float sum = 0.f;
    for (auto& m : modules::all())
        if (m->enabled()) {
            costly.push_back({m->costMs, m->name()});
            sum += m->costMs;
        }
    std::sort(costly.rbegin(), costly.rend());
    out << std::format("\nCost of all modules that are on: {:.0f} us a frame. The most expensive:\n", sum * 1000.f);
    for (size_t i = 0; i < costly.size() && i < 12; i++) out << std::format("  {}: {:.0f} us\n", costly[i].second, costly[i].first * 1000.f);
    data(out);
    return out.str();
}

static void save(const std::string& text, const std::filesystem::path& path) {
    std::ofstream out(path, std::ios::trunc);
    if (out) out << text;
}

void write() {
    if (pending.valid()) pending.get();
    save(report(), paths::logs() / L"module-check.txt");
}

void tick() {
    watch();
    double now = ui::time();
    if (game::state().inWorld) inWorld += game::state().dt;
    // soon after a world is entered, then every twenty seconds
    if (!game::state().inWorld || now - last < (first ? 20.0 : 5.0) || inWorld < 5.0) return;
    if (pending.valid()) {
        if (pending.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
        pending.get();
    }
    last = now;
    first = true;
    guard::call("module check report", [] {
        auto text = report();
        auto path = paths::logs() / L"module-check.txt";
        pending = std::async(std::launch::async, [text = std::move(text), path = std::move(path)] {
            save(text, path);
        });
    });
}

}
