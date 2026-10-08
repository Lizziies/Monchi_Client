#include "core/Guard.hpp"
#include "HiveApi.hpp"

#include <windows.h>
#include <winhttp.h>
#include <json.hpp>

#include <chrono>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>

using nlohmann::json;

namespace hive {

namespace {

constexpr const char* names[] = {"BedWars", "SkyWars", "Treasure Wars", "Murder Mystery", "Hide and Seek", "Death Run", "Capture the Flag",
                                 "Ground Wars", "Just Build", "Block Drop", "Gravity", "Survival Games", "Block Party"};
constexpr const char* ids[] = {"bed", "sky", "wars", "murder", "hide", "dr", "ctf", "ground", "build", "drop", "grav", "sg", "party"};
constexpr const char* hints[] = {"bed", "sky", "treasure", "murder", "hide", "death", "capture", "ground", "build", "drop", "gravity", "survival", "party"};

struct Entry {
    Load load = Load::None;
    Stats stats;
    double at = -1e9;
    bool busy = false;
};

struct BoardEntry {
    Load load = Load::None;
    std::vector<Row> rows;
    double at = -1e9;
    bool busy = false;
};

struct Job {
    bool board = false;
    std::string key;
    std::string path;
};

std::mutex lock;
std::condition_variable wake;
std::deque<Job> jobs;
std::map<std::string, Entry> players;
std::map<std::string, BoardEntry> boards;
HANDLE worker = nullptr;
bool stopping = false;

double now() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

std::string lowered(std::string s) {
    for (auto& c : s) c = char(std::tolower((unsigned char)c));
    return s;
}

std::string encode(const std::string& s) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.') out += char(c);
        else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 15];
        }
    }
    return out;
}

std::wstring widen(const std::string& s) { return std::wstring(s.begin(), s.end()); }

int fetch(const std::string& path, std::string& body) {
    HINTERNET session = WinHttpOpen(L"Monchi", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) return 0;
    WinHttpSetTimeouts(session, 2000, 2000, 2000, 3000);
    int status = 0;
    HINTERNET conn = WinHttpConnect(session, L"api.playhive.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET req = conn ? WinHttpOpenRequest(conn, L"GET", widen(path).c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                              WINHTTP_FLAG_SECURE)
                         : nullptr;
    if (req && WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && WinHttpReceiveResponse(req, nullptr)) {
        DWORD code = 0, size = sizeof(code);
        WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &code, &size, WINHTTP_NO_HEADER_INDEX);
        status = int(code);
        DWORD avail = 0;
        while (WinHttpQueryDataAvailable(req, &avail) && avail) {
            std::string chunk(avail, '\0');
            DWORD read = 0;
            if (!WinHttpReadData(req, chunk.data(), avail, &read)) break;
            body.append(chunk.data(), read);
            if (body.size() > (4u << 20)) break;
        }
    }
    if (req) WinHttpCloseHandle(req);
    if (conn) WinHttpCloseHandle(conn);
    WinHttpCloseHandle(session);
    return status;
}

double number(const json& j, std::initializer_list<const char*> keys) {
    for (auto k : keys)
        if (j.contains(k) && j[k].is_number()) return j[k].get<double>();
    return 0.0;
}

Stats parseStats(const json& j) {
    Stats s;
    s.played = number(j, {"played", "games_played"});
    s.wins = number(j, {"victories", "wins"});
    s.kills = number(j, {"kills"});
    s.deaths = number(j, {"deaths"});
    s.finalKills = number(j, {"final_kills"});
    s.beds = number(j, {"beds_destroyed", "beds"});
    s.xp = number(j, {"xp"});
    s.prestige = number(j, {"prestige"});
    s.streak = number(j, {"win_streak", "streak"});
    s.firstPlayed = (long long)number(j, {"first_played"});
    return s;
}

std::vector<Row> parseBoard(const json& root) {
    const json* list = &root;
    if (root.is_object())
        for (auto k : {"leaderboard", "players", "data"})
            if (root.contains(k) && root[k].is_array()) list = &root[k];
    std::vector<Row> out;
    if (!list->is_array()) return out;
    for (auto& item : *list) {
        if (!item.is_object()) continue;
        Row r;
        for (auto k : {"username", "name", "player"})
            if (item.contains(k) && item[k].is_string()) {
                r.name = item[k].get<std::string>();
                break;
            }
        r.value = number(item, {"value", "victories", "xp", "kills"});
        r.rank = int(number(item, {"human_index", "rank"}));
        if (!r.rank) r.rank = int(out.size()) + 1;
        if (!r.name.empty()) out.push_back(std::move(r));
    }
    return out;
}

