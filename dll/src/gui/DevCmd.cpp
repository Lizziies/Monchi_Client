#include "Gui.hpp"
#include "GuiInternal.hpp"
#include "core/Log.hpp"
#include "core/Paths.hpp"
#include "modules/Manager.hpp"
#include "modules/post/Capture.hpp"
#include "sdk/Explore.hpp"
#include "sdk/Game.hpp"
#include <json.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace gui {

namespace {

bool pendingShot = false;
bool shotRequested = false;
std::string shotName;

void pollShot() {
    if (!pendingShot) return;
    if (!shotRequested) {
        shotRequested = capture::request(capture::Stage::Final);
        return;
    }
    capture::Image img;
    if (!capture::poll(img)) return;
    capture::save(std::move(img), paths::root() / L"out" / (logger::widen(shotName) + L".png"), capture::Format::Png, 100);
    pendingShot = shotRequested = false;
}

}

// development only: a file named dev.cmd in the data folder drives the menu, one command per line
void pollDevCommands() {
    static const bool dev = [] {
        bool on = std::filesystem::exists(paths::dllDir() / L"Monchi.root");
        logger::info("dev channel {} ({})", on ? "on" : "off", logger::narrow(paths::dllDir().wstring()));
        return on;
    }();
    static int tick = 0;
    if (!dev) return;
    pollShot();
    if (++tick % 20) return;

    auto file = paths::root() / L"dev.cmd";
    std::error_code ec;
    if (!std::filesystem::exists(file, ec)) return;
    logger::info("dev file found {}", logger::narrow(file.wstring()));

    std::ifstream in(file);
    std::vector<std::string> lines;
    for (std::string line; std::getline(in, line);) lines.push_back(line);
    in.close();
    std::filesystem::remove(file, ec);

    for (auto& line : lines) {
        std::istringstream ss(line);
        std::string cmd;
        ss >> cmd;
        std::string rest;
        std::getline(ss, rest);
        if (!rest.empty() && rest[0] == ' ') rest.erase(0, 1);

        if (cmd == "open") {
            setOpen(true);
            go(Page::Hub);
        } else if (cmd == "close") {
            setOpen(false);
            setEditingHud(false);
        } else if (cmd == "page") {
            setOpen(true);
            go(rest == "modules" ? Page::Modules : rest == "cosmetics" ? Page::Cosmetics : rest == "settings" ? Page::Settings : Page::Hub);
        } else if (cmd == "settings") {
            setOpen(true);
            go(Page::Settings);
            settingsTab() = std::atoi(rest.c_str());
        } else if (cmd == "module") {
            if (auto* m = modules::find(rest)) showModule(m);
        } else if (cmd == "explore") {
            explore::run(rest);
        } else if (cmd == "hudedit") {
            setEditingHud(true);
        } else if (cmd == "search") {
            setOpen(true);
            go(Page::Modules);
            std::snprintf(searchText(), 64, "%s", rest.c_str());
        } else if (cmd == "demo") {
            if (auto* support = modules::find("Game Support"))
                for (auto& st : support->settings())
                    if (st.id == "demo") st.b = rest != "off";
        } else if (cmd == "audit") {
            std::filesystem::create_directories(paths::root() / L"out");
            std::ofstream out(paths::root() / L"out" / L"modules.txt");
            out << "inWorld=" << game::state().inWorld << " demo=" << game::demo() << '\n';
            for (auto& m : modules::all()) {
                out << (m->available() ? "available" : "locked") << '\t' << m->name();
                for (auto& sig : m->missingSigs()) out << "\tmissing:" << sig;
                out << '\n';
            }
            nlohmann::json report = {{"inWorld", game::state().inWorld}, {"demo", game::demo()},
                                     {"liveDomains", game::state().have}, {"modules", nlohmann::json::array()}};
            for (auto& m : modules::all())
                report["modules"].push_back({{"name", m->name()}, {"available", m->available()},
                                            {"requested", m->userEnabled()}, {"effective", m->enabled()},
                                            {"rule", int(m->rule())}, {"missingSignatures", m->missingSigs()}});
            std::ofstream(paths::root() / L"out" / L"modules.json") << report.dump(2);
        } else if (cmd == "enable") {
            if (auto* m = modules::find(rest)) m->setEnabled(true);
        } else if (cmd == "disable") {
            if (auto* m = modules::find(rest)) m->setEnabled(false);
        } else if (cmd == "cosmetic") {
            cosmeticsDev(rest);
        } else if (cmd == "shot") {
            shotName = rest.empty() ? "shot" : rest;
            pendingShot = true;
            shotRequested = false;
        }
        logger::info("dev command: {}", line);
    }
}

}
