#include "SelfTest.hpp"
#include "I18n.hpp"
#include "Manager.hpp"
#include "core/Client.hpp"
#include "core/Log.hpp"
#include "hook/Input.hpp"
#include "sdk/Game.hpp"
#include "sig/Image.hpp"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

namespace selftest {

namespace {

constexpr int framesPerModule = 120;

bool on = std::getenv("MONCHI_SELFTEST") != nullptr;
std::mt19937 rng{12345};
size_t index = 0;
int frame = 0;
bool started = false;
std::vector<std::string> failed;
std::vector<std::string> stalled;
std::vector<std::string> slow;
constexpr float costBudgetMs = 1.0f;
double lastTick = 0.0;
std::vector<std::string> skipped;
int tested = 0;

float rnd(float a, float b) { return std::uniform_real_distribution<float>(a, b)(rng); }

void mutate(Module& m) {
    auto& all = m.settings();
    if (all.empty()) return;
    Setting& s = all[std::uniform_int_distribution<size_t>(0, all.size() - 1)(rng)];
    if (s.id == "key" || s.id == "hold") return;
    switch (s.type) {
    case SettingType::Bool: s.b = !s.b; break;
    case SettingType::Float: {
        int pick = std::uniform_int_distribution<int>(0, 3)(rng);
        s.f = pick == 0 ? s.fmin : pick == 1 ? s.fmax : rnd(s.fmin, s.fmax);
        break;
    }
    case SettingType::Int: {
        int pick = std::uniform_int_distribution<int>(0, 3)(rng);
        s.i = pick == 0 ? s.imin : pick == 1 ? s.imax : std::uniform_int_distribution<int>(s.imin, s.imax)(rng);
        break;
    }
    case SettingType::Color: s.color = {rnd(0, 1), rnd(0, 1), rnd(0, 1), rnd(0, 1)}; break;
    case SettingType::Choice:
        if (!s.choices.empty()) s.i = std::uniform_int_distribution<int>(0, (int)s.choices.size() - 1)(rng);
        break;
    case SettingType::Text: s.text = std::string(std::uniform_int_distribution<int>(0, 40)(rng), 'x'); break;
    default: break;
    }
}

void poke(Module& m) {
    static const int keys[] = {'W', 'A', 'S', 'D', VK_SPACE, VK_SHIFT, VK_CONTROL, 'E', VK_ESCAPE, VK_TAB, VK_F5};
    KeyEvent k;
    k.vk = keys[std::uniform_int_distribution<size_t>(0, std::size(keys) - 1)(rng)];
    k.down = std::uniform_int_distribution<int>(0, 1)(rng);
    modules::dispatchKey(k);

    MouseEvent e;
    e.button = std::uniform_int_distribution<int>(0, 1) (rng) ? MouseButton::Left : MouseButton::Right;
    e.down = std::uniform_int_distribution<int>(0, 1)(rng);
    e.wheel = std::uniform_int_distribution<int>(-2, 2)(rng);
    e.dx = std::uniform_int_distribution<int>(-300, 300)(rng);
    e.dy = std::uniform_int_distribution<int>(-300, 300)(rng);
    modules::dispatchMouse(e);
}

const char* sameInGerman[] = {"Client", "Timer", "Ring", "Minecraft (MB)", "System (%)", "Name", "Chunk", "Text", "Yaw / Pitch", "Horizontal",
                              "km/h", "Yaw", "Absorption", "FPS", "Frametime", "CPS", "Ping", "Jitter", "Position", "Hunger", "Combo", "Reach",
                              "RAM", "Server", "Version", "ICMP ping", "UDP port", "Warm", "Retro", "Gamma", "Vignette", "Position X",
                              "Position Y", "Position Z", "Crosshair", "Kills", "Plus", "Chat", "Audio", "Format", "PNG", "JPEG", "Limit",
                              "ICMP-Ping", "RakNet-Ping (UDP)", "UDP-Port", "PvP Max FPS", "Soft Glow", "Vibrant", "Cinematic", "Crisp", "example", "Wind", "The Hive", "Zeqa", "BedWars", "SkyWars", "Treasure Wars", "Ground Wars", "Capture the Flag", "/hub"};

void audit() {
    int missing = 0;
    auto check = [&](const std::string& owner, const std::string& text) {
        if (text.size() < 3 || i18n::known(text.c_str())) return;
        if (std::any_of(std::begin(sameInGerman), std::end(sameInGerman), [&](const char* w) { return text == w; })) return;
        if (std::none_of(text.begin(), text.end(), [](unsigned char c) { return std::isalpha(c); })) return;
        logger::warn("untranslated [{}]: {}", owner, text);
        missing++;
    };
    for (auto& mp : modules::all()) {
        Module& m = *mp;
        check(m.name(), m.description());
        check(m.name(), m.sub());
        for (auto& st : m.settings()) {
            if (st.hidden) continue;
            check(m.name(), st.label);
            for (auto& c : st.choices) check(m.name(), c);
        }
    }
    logger::info("selftest: {} untranslated strings", missing);
}

void finish() {
    audit();
    for (auto& n : stalled) logger::error("selftest STALL: {}", n);
    for (auto& n : slow) logger::error("selftest SLOW: {}", n);
    logger::info("selftest: tested {} modules, {} skipped (locked), {} failed, {} stalled, {} slow", tested, skipped.size(), failed.size(), stalled.size(), slow.size());
    for (auto& n : failed) logger::error("selftest FAIL: {}", n);
    logger::info("selftest done");
    client::requestUnload();
}

}

bool active() { return on; }

void tick() {
    if (!on) return;
    auto& list = modules::all();

    double now = GetTickCount64() / 1000.0;
    if (started && lastTick > 0.0 && now - lastTick > 0.35 && index < list.size()) {
        stalled.push_back(list[index]->name());
        logger::warn("selftest: frame stall of {:.0f} ms in {}", (now - lastTick) * 1000.0, list[index]->name());
    }
    lastTick = now;

    if (!started) {
        started = true;
        if (auto* support = modules::find("Game Support"))
            for (auto& st : support->settings())
                if (st.id == "demo") st.b = true;
        game::setDemo(true);
        modules::refreshSigs();
        std::string why;
        if (!input::inMinecraft()) logger::info("selftest: image resolver skipped, not running inside Minecraft");
        else if (image::selfCheck(why)) logger::info("selftest: image resolver ok");
        else {
            failed.push_back("image resolver");
            logger::error("selftest: image resolver failed: {}", why);
        }
        logger::info("selftest start: {} modules", list.size());
        return;
    }
    if (index >= list.size()) {
        if (index == list.size()) {
            index++;
            finish();
        }
        return;
    }

    Module& m = *list[index];
    const char* only = std::getenv("MONCHI_SELFTEST_ONLY");
    if (only && (std::string(",") + only + ",").find("," + m.name() + ",") == std::string::npos) {
        index++;
        return;
    }
    if (m.name() == "ClickGUI" || m.name() == "Game Support" || !m.available()) {
        if (frame == 0 && !m.available()) {
            skipped.push_back(m.name());
            std::string miss;
            for (auto& n : m.missingSigs()) miss += n + " ";
            logger::info("selftest locked: {} needs {}", m.name(), miss);
        }
        index++;
        frame = 0;
        return;
    }

    if (frame == 0) {
        logger::info("selftest: {}", m.name());
        m.setEnabled(true);
    }
    if (frame % 7 == 3) mutate(m);
    if (frame % 5 == 1) poke(m);

    if (++frame >= framesPerModule) {
        tested++;
        if (m.costMs > costBudgetMs) {
            slow.push_back(m.name());
            logger::warn("selftest: {} takes {:.2f} ms per frame", m.name(), m.costMs);
        }
        if (!m.userEnabled()) {
            failed.push_back(m.name());
            logger::error("selftest: {} was disabled by an error", m.name());
        }
        m.setEnabled(false);
        index++;
        frame = 0;
    }
}

}