void run() {
    double last = 0.0;
    for (;;) {
        Job job;
        {
            std::unique_lock g(lock);
            wake.wait(g, [] { return stopping || !jobs.empty(); });
            if (stopping) return;
            job = std::move(jobs.front());
            jobs.pop_front();
        }
        double wait = 0.7 - (now() - last);
        if (wait > 0.0) Sleep(DWORD(wait * 1000));
        last = now();

        std::string body;
        int status = fetch(job.path, body);
        json parsed = status == 200 ? json::parse(body, nullptr, false) : json();
        bool ok = status == 200 && !parsed.is_discarded();

        std::scoped_lock g(lock);
        if (job.board) {
            auto& e = boards[job.key];
            e.busy = false;
            e.at = now();
            e.load = ok ? Load::Ready : Load::Failed;
            if (ok) e.rows = parseBoard(parsed);
        } else {
            auto& e = players[job.key];
            e.busy = false;
            e.at = now();
            if (ok) {
                e.load = Load::Ready;
                e.stats = parseStats(parsed);
            } else {
                e.load = status == 404 ? Load::Missing : Load::Failed;
            }
        }
    }
}

void enqueue(Job job) {
    jobs.push_back(std::move(job));
    if (!worker) worker = CreateThread(nullptr, 0, [](LPVOID) -> DWORD { guard::call("hive api", run); return 0; }, nullptr, 0, nullptr);
    wake.notify_one();
}

}

const char* gameName(int index) { return names[std::clamp(index, 0, gameCount - 1)]; }

const char* gameId(int index) { return ids[std::clamp(index, 0, gameCount - 1)]; }

int gameFromTitle(const std::string& lowerTitle) {
    for (int i = 0; i < gameCount; i++)
        if (lowerTitle.find(hints[i]) != std::string::npos) return i;
    return -1;
}

Player player(int game, const std::string& name, float maxAgeSec) {
    std::string key = std::string(gameId(game)) + "/" + lowered(name);
    std::scoped_lock g(lock);
    auto& e = players[key];
    double age = now() - e.at;
    bool retry = e.load == Load::Failed ? age > 60.0 : age > maxAgeSec;
    if (!e.busy && (e.load == Load::None || retry)) {
        e.busy = true;
        if (e.load == Load::None) e.load = Load::Loading;
        enqueue({false, key, "/v0/game/all/" + std::string(gameId(game)) + "/" + encode(name)});
    }
    return {e.load, e.stats};
}

Board board(int game, bool monthly, int rows, float maxAgeSec) {
    std::string key = std::string(gameId(game)) + (monthly ? "/m/" : "/a/") + std::to_string(rows);
    std::scoped_lock g(lock);
    auto& e = boards[key];
    double age = now() - e.at;
    bool retry = e.load == Load::Failed ? age > 60.0 : age > maxAgeSec;
    if (!e.busy && (e.load == Load::None || retry)) {
        e.busy = true;
        if (e.load == Load::None) e.load = Load::Loading;
        enqueue({true, key, std::string("/v0/game/") + (monthly ? "monthly" : "all") + "/" + gameId(game) + "?amount=" + std::to_string(rows)});
    }
    Board out;
    out.load = e.load;
    out.rows = e.rows;
    return out;
}

Stats sample(const std::string& name) {
    unsigned h = 2166136261u;
    for (unsigned char c : lowered(name)) h = (h ^ c) * 16777619u;
    auto roll = [&](double lo, double hi) {
        h = h * 1664525u + 1013904223u;
        return lo + (hi - lo) * double(h >> 8) / double(1u << 24);
    };
    Stats s;
    s.played = double(int(roll(40, 4000)));
    s.wins = double(int(s.played * roll(0.05, 0.6)));
    s.deaths = double(int(roll(30, 3000)));
    s.kills = double(int(s.deaths * roll(0.4, 3.2)));
    s.finalKills = double(int(s.deaths * roll(0.3, 4.0)));
    s.beds = double(int(s.played * roll(0.1, 0.8)));
    s.xp = double(int(roll(2000, 900000)));
    s.prestige = double(int(roll(0, 6)));
    s.streak = double(int(roll(0, 9)));
    s.firstPlayed = 1609459200LL + (long long)roll(0, 160000000);
    return s;
}

Board sampleBoard(int rows) {
    static const char* pool[] = {"Alex", "Mika", "Luna", "Steve", "Noah", "Kiki", "Max", "Rin", "Tobi", "Zoe", "Finn", "Mia"};
    Board b;
    b.load = Load::Ready;
    for (int i = 0; i < rows; i++) {
        Row r;
        r.rank = i + 1;
        r.name = std::string(pool[i % 12]) + (i >= 12 ? std::to_string(i / 12 + 1) : "");
        r.value = 90000.0 - i * 731.0;
        b.rows.push_back(std::move(r));
    }
    return b;
}

void shutdown() {
    {
        std::scoped_lock g(lock);
        stopping = true;
    }
    wake.notify_all();
    if (worker) {
        WaitForSingleObject(worker, INFINITE);
        CloseHandle(worker);
        worker = nullptr;
    }
}

}
