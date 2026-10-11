#include "I18n.hpp"
#include "Rules.hpp"
#include "core/Bg.hpp"
#include "core/Http.hpp"
#include "core/Log.hpp"
#include "core/Paths.hpp"
#include "gui/Notify.hpp"
#include "hook/Net.hpp"
#include "modules/Manager.hpp"
#include "render/Ui.hpp"
#include "sdk/Game.hpp"
#include "modules/common/Text.hpp"

#include <json.hpp>

#include <algorithm>
#include <atomic>
#include <fstream>
#include <map>
#include <mutex>
#include <thread>

using nlohmann::json;

namespace rules {

struct Server {
    std::string name;
    std::vector<std::string> match;
    std::vector<std::string> resolve;
    std::vector<std::string> block;
    std::vector<std::string> warn;
    std::vector<std::string> warnTags;
    std::map<std::string, std::vector<std::string>> blockOptions;
    std::string rulesUrl;
    std::string notice;
};

static std::mutex lock;
static std::vector<Server> servers;
static std::string loadedFrom = "–";
static std::atomic<bool> reload{false};

static Status state;
static std::string activeIp;
static std::string matched;
static double lastSeen = 0;
static double lastSample = 0;

static std::vector<std::string> strings(const json& j, const char* key) {
    std::vector<std::string> out;
    if (j.contains(key) && j[key].is_array())
        for (auto& v : j[key])
            if (v.is_string()) out.push_back(v.get<std::string>());
    return out;
}

static std::vector<Server> parse(const json& j) {
    std::vector<Server> out;
    if (!j.contains("servers") || !j["servers"].is_array()) return out;
    for (auto& s : j["servers"]) {
        Server srv;
        srv.name = s.value("name", "?");
        srv.match = strings(s, "match");
        srv.resolve = strings(s, "resolve");
        srv.block = strings(s, "block");
        srv.warn = strings(s, "warn");
        srv.warnTags = strings(s, "warnTags");
        srv.rulesUrl = s.value("rules", "");
        srv.notice = s.value("notice", "");
        if (s.contains("blockOptions") && s["blockOptions"].is_object())
            for (auto& [mod, opts] : s["blockOptions"].items()) srv.blockOptions[mod] = strings(s["blockOptions"], mod.c_str());
        out.push_back(std::move(srv));
    }
    return out;
}

static bool tryLoad(const std::string& body, const std::string& from) {
    auto j = json::parse(body, nullptr, false);
    if (j.is_discarded()) return false;
    auto parsed = parse(j);
    if (parsed.empty()) return false;
    std::scoped_lock g(lock);
    servers = std::move(parsed);
    loadedFrom = from;
    reload = true;
    return true;
}

static std::string readFile(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    return in ? std::string(std::istreambuf_iterator<char>(in), {}) : std::string();
}

void init() {
    auto cached = paths::cache() / L"servers.json";
    if (!tryLoad(readFile(cached), "Cache")) tryLoad(readFile(paths::dllDir() / L"servers.json"), "bundled");

    bg::run([cached] {
        if (auto body = http::get(L"raw.githubusercontent.com", http::repoRawPath(L"servers/servers.json"))) {
            if (tryLoad(*body, "GitHub")) {
                std::ofstream out(cached, std::ios::binary | std::ios::trunc);
                out << *body;
            }
        }
        logger::info("server rules from {}", source());
        // Minecraft joins the servers of its own list by address, so Zeqa (40.223.14.30) and Mineville (40.223.14.20)
        // would not be recognised by name. The names every list knows are looked up here once.
        std::vector<std::pair<std::string, std::string>> names{{"zeqa.net", "zeqa.net"}, {"play.inpvp.net", "mineville.org"}};
        {
            std::scoped_lock g(lock);
            for (auto& s : servers)
                for (auto& host : s.resolve)
                    if (!s.match.empty()) names.push_back({host, s.match.front()});
        }
        for (auto& [host, as] : names) net::learn(host, as);
    });
}

static bool hostMatches(const std::string& host, const std::string& pattern) {
    if (host.empty()) return false;
    if (host == pattern) return true;
    return host.size() > pattern.size() && host.ends_with(pattern) && host[host.size() - pattern.size() - 1] == '.';
}

static std::string worldTold;

static const Server* lookup(const std::string& host) {
    for (auto& s : servers)
        for (auto& m : s.match)
            if (hostMatches(host, m)) return &s;
    return nullptr;
}

static bool contains(const std::vector<std::string>& v, const std::string& x) {
    return std::find(v.begin(), v.end(), x) != v.end();
}

static bool contains(const std::vector<std::string>& v, const Module& m) {
    return contains(v, m.name()) || (*m.formerName() && contains(v, std::string(m.formerName())));
}

static void apply(const Server* srv) {
    state.blocked = state.warned = 0;
    state.notes.clear();
    state.notice = srv ? srv->notice : "";
    state.rulesUrl = srv ? srv->rulesUrl : "";

    for (auto& m : modules::all()) {
        RuleLevel level = RuleLevel::Allowed;
        std::string note;
        std::vector<std::string> opts;
        if (srv) {
            if (contains(srv->block, *m)) {
                level = RuleLevel::Block;
                note = i18n::fmt("Not allowed on {}.", srv->name);
            } else {
                bool warn = contains(srv->warn, *m);
                for (auto& tag : srv->warnTags) warn = warn || m->hasTag(tag);
                if (warn) {
                    level = RuleLevel::Warn;
                    note = i18n::fmt("{}: not explicitly allowed, use at your own risk.", srv->name);
                }
            }
            if (auto it = srv->blockOptions.find(m->name()); it != srv->blockOptions.end()) opts = it->second;
        }
        if (level == RuleLevel::Block) {
            state.blocked++;
            state.notes.push_back(i18n::fmt("{}: blocked", m->name()));
        } else if (level == RuleLevel::Warn && m->userEnabled()) {
            state.warned++;
            state.notes.push_back(i18n::fmt("{}: notice", m->name()));
        }
        m->setBlockedOptions(opts);
        if (m->rule() != level || m->ruleNote() != note) m->applyRule(level, note);
    }
}

static void connect(const std::string& ip, const std::string& host) {
    const Server* srv = nullptr;
    {
        std::scoped_lock g(lock);
        if (host.empty() && !matched.empty()) {
            activeIp = ip;
            state.ip = ip;
            return;
        }
        srv = lookup(host);
        matched = srv ? srv->name : "";
        activeIp = ip;
        state.ip = ip;
        state.host = host.empty() ? ip : host;
        state.server = srv ? srv->name : state.host;
        apply(srv);
    }
    logger::info("server: {} ({} / {})", state.server, host, ip);
    if (srv && state.blocked)
        notify::push(srv->name, i18n::fmt("{} modules blocked here.", state.blocked), notify::Kind::Warn);
    modules::dispatchServer({state.server, state.host, true});
}

static void disconnect() {
    logger::info("left server {}", state.server);
    modules::dispatchServer({state.server, state.host, false});
    std::scoped_lock g(lock);
    activeIp.clear();
    matched.clear();
    state = Status{};
    apply(nullptr);
}

void refresh() {
    std::scoped_lock g(lock);
    if (!activeIp.empty()) apply(lookup(state.host));
}

void tick() {
    double now = ui::time();
    if (reload.exchange(false) && !activeIp.empty()) {
        std::scoped_lock g(lock);
        apply(lookup(state.host));
    }
    if (now - lastSample < 1.0) return;
    double window = now - lastSample;
    lastSample = now;

    auto peers = net::drain();
    const net::Peer* best = nullptr;
    for (auto& p : peers)
        if (!best || p.packets > best->packets) best = &p;

    const double rate = best ? best->packets / std::max(window, 0.5) : 0;
    bool active = best && rate >= 12.0;

    if (active) {
        lastSeen = now;
        std::string host = best->host;
        // DNS may have completed before injection. Zeqa publishes its network name in LevelData.
        const auto& world = game::state();
        if (host.empty() && matched.empty() && world.inWorld && text::lower(text::strip(world.world.name)) == "zeqa network")
            host = "zeqa.net";
        if (best->ip != activeIp || (!host.empty() && host != state.host)) connect(best->ip, host);
        // which world the server put the player into, once per address: the trace for "I was in a lobby I never saw"
        if (world.inWorld && worldTold != activeIp + world.world.name) {
            worldTold = activeIp + world.world.name;
            logger::info("server world: '{}' on {} ({}), dimension {}", text::strip(world.world.name), state.server, activeIp, world.player.dimension);
        }
    } else if (!activeIp.empty()) {
        bool stillTalking = std::any_of(peers.begin(), peers.end(), [](auto& p) { return p.ip == activeIp && p.packets > 0; });
        if (stillTalking) lastSeen = now;
        if (now - lastSeen > 6.0) disconnect();
    }
}

Status status() {
    std::scoped_lock g(lock);
    return state;
}

std::string source() {
    std::scoped_lock g(lock);
    return loadedFrom;
}

}
