#include "core/Guard.hpp"
#include "Online.hpp"
#include "Transport.hpp"
#include "UserCache.hpp"
#include "I18n.hpp"
#include "core/Build.hpp"
#include "core/Log.hpp"
#include "core/Paths.hpp"
#include "modules/common/Text.hpp"

#include <json.hpp>

#include <windows.h>
#include <bcrypt.h>
#include <winhttp.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <format>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <utility>

using nlohmann::json;

namespace online {

namespace {

using Clock = std::chrono::steady_clock;

struct Reply {
    int status = 0;
    std::string body;
};

struct Snapshot {
    Config cfg;
    std::vector<std::string> names;
    std::string self;
};

std::mutex lock;
std::condition_variable wake;
HANDLE worker = nullptr;
std::atomic<bool> stopping{false};

Snapshot snap;
Style ownStyle;
std::vector<Worn> ownWorn;
unsigned profileGen = 1;
std::map<std::string, User> known;
UserCache<User> published;
// What the service knows about oneself is as old as the last lookup. For the own name the settings on this machine
// count at once: a new tag color or a heart switched off shows the moment it is set.
std::atomic<std::shared_ptr<const User>> own{nullptr};

// call with the lock held
void publishOwn() {
    own.store(snap.self.empty() ? nullptr : std::make_shared<const User>(User{snap.self, ownStyle, ownWorn, ""}));
}
State current = State::Off;
std::string detail;
bool forgetNow = false;
bool demoDirty = false;

std::string secretKey;

std::string lower(std::string s) {
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

uint32_t fnv(const std::string& s) {
    uint32_t h = 2166136261u;
    for (unsigned char c : s) h = (h ^ c) * 16777619u;
    return h;
}

void setState(State s, std::string text = {}) {
    std::scoped_lock g(lock);
    current = s;
    detail = std::move(text);
}

std::string loadSecret() {
    auto file = paths::root() / L"online.key";
    std::ifstream in(file);
    std::string key;
    if (in && std::getline(in, key) && key.size() == 48) return key;
    unsigned char raw[24];
    if (BCryptGenRandom(nullptr, raw, sizeof(raw), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) return {};
    static const char* digits = "0123456789abcdef";
    key.clear();
    for (unsigned char b : raw) {
        key += digits[b >> 4];
        key += digits[b & 15];
    }
    std::ofstream out(file, std::ios::trunc);
    out << key << "\n";
    return key;
}

Reply post(const std::string& base, const std::string& path, const json& body, const std::string& token) {
    Reply r;
    std::wstring url = logger::widen(base);
    URL_COMPONENTSW parts{};
    parts.dwStructSize = sizeof(parts);
    parts.dwHostNameLength = parts.dwUrlPathLength = parts.dwSchemeLength = (DWORD)-1;
    if (!WinHttpCrackUrl(url.c_str(), (DWORD)url.size(), 0, &parts) || !parts.lpszHostName || !parts.dwHostNameLength) return r;
    if (!parts.lpszUrlPath) parts.dwUrlPathLength = 0;

    std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
    std::wstring prefix(parts.lpszUrlPath ? parts.lpszUrlPath : L"", parts.dwUrlPathLength);
    while (!prefix.empty() && prefix.back() == L'/') prefix.pop_back();
    std::wstring target = prefix + logger::widen(path);
    bool secure = parts.nScheme == INTERNET_SCHEME_HTTPS;
    if (!monchiOnline::safeTransport(secure, parts.nScheme == INTERNET_SCHEME_HTTP, host)) return r;

    HINTERNET session = WinHttpOpen(L"Monchi", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) return r;
    WinHttpSetTimeouts(session, 4000, 4000, 4000, 4000);
    HINTERNET conn = WinHttpConnect(session, host.c_str(), parts.nPort, 0);
    HINTERNET req = conn ? WinHttpOpenRequest(conn, L"POST", target.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, secure ? WINHTTP_FLAG_SECURE : 0) : nullptr;

    std::string payload = body.dump();
    std::wstring headers = L"Content-Type: application/json\r\n";
    if (!token.empty()) headers += L"Authorization: Bearer " + logger::widen(token) + L"\r\n";

    DWORD redirects = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    if (req && WinHttpSetOption(req, WINHTTP_OPTION_REDIRECT_POLICY, &redirects, sizeof(redirects)) &&
        WinHttpSendRequest(req, headers.c_str(), (DWORD)-1, payload.data(), (DWORD)payload.size(), (DWORD)payload.size(), 0) && WinHttpReceiveResponse(req, nullptr)) {
        DWORD status = 0, size = sizeof(status);
        WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
        r.status = (int)status;
        std::array<char, 16384> chunk;
        bool complete = false;
        for (;;) {
            DWORD got = 0;
            if (!WinHttpReadData(req, chunk.data(), DWORD(chunk.size()), &got)) break;
            if (!got) { complete = true; break; }
            if (r.body.size() + got > (256u << 10)) break;
            r.body.append(chunk.data(), got);
        }
        if (!complete) { r.status = 0; r.body.clear(); }
    }
    if (req) WinHttpCloseHandle(req);
    if (conn) WinHttpCloseHandle(conn);
    WinHttpCloseHandle(session);
    return r;
}

const char* const modeNames[] = {"solid", "gradient", "rainbow", "pulse"};

json styleJson(const Style& s) {
    return {{"mode", modeNames[(int)s.mode]}, {"a", hex(s.a)}, {"b", hex(s.b)}, {"speed", s.speed}, {"heartColor", hex(s.heartColor)}, {"heart", s.heart}, {"tag", s.tag}, {"tagColor", hex(s.tagColor)}};
}

json wornJson(const std::vector<Worn>& list) {
    json out = json::array();
    for (auto& w : list) {
        json tint = json::array();
        for (auto c : w.tint) tint.push_back(hex(c));
        out.push_back({{"id", w.id}, {"tint", tint}});
    }
    return out;
}

bool validId(const std::string& id) {
    return !id.empty() && id.size() <= 40 && std::all_of(id.begin(), id.end(), [](unsigned char c) { return std::islower(c) || std::isdigit(c) || c == '_'; });
}

Style styleFrom(const json& j) {
    Style s;
    if (!j.is_object()) return s;
    std::string mode = j.value("mode", "solid");
    for (int i = 0; i < 4; i++)
        if (mode == modeNames[i]) s.mode = Mode(i);
    s.a = parseHex(j.value("a", ""), s.a);
    s.b = parseHex(j.value("b", ""), s.b);
    s.speed = std::clamp(j.value("speed", 1.f), 0.1f, 5.f);
    s.heartColor = parseHex(j.value("heartColor", ""), s.heartColor);
    s.heart = j.value("heart", true);
    // The tag is the one free text another user can put on this screen. It is taken as plain letters only: no control
    // characters, and no section sign, with which the game would change color or scramble the name behind it.
    s.tag = j.value("tag", "");
    bool plain = s.tag.size() <= 96;
    for (size_t i = 0; plain && i < s.tag.size(); i++) {
        unsigned char c = static_cast<unsigned char>(s.tag[i]);
        if (c < 0x20 || c == 0x7f || (c == 0xc2 && i + 1 < s.tag.size() && static_cast<unsigned char>(s.tag[i + 1]) == 0xa7)) plain = false;
    }
    if (!plain) s.tag.clear();
    s.tagColor = parseHex(j.value("tagColor", ""), s.tagColor);
    return s;
}

std::vector<Worn> wornFrom(const json& j) {
    std::vector<Worn> out;
    if (!j.is_array()) return out;
    for (auto& w : j) {
        if (out.size() >= 12 || !w.is_object() || !w.contains("id") || !w["id"].is_string()) continue;
        Worn item;
        item.id = w["id"].get<std::string>();
        if (!validId(item.id)) continue;
        if (w.contains("tint") && w["tint"].is_array())
            for (auto& c : w["tint"])
                if (c.is_string() && item.tint.size() < 8) item.tint.push_back(parseHex(c.get<std::string>(), 0xffffff));
        out.push_back(std::move(item));
    }
    return out;
}

User userFrom(const json& j) {
    User u;
    u.name = j.value("name", "");
    u.style = styleFrom(j.value("style", json::object()));
    u.worn = wornFrom(j.value("worn", json::array()));
    if (auto r = j.value("role", ""); r == "owner" || r == "staff") u.role = r;
    return u;
}

const uint32_t demoColors[] = {0xff7eb6, 0x7ec8ff, 0xffd27e, 0x8be8b0, 0xc77dff, 0xff6b6b};

bool demoMember(const std::string& name) {
    static const char* fixed[] = {"luna", "kiki", "teammate", "finn", "monchiplayer"};
    std::string n = lower(name);
    return std::any_of(std::begin(fixed), std::end(fixed), [&](const char* f) { return n == f; }) || fnv(n) % 6 == 0;
}

User demoUser(const std::string& name) {
    uint32_t h = fnv(lower(name));
    User u;
    u.name = name;
    u.style.mode = Mode(h % 4);
    u.style.a = demoColors[(h >> 3) % 6];
    u.style.b = demoColors[(h >> 7) % 6];
    u.style.speed = 0.6f + float((h >> 11) % 8) * 0.2f;
    u.style.heart = (h >> 9) % 7 != 0;
    if (h % 3 == 0) u.worn.push_back({"sakura_wings", {u.style.a, 0xffffff}});
    return u;
}

void rebuildDemo(const Snapshot& s) {
    std::map<std::string, User> fresh;
    for (auto& n : s.names)
        if (demoMember(n)) fresh[lower(n)] = demoUser(n);
    std::scoped_lock g(lock);
    if (s.cfg.visible && !s.self.empty()) fresh[lower(s.self)] = User{s.self, ownStyle, ownWorn};
    known = std::move(fresh);
    published.replace(known);
    current = State::Demo;
    detail = i18n::tr("Showing made-up Monchi users, nothing is sent.");
}

struct Net {
    std::string token;
    std::string server;
    unsigned sentGen = 0;
    bool sentVisible = false;
    Clock::time_point nextTry{};
    Clock::time_point presenceAt{};
    Clock::time_point lookupAt{};
    std::map<std::string, Clock::time_point> asked;
    std::string url;
};

void fail(Net& net, int status, int backoff) {
    if (status == 401) net.token.clear();
    if (status == 403) setState(State::Claimed, i18n::tr("This gamertag is registered by another Monchi install."));
    else setState(State::Offline, status ? i18n::fmt("Service answered {}.", status) : i18n::tr("Service not reachable."));
    net.nextTry = Clock::now() + std::chrono::seconds(backoff);
}

void hello(Net& net, const Snapshot& s, const Style& st, const std::vector<Worn>& wn, unsigned gen) {
    json body = {{"name", s.self}, {"client", build::version}, {"secret", secretKey}, {"visible", s.cfg.visible}, {"style", styleJson(st)}, {"worn", wornJson(wn)}};
    Reply r = post(net.url, "/v1/hello", body, {});
    if (r.status != 200) return fail(net, r.status, r.status == 403 ? 60 : 15);
    auto j = json::parse(r.body, nullptr, false);
    if (j.is_discarded() || !j.contains("token") || !j["token"].is_string()) return fail(net, 0, 30);
    net.token = j["token"].get<std::string>();
    net.sentGen = gen;
    net.sentVisible = s.cfg.visible;
    net.presenceAt = {};
    net.lookupAt = {};
    net.asked.clear();
    setState(State::Online);
    logger::info("online: signed in as {}", s.self);
}

void sendProfile(Net& net, const Snapshot& s, const Style& st, const std::vector<Worn>& wn, unsigned gen) {
    json body = {{"visible", s.cfg.visible}, {"style", styleJson(st)}, {"worn", wornJson(wn)}};
    Reply r = post(net.url, "/v1/profile", body, net.token);
    if (r.status != 200) return fail(net, r.status, 10);
    net.sentGen = gen;
    net.sentVisible = s.cfg.visible;
}

void sendPresence(Net& net, const Snapshot& s) {
    Reply r = post(net.url, "/v1/presence", {{"server", s.cfg.server}}, net.token);
    if (r.status != 200) return fail(net, r.status, 10);
    net.server = s.cfg.server;
    net.presenceAt = Clock::now();
}

std::vector<std::string> due(const Net& net, const Snapshot& s) {
    std::vector<std::string> out;
    auto now = Clock::now();
    std::scoped_lock g(lock);
    for (auto& n : s.names) {
        if (out.size() >= 100) break;
        std::string key = lower(n);
        auto it = net.asked.find(key);
        auto ttl = std::chrono::seconds(known.count(key) ? 30 : 60);
        if (it == net.asked.end() || now - it->second > ttl) out.push_back(n);
    }
    return out;
}

void sendLookup(Net& net, const Snapshot& s, const std::vector<std::string>& asked) {
    json names = json::array();
    for (auto& n : asked) names.push_back(n);
    Reply r = post(net.url, "/v1/lookup", {{"names", names}}, net.token);
    net.lookupAt = Clock::now();
    if (r.status != 200) return fail(net, r.status, 10);
    auto j = json::parse(r.body, nullptr, false);
    if (j.is_discarded() || !j.contains("users") || !j["users"].is_array()) return;
    std::map<std::string, User> found;
    for (auto& e : j["users"]) {
        // a field of the wrong type makes the json library throw; that costs the one entry, not the worker
        try {
            User u = userFrom(e);
            if (!u.name.empty() && u.name.size() <= 32) found[lower(u.name)] = std::move(u);
        } catch (const std::exception&) {
        }
    }
    std::set<std::string> present;
    for (auto& n : s.names) present.insert(lower(n));
    for (auto& n : asked) net.asked[lower(n)] = net.lookupAt;
    for (auto it = net.asked.begin(); it != net.asked.end();) it = present.count(it->first) ? std::next(it) : net.asked.erase(it);

    std::scoped_lock g(lock);
    for (auto& n : asked) known.erase(lower(n));
    for (auto& [k, u] : found) known[k] = std::move(u);
    for (auto it = known.begin(); it != known.end();) it = present.count(it->first) ? std::next(it) : known.erase(it);
    published.replace(known);
    current = State::Online;
    detail.clear();
}

void signOut(Net& net) {
    if (!net.token.empty()) post(net.url, "/v1/bye", json::object(), net.token);
    net.token.clear();
    net.server.clear();
    std::scoped_lock g(lock);
    known.clear();
    published.replace(known);
}

void loop() {
    Net net;
    for (;;) {
        Snapshot s;
        Style st;
        std::vector<Worn> wn;
        unsigned gen = 0;
        bool erase = false;
        {
            std::unique_lock g(lock);
            wake.wait_for(g, std::chrono::milliseconds(300), [] { return stopping.load(); });
            if (stopping) break;
            s = snap;
            st = ownStyle;
            wn = ownWorn;
            gen = profileGen;
            erase = std::exchange(forgetNow, false);
        }
        if (s.cfg.demo) continue;
        if (s.cfg.url != net.url) {
            signOut(net);
            net.url = s.cfg.url;
        }
        if (erase && !net.url.empty() && !net.token.empty()) {
            post(net.url, "/v1/forget", json::object(), net.token);
            net.token.clear();
            std::scoped_lock g(lock);
            known.clear();
            published.replace(known);
        }
        if (!s.cfg.on) {
            if (!net.token.empty()) signOut(net);
            setState(State::Off);
            continue;
        }
        if (net.url.empty()) {
            setState(State::NoUrl, i18n::tr("No service address set."));
            continue;
        }
        if (s.self.empty() || Clock::now() < net.nextTry) continue;

        if (net.token.empty()) {
            if (secretKey.empty()) secretKey = loadSecret();
            if (secretKey.empty()) continue;
            setState(State::Starting);
            hello(net, s, st, wn, gen);
            continue;
        }
        if (net.sentGen != gen || net.sentVisible != s.cfg.visible) {
            sendProfile(net, s, st, wn, gen);
            continue;
        }
        auto now = Clock::now();
        if (net.server != s.cfg.server || now - net.presenceAt > std::chrono::seconds(120)) {
            sendPresence(net, s);
            continue;
        }
        if (now - net.lookupAt > std::chrono::seconds(15)) {
            auto ask = due(net, s);
            if (!ask.empty()) sendLookup(net, s, ask);
        }
    }
    signOut(net);
}

void ensureWorker() {
    if (!worker) worker = CreateThread(nullptr, 0, [](LPVOID) -> DWORD { guard::call("online", [] { loop(); }); return 0; }, nullptr, 0, nullptr);
}

}

std::string hex(uint32_t c) {
    char buf[8];
    snprintf(buf, sizeof(buf), "#%06x", c & 0xffffff);
    return buf;
}

uint32_t parseHex(const std::string& s, uint32_t fallback) {
    if (s.size() != 7 || s[0] != '#') return fallback;
    for (size_t i = 1; i < 7; i++)
        if (!std::isxdigit((unsigned char)s[i])) return fallback;
    return (uint32_t)std::strtoul(s.c_str() + 1, nullptr, 16);
}

const char* modeId(Mode m) { return modeNames[(int)m]; }

ImU32 rgb(uint32_t c, float alpha) { return IM_COL32((c >> 16) & 255, (c >> 8) & 255, c & 255, int(std::clamp(alpha, 0.f, 1.f) * 255.f)); }

ImU32 color(const Style& s, double t, int index, int total) {
    auto mix = [](uint32_t a, uint32_t b, float f) {
        auto ch = [&](int shift) { return int(float((a >> shift) & 255) * (1.f - f) + float((b >> shift) & 255) * f); };
        return IM_COL32(ch(16), ch(8), ch(0), 255);
    };
    float f = total > 1 ? float(index) / float(total - 1) : 0.f;
    switch (s.mode) {
    case Mode::Solid: return rgb(s.a);
    case Mode::Gradient: return mix(s.a, s.b, f);
    case Mode::Rainbow: {
        float h = std::fmod(float(t) * 0.15f * s.speed + float(index) * 0.07f, 1.f);
        float r, g, b;
        ImGui::ColorConvertHSVtoRGB(h, 0.6f, 1.f, r, g, b);
        return IM_COL32(int(r * 255), int(g * 255), int(b * 255), 255);
    }
    case Mode::Pulse: return mix(s.a, s.b, 0.5f + 0.5f * std::sin(float(t) * s.speed * 2.4f));
    }
    return rgb(s.a);
}

void heartIcon(ImDrawList* dl, ImVec2 c, float size, ImU32 col) {
    float r = size * 0.27f;
    dl->AddCircleFilled({c.x - size * 0.25f, c.y - size * 0.14f}, r, col, 14);
    dl->AddCircleFilled({c.x + size * 0.25f, c.y - size * 0.14f}, r, col, 14);
    dl->AddTriangleFilled({c.x - size * 0.51f, c.y - size * 0.05f}, {c.x + size * 0.51f, c.y - size * 0.05f}, {c.x, c.y + size * 0.5f}, col);
}

void tick(const Config& cfg, const std::vector<std::string>& names, const std::string& self) {
    Snapshot next{cfg, names, self};
    bool demoChanged = false;
    {
        std::scoped_lock g(lock);
        demoChanged = cfg.demo && (!snap.cfg.demo || snap.names != names || snap.self != self || snap.cfg.visible != cfg.visible || demoDirty);
        if (snap.cfg.demo && !cfg.demo) {
            known.clear();
            published.replace(known);
        }
        demoDirty = false;
        snap = next;
        publishOwn();
    }
    if (cfg.demo) {
        if (demoChanged) rebuildDemo(next);
        return;
    }
    if (!cfg.on && !worker) {
        setState(State::Off);
        return;
    }
    ensureWorker();
}

void setStyle(const Style& s) {
    std::scoped_lock g(lock);
    if (ownStyle == s) return;
    ownStyle = s;
    profileGen++;
    demoDirty = true;
    publishOwn();
}

void setWorn(const std::vector<Worn>& w) {
    std::scoped_lock g(lock);
    if (ownWorn == w) return;
    ownWorn = w;
    profileGen++;
    demoDirty = true;
    publishOwn();
}

Style style() {
    std::scoped_lock g(lock);
    return ownStyle;
}

std::vector<Worn> worn() {
    std::scoped_lock g(lock);
    return ownWorn;
}

bool find(const std::string& name, User& out) {
    if (auto me = own.load(); me && NameEqual()(me->name, name)) {
        out = *me;
        return true;
    }
    return published.find(name, out);
}

std::vector<User> users() {
    auto snapshot = published.snapshot();
    std::vector<User> out;
    out.reserve(snapshot->size());
    auto me = own.load();
    for (auto& [k, u] : *snapshot)
        if (!me || !NameEqual()(me->name, u.name)) out.push_back(u);
    if (me) out.push_back(*me);
    std::sort(out.begin(), out.end(), [](const User& a, const User& b) { return a.name < b.name; });
    return out;
}

int count() {
    return static_cast<int>(published.snapshot()->size());
}

State state() {
    std::scoped_lock g(lock);
    return current;
}

std::string stateText() {
    std::scoped_lock g(lock);
    switch (current) {
    case State::Off: return i18n::tr("Off");
    case State::NoUrl: return detail;
    case State::Starting: return i18n::tr("Connecting ...");
    case State::Online: return i18n::tr("Connected");
    case State::Demo: return detail;
    case State::Offline:
    case State::Claimed: return detail;
    }
    return {};
}

void forget() {
    std::scoped_lock g(lock);
    forgetNow = true;
}

void shutdown() {
    stopping = true;
    wake.notify_all();
    if (worker) {
        WaitForSingleObject(worker, INFINITE);
        CloseHandle(worker);
        worker = nullptr;
    }
}

const char* nearestCode(uint32_t rgb) {
    static const std::pair<uint32_t, const char*> codes[] = {
        {0x000000, "\xC2\xA7" "0"}, {0x0000AA, "\xC2\xA7" "1"}, {0x00AA00, "\xC2\xA7" "2"}, {0x00AAAA, "\xC2\xA7" "3"},
        {0xAA0000, "\xC2\xA7" "4"}, {0xAA00AA, "\xC2\xA7" "5"}, {0xFFAA00, "\xC2\xA7" "6"}, {0xAAAAAA, "\xC2\xA7" "7"},
        {0x555555, "\xC2\xA7" "8"}, {0x5555FF, "\xC2\xA7" "9"}, {0x55FF55, "\xC2\xA7" "a"}, {0x55FFFF, "\xC2\xA7" "b"},
        {0xFF5555, "\xC2\xA7" "c"}, {0xFF55FF, "\xC2\xA7" "d"}, {0xFFFF55, "\xC2\xA7" "e"}, {0xFFFFFF, "\xC2\xA7" "f"}};
    const char* best = codes[15].second;
    int least = INT_MAX;
    for (auto& [c, code] : codes) {
        int dr = int((c >> 16) & 255) - int((rgb >> 16) & 255), dg = int((c >> 8) & 255) - int((rgb >> 8) & 255), db = int(c & 255) - int(rgb & 255);
        int d = dr * dr + dg * dg + db * db;
        if (d < least) {
            least = d;
            best = code;
        }
    }
    return best;
}

bool decorate(std::string& sender, std::string& body, bool names, bool hearts) {
    if (!names && !hearts) return false;
    std::vector<std::pair<std::string, Style>> people;
    {
        std::scoped_lock g(lock);
        if (current != State::Online && current != State::Demo) return false;
        std::string me = lower(snap.self);
        for (auto& [key, u] : known)
            if (key != me) people.push_back({u.name, u.style});
        if (!snap.self.empty()) people.push_back({snap.self, ownStyle});
    }
    auto wordAt = [](const std::string& text, const std::string& name, size_t limit) {
        for (size_t p = text.find(name); p != std::string::npos && p < limit; p = text.find(name, p + 1)) {
            size_t e = p + name.size();
            bool left = p == 0 || !std::isalnum((unsigned char)text[p - 1]);
            bool right = e >= text.size() || !std::isalnum((unsigned char)text[e]);
            if (left && right) return p;
        }
        return std::string::npos;
    };
    for (auto& [name, style] : people) {
        if (name.empty()) continue;
        std::string front;
        if (names && !style.tag.empty()) front += std::string(nearestCode(style.tagColor)) + style.tag + " ";
        if (hearts && style.heart) front += std::string(nearestCode(style.heartColor)) + "\xE2\x99\xA5 ";
        if (!sender.empty()) {
            if (wordAt(sender, name, sender.size()) == std::string::npos) continue;
            if (sender.find("\xE2\x99\xA5") != std::string::npos) return false;
            sender = front + (names ? nearestCode(style.a) : "\xC2\xA7r") + sender + "\xC2\xA7r";
            return true;
        }
        // servers send the whole line as text: "[Rank] Name: message"
        size_t at = wordAt(body, name, 64);
        if (at == std::string::npos || front.empty()) continue;
        if (body.find("\xE2\x99\xA5") < at + 8) return false;
        // back to the color that was running before the name
        std::string back = "\xC2\xA7r";
        for (size_t p = body.rfind("\xC2\xA7", at); p != std::string::npos && p + 2 < body.size(); p = body.rfind("\xC2\xA7", p - 1)) {
            char c = body[p + 2];
            if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'g')) {
                back = body.substr(p, 3);
                break;
            }
            if (!p) break;
        }
        body.insert(at, front + back);
        return true;
    }
    return false;
}

std::string tagLine(const std::string& line, bool names, bool hearts) {
    if (!names && !hearts) return line;
    std::string plain = text::strip(line);
    size_t end = plain.find_first_of(">:");
    size_t guillemet = plain.find("\xC2\xBB");
    if (guillemet != std::string::npos && (end == std::string::npos || guillemet < end)) end = guillemet;
    if (end == std::string::npos || end > 48) return line;
    std::string head = plain.substr(0, end);

    std::vector<User> all = users();
    const User* hit = nullptr;
    size_t at = std::string::npos;
    for (auto& u : all) {
        for (size_t p = head.find(u.name); p != std::string::npos; p = head.find(u.name, p + 1)) {
            size_t e = p + u.name.size();
            bool left = p == 0 || !std::isalnum((unsigned char)head[p - 1]);
            bool right = e >= head.size() || !std::isalnum((unsigned char)head[e]);
            if (left && right && (at == std::string::npos || p < at)) {
                hit = &u;
                at = p;
            }
            break;
        }
    }
    if (!hit) return line;

    size_t from = std::string::npos;
    for (size_t p = line.find(hit->name); p != std::string::npos; p = line.find(hit->name, p + 1)) {
        size_t e = p + hit->name.size();
        bool left = p == 0 || !std::isalnum((unsigned char)line[p - 1]);
        bool right = e >= line.size() || !std::isalnum((unsigned char)line[e]);
        if (left && right) {
            from = p;
            break;
        }
    }
    if (from == std::string::npos) return line;
    size_t to = from + hit->name.size();

    std::string out = line.substr(0, from);
    if (names) {
        double t = ImGui::GetTime();
        int total = 0;
        for (unsigned char c : hit->name)
            if ((c & 0xC0) != 0x80) total++;
        int index = 0;
        for (size_t i = 0; i < hit->name.size();) {
            size_t n = 1;
            unsigned char c = (unsigned char)hit->name[i];
            if (c >= 0xF0) n = 4;
            else if (c >= 0xE0) n = 3;
            else if (c >= 0xC0) n = 2;
            ImU32 col = color(hit->style, t, index++, total);
            out += std::format("§#{:02X}{:02X}{:02X};{}", int(col & 255), int((col >> 8) & 255), int((col >> 16) & 255), hit->name.substr(i, n));
            i += n;
        }
        out += "§r";
    } else {
        out += hit->name;
    }
    if (hearts && hit->style.heart) out += std::format(" §#{:06X};\x01§r", hit->style.heartColor & 0xffffff);
    if (!hit->role.empty()) out += " §#3BA7EC;" + badge(hit->role) + "§r";
    return out + line.substr(to);
}

std::string badge(const std::string& role) {
    if (role == "owner") return i18n::tr("[Owner]");
    if (role == "staff") return i18n::tr("[Staff]");
    return {};
}

}
