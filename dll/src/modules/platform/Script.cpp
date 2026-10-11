#include "Script.hpp"
#include "I18n.hpp"
#include "core/Guard.hpp"
#include "core/Log.hpp"
#include "gui/Notify.hpp"
#include "hook/Dx.hpp"
#include "modules/common/Needs.hpp"
#include "modules/common/Text.hpp"
#include "render/Fonts.hpp"
#include "render/Ui.hpp"
#include "sdk/Game.hpp"
#include "sdk/Inject.hpp"

#include <json.hpp>

extern "C" {
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
}

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <set>

namespace script {

namespace fs = std::filesystem;
using nlohmann::json;

namespace {

constexpr size_t memoryLimit = 24u << 20;
constexpr int hookEvery = 1000;
constexpr int budgetCalls = 3000;

struct Script {
    std::string name;
    fs::path path;
    fs::file_time_type stamp{};
    lua_State* L = nullptr;
    size_t used = 0;
    std::string error;
    bool enabled = true;
    bool running = false;
    int budget = 0;
    bool allowChat = false;

    struct Handler {
        std::string event;
        int ref;
    };
    struct Timer {
        double every;
        double next;
        int ref;
    };
    std::vector<Handler> handlers;
    std::vector<Timer> timers;
    std::vector<Hud> hud;
    json data = json::object();
    bool dirty = false;
};

void* allocate(void* ud, void* ptr, size_t osize, size_t nsize) {
    auto* s = static_cast<Script*>(ud);
    size_t old = ptr ? osize : 0;
    if (nsize == 0) {
        if (ptr) s->used -= osize;
        std::free(ptr);
        return nullptr;
    }
    if (s->used - old + nsize > memoryLimit) return nullptr;
    void* p = std::realloc(ptr, nsize);
    if (p) s->used = s->used - old + nsize;
    return p;
}

Script* owner(lua_State* L) { return *static_cast<Script**>(lua_getextraspace(L)); }

Script* upvalue(lua_State* L) { return static_cast<Script*>(lua_touserdata(L, lua_upvalueindex(1))); }

void hook(lua_State* L, lua_Debug*) {
    if (--owner(L)->budget < 0) luaL_error(L, "the script ran for too long");
}

void field(lua_State* L, const char* key, double v) {
    lua_pushnumber(L, v);
    lua_setfield(L, -2, key);
}

void field(lua_State* L, const char* key, bool v) {
    lua_pushboolean(L, v);
    lua_setfield(L, -2, key);
}

void field(lua_State* L, const char* key, const std::string& v) {
    lua_pushlstring(L, v.data(), v.size());
    lua_setfield(L, -2, key);
}

int apiLog(lua_State* L) {
    int n = lua_gettop(L);
    std::string out;
    for (int i = 1; i <= n; i++) {
        size_t len = 0;
        const char* s = luaL_tolstring(L, i, &len);
        out += (i > 1 ? "\t" : "") + std::string(s, len);
        lua_pop(L, 1);
    }
    logger::info("[script {}] {}", upvalue(L)->name, out);
    return 0;
}

int apiNotify(lua_State* L) {
    std::string title = luaL_checkstring(L, 1);
    std::string body = luaL_optstring(L, 2, "");
    std::string kind = luaL_optstring(L, 3, "info");
    notify::push(title, body, kind == "ok" ? notify::Kind::Ok : kind == "warn" ? notify::Kind::Warn : kind == "error" ? notify::Kind::Error : notify::Kind::Info);
    return 0;
}

int apiOn(lua_State* L) {
    auto* s = upvalue(L);
    std::string event = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_pushvalue(L, 2);
    s->handlers.push_back({event, luaL_ref(L, LUA_REGISTRYINDEX)});
    return 0;
}

int apiEvery(lua_State* L) {
    auto* s = upvalue(L);
    double every = std::max(0.05, luaL_checknumber(L, 1));
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_pushvalue(L, 2);
    s->timers.push_back({every, ui::time() + every, luaL_ref(L, LUA_REGISTRYINDEX)});
    return 0;
}

int apiSay(lua_State* L) {
    auto* s = upvalue(L);
    std::string text = luaL_checkstring(L, 1);
    if (!s->allowChat) return luaL_error(L, "chat is turned off for scripts in the Scripts settings");
    inject::say(text);
    return 0;
}

int apiTime(lua_State* L) {
    lua_pushnumber(L, ui::time());
    return 1;
}

int apiFps(lua_State* L) {
    double ms = dx::frame().frameMs;
    lua_pushnumber(L, ms > 0.0 ? 1000.0 / ms : 0.0);
    return 1;
}

int apiPlayer(lua_State* L) {
    auto& p = game::state().player;
    lua_createtable(L, 0, 20);
    if (!p.name.empty()) field(L, "name", p.name);
    field(L, "x", p.pos.x);
    field(L, "y", p.pos.y);
    field(L, "z", p.pos.z);
    field(L, "vx", p.vel.x);
    field(L, "vy", p.vel.y);
    field(L, "vz", p.vel.z);
    field(L, "yaw", p.yaw);
    field(L, "pitch", p.pitch);
    if (need::have("PlayerStats")) {
        field(L, "health", p.health);
        field(L, "maxHealth", p.maxHealth);
        field(L, "hunger", p.hunger);
        field(L, "level", double(p.level));
    }
    if (need::have("Dimension")) field(L, "dimension", double(p.dimension));
    if (need::have("Inventory")) {
        field(L, "slot", double(p.slot + 1));
        field(L, "held", p.held().empty() ? std::string() : p.held().name);
    }
    if (need::have("MoveState")) {
        field(L, "onGround", p.onGround);
        field(L, "sprinting", p.sprinting);
        field(L, "sneaking", p.sneaking);
        field(L, "swimming", p.swimming);
    }
    return 1;
}

int apiTarget(lua_State* L) {
    auto& t = game::state().target;
    if (t.kind == game::Target::Kind::None) {
        lua_pushnil(L);
        return 1;
    }
    lua_createtable(L, 0, 8);
    field(L, "kind", std::string(t.kind == game::Target::Kind::Block ? "block" : "entity"));
    field(L, "distance", t.distance);
    if (need::have("TargetInfo")) {
        field(L, "name", t.name);
        field(L, "isPlayer", t.isPlayer);
        field(L, "health", t.health);
        field(L, "breakProgress", t.breakProgress);
    }
    return 1;
}

int apiWorld(lua_State* L) {
    auto& w = game::state().world;
    lua_createtable(L, 0, 8);
    if (need::have("WorldTime")) {
        field(L, "time", double(w.time));
        field(L, "day", double(w.day));
        field(L, "raining", w.raining);
    }
    if (need::have("Biome")) field(L, "biome", w.biome);
    if (game::has(game::Domain::World)) {
        field(L, "ping", double(w.ping));
        field(L, "players", double(w.players));
        field(L, "entities", double(w.entities));
    }
    return 1;
}

int apiCombat(lua_State* L) {
    auto& c = game::state().combat;
    lua_createtable(L, 0, 12);
    field(L, "hits", double(c.hits));
    field(L, "swings", double(c.swings));
    field(L, "lastReach", c.lastReach);
    if (need::have("HurtEvents")) {
        field(L, "combo", double(c.combo));
        field(L, "bestCombo", double(c.bestCombo));
        field(L, "kills", double(c.kills));
        field(L, "deaths", double(c.deaths));
        field(L, "streak", double(c.streak));
    }
    return 1;
}

int apiServer(lua_State* L) {
    lua_pushstring(L, game::state().server.c_str());
    return 1;
}

int apiScreen(lua_State* L) {
    lua_pushstring(L, game::state().inWorld ? "game" : "menu");
    return 1;
}

ImVec4 color(lua_State* L, int index, ImVec4 fallback) {
    if (lua_isnumber(L, index)) {
        auto v = uint32_t(lua_tointeger(L, index));
        bool alpha = v > 0xFFFFFFu;
        return {float((v >> 16) & 255) / 255.f, float((v >> 8) & 255) / 255.f, float(v & 255) / 255.f, alpha ? float((v >> 24) & 255) / 255.f : 1.f};
    }
    if (!lua_istable(L, index)) return fallback;
    ImVec4 c = fallback;
    const char* names[4] = {"r", "g", "b", "a"};
    float* out[4] = {&c.x, &c.y, &c.z, &c.w};
    for (int i = 0; i < 4; i++) {
        lua_getfield(L, index, names[i]);
        if (lua_isnumber(L, -1)) *out[i] = float(lua_tonumber(L, -1));
        lua_pop(L, 1);
    }
    return c;
}

void options(lua_State* L, int index, Hud& h) {
    if (!lua_istable(L, index)) return;
    lua_getfield(L, index, "color");
    h.color = color(L, -1, h.color);
    lua_pop(L, 1);
    lua_getfield(L, index, "fill");
    if (!lua_isnil(L, -1)) {
        h.fill = color(L, -1, h.fill);
        h.background = true;
    }
    lua_pop(L, 1);
    lua_getfield(L, index, "background");
    if (lua_isboolean(L, -1)) h.background = lua_toboolean(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, index, "scale");
    if (lua_isnumber(L, -1)) h.scale = std::clamp(float(lua_tonumber(L, -1)), 0.3f, 5.f);
    lua_pop(L, 1);
    lua_getfield(L, index, "shadow");
    if (lua_isboolean(L, -1)) h.shadow = lua_toboolean(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, index, "align");
    if (lua_isstring(L, -1)) {
        std::string a = lua_tostring(L, -1);
        h.align = a == "center" ? 1 : a == "right" ? 2 : 0;
    }
    lua_pop(L, 1);
}

Hud& slot(Script& s, const std::string& id) {
    for (auto& h : s.hud)
        if (h.id == id) return h;
    s.hud.push_back({});
    s.hud.back().id = id;
    return s.hud.back();
}

int hudText(lua_State* L) {
    auto* s = upvalue(L);
    std::string id = luaL_checkstring(L, 1);
    std::string text = luaL_checkstring(L, 2);
    if (s->hud.size() > 64) return luaL_error(L, "too many HUD elements (64)");
    Hud& h = slot(*s, id);
    h.bar = false;
    h.text = text.substr(0, 400);
    h.x = float(luaL_optnumber(L, 3, 0.0));
    h.y = float(luaL_optnumber(L, 4, 0.0));
    options(L, 5, h);
    return 0;
}

int hudBar(lua_State* L) {
    auto* s = upvalue(L);
    std::string id = luaL_checkstring(L, 1);
    if (s->hud.size() > 64) return luaL_error(L, "too many HUD elements (64)");
    Hud& h = slot(*s, id);
    h.bar = true;
    h.x = float(luaL_checknumber(L, 2));
    h.y = float(luaL_checknumber(L, 3));
    h.w = std::clamp(float(luaL_optnumber(L, 4, 120.0)), 4.f, 1000.f);
    h.h = std::clamp(float(luaL_optnumber(L, 5, 8.0)), 2.f, 200.f);
    h.fraction = std::clamp(float(luaL_optnumber(L, 6, 1.0)), 0.f, 1.f);
    options(L, 7, h);
    return 0;
}

int hudRemove(lua_State* L) {
    auto* s = upvalue(L);
    std::string id = luaL_checkstring(L, 1);
    std::erase_if(s->hud, [&](const Hud& h) { return h.id == id; });
    return 0;
}

int hudClear(lua_State* L) {
    upvalue(L)->hud.clear();
    return 0;
}

int apiSave(lua_State* L) {
    auto* s = upvalue(L);
    std::string key = luaL_checkstring(L, 1);
    if (lua_isboolean(L, 2)) s->data[key] = bool(lua_toboolean(L, 2));
    else if (lua_isinteger(L, 2)) s->data[key] = lua_tointeger(L, 2);
    else if (lua_isnumber(L, 2)) s->data[key] = lua_tonumber(L, 2);
    else if (lua_isstring(L, 2)) s->data[key] = std::string(lua_tostring(L, 2));
    else s->data.erase(key);
    s->dirty = true;
    return 0;
}

int apiLoad(lua_State* L) {
    auto* s = upvalue(L);
    std::string key = luaL_checkstring(L, 1);
    auto it = s->data.find(key);
    if (it == s->data.end()) {
        lua_pushvalue(L, 2);
        return 1;
    }
    if (it->is_boolean()) lua_pushboolean(L, it->get<bool>());
    else if (it->is_number_integer()) lua_pushinteger(L, it->get<lua_Integer>());
    else if (it->is_number()) lua_pushnumber(L, it->get<double>());
    else if (it->is_string()) lua_pushstring(L, it->get<std::string>().c_str());
    else lua_pushnil(L);
    return 1;
}

void reg(lua_State* L, Script* s, const char* name, lua_CFunction fn) {
    lua_pushlightuserdata(L, s);
    lua_pushcclosure(L, fn, 1);
    lua_setfield(L, -2, name);
}

void strip(lua_State* L, const char* lib, std::initializer_list<const char*> names) {
    lua_getglobal(L, lib);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return;
    }
    for (auto n : names) {
        lua_pushnil(L);
        lua_setfield(L, -2, n);
    }
    lua_pop(L, 1);
}

void openLibs(lua_State* L) {
    struct Lib {
        const char* name;
        lua_CFunction open;
    };
    static const Lib libs[] = {{"_G", luaopen_base}, {"coroutine", luaopen_coroutine}, {"table", luaopen_table}, {"string", luaopen_string},
                               {"math", luaopen_math}, {"utf8", luaopen_utf8},          {"os", luaopen_os}};
    for (auto& l : libs) {
        luaL_requiref(L, l.name, l.open, 1);
        lua_pop(L, 1);
    }
    for (auto n : {"dofile", "loadfile", "load", "loadstring", "require", "collectgarbage", "print"}) {
        lua_pushnil(L);
        lua_setglobal(L, n);
    }
    strip(L, "os", {"execute", "remove", "rename", "tmpname", "getenv", "exit", "setlocale"});
    strip(L, "string", {"dump"});
}

void bind(lua_State* L, Script* s) {
    lua_createtable(L, 0, 32);
    reg(L, s, "log", apiLog);
    reg(L, s, "notify", apiNotify);
    reg(L, s, "on", apiOn);
    reg(L, s, "every", apiEvery);
    reg(L, s, "say", apiSay);
    reg(L, s, "time", apiTime);
    reg(L, s, "fps", apiFps);
    reg(L, s, "player", apiPlayer);
    reg(L, s, "target", apiTarget);
    reg(L, s, "world", apiWorld);
    reg(L, s, "combat", apiCombat);
    reg(L, s, "server", apiServer);
    reg(L, s, "screen", apiScreen);
    reg(L, s, "save", apiSave);
    reg(L, s, "load", apiLoad);
    lua_createtable(L, 0, 4);
    reg(L, s, "text", hudText);
    reg(L, s, "bar", hudBar);
    reg(L, s, "remove", hudRemove);
    reg(L, s, "clear", hudClear);
    lua_setfield(L, -2, "hud");
    lua_pushstring(L, "1");
    lua_setfield(L, -2, "api");
    // scripts written before the rename still say "mochi"
    lua_pushvalue(L, -1);
    lua_setglobal(L, "mochi");
    lua_setglobal(L, "monchi");
    lua_pushlightuserdata(L, s);
    lua_pushcclosure(L, apiLog, 1);
    lua_setglobal(L, "print");
}

}

struct Engine::Impl {
    fs::path folder;
    std::vector<std::unique_ptr<Script>> scripts;
    std::set<std::string> disabled;
    bool allowChat = false;
    double lastScan = -10.0;

    void close(Script& s) {
        if (s.L) lua_close(s.L);
        s.L = nullptr;
        s.handlers.clear();
        s.timers.clear();
        s.hud.clear();
        s.running = false;
        s.used = 0;
    }

    void flush(Script& s) {
        if (!s.dirty) return;
        s.dirty = false;
        std::ofstream(folder / (s.name + ".data.json"), std::ios::trunc) << s.data.dump(2);
    }

    bool call(Script& s, int nargs) {
        s.budget = budgetCalls;
        if (lua_pcall(s.L, nargs, 0, 0) == LUA_OK) return true;
        s.error = lua_tostring(s.L, -1) ? lua_tostring(s.L, -1) : "unknown error";
        lua_pop(s.L, 1);
        logger::error("script {}: {}", s.name, s.error);
        notify::push(i18n::fmt("Script {} stopped", s.name), s.error.substr(0, 140), notify::Kind::Error, 6.f);
        s.running = false;
        return false;
    }

    void load(Script& s) {
        close(s);
        s.error.clear();
        s.data = json::object();
        if (std::ifstream in(folder / (s.name + ".data.json")); in) {
            auto j = json::parse(in, nullptr, false);
            if (j.is_object()) s.data = j;
        }
        s.allowChat = allowChat;
        s.L = lua_newstate(allocate, &s);
        if (!s.L) {
            s.error = "not enough memory";
            return;
        }
        *static_cast<Script**>(lua_getextraspace(s.L)) = &s;
        lua_sethook(s.L, hook, LUA_MASKCOUNT, hookEvery);
        openLibs(s.L);
        bind(s.L, &s);

        std::ifstream in(s.path, std::ios::binary);
        std::string code((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        std::string chunk = "@" + s.path.filename().string();
        if (luaL_loadbufferx(s.L, code.data(), code.size(), chunk.c_str(), "t") != LUA_OK) {
            s.error = lua_tostring(s.L, -1) ? lua_tostring(s.L, -1) : "syntax error";
            lua_pop(s.L, 1);
            logger::error("script {}: {}", s.name, s.error);
            return;
        }
        s.running = true;
        guard::call(s.name.c_str(), [&] { call(s, 0); });
    }

    template <class Push>
    void fire(const char* event, Push&& push) {
        for (auto& sp : scripts) {
            Script& s = *sp;
            if (!s.enabled || !s.running) continue;
            for (size_t i = 0; i < s.handlers.size() && s.running; i++) {
                if (s.handlers[i].event != event) continue;
                lua_rawgeti(s.L, LUA_REGISTRYINDEX, s.handlers[i].ref);
                int n = push(s.L);
                guard::call(s.name.c_str(), [&] { call(s, n); });
            }
        }
    }
};

Engine::Engine() : impl_(std::make_unique<Impl>()) {}

Engine::~Engine() { shutdown(); }

void Engine::setFolder(fs::path folder) { impl_->folder = std::move(folder); }

void Engine::setAllowChat(bool allow) {
    impl_->allowChat = allow;
    for (auto& s : impl_->scripts) s->allowChat = allow;
}

void Engine::setDisabled(const std::vector<std::string>& names) {
    impl_->disabled = std::set<std::string>(names.begin(), names.end());
    for (auto& s : impl_->scripts) s->enabled = !impl_->disabled.count(s->name);
}

void Engine::scan(bool force) {
    double now = ui::time();
    if (!force && now - impl_->lastScan < 1.0) return;
    impl_->lastScan = now;
    std::error_code ec;
    std::set<std::string> seen;
    for (auto& e : fs::directory_iterator(impl_->folder, ec)) {
        if (e.path().extension() != ".lua") continue;
        std::string name = e.path().stem().string();
        seen.insert(name);
        auto stamp = fs::last_write_time(e.path(), ec);
        Script* found = nullptr;
        for (auto& s : impl_->scripts)
            if (s->name == name) found = s.get();
        if (!found) {
            impl_->scripts.push_back(std::make_unique<Script>());
            found = impl_->scripts.back().get();
            found->name = name;
            found->path = e.path();
            found->enabled = !impl_->disabled.count(name);
            found->stamp = {};
        }
        if (found->stamp != stamp) {
            found->stamp = stamp;
            if (found->enabled) guard::call(name.c_str(), [&] { impl_->load(*found); });
        }
    }
    for (size_t i = 0; i < impl_->scripts.size();) {
        if (seen.count(impl_->scripts[i]->name)) {
            i++;
            continue;
        }
        impl_->close(*impl_->scripts[i]);
        impl_->scripts.erase(impl_->scripts.begin() + long(i));
    }
}

void Engine::tick(float dt) {
    double now = ui::time();
    impl_->fire("tick", [&](lua_State* L) {
        lua_pushnumber(L, dt);
        return 1;
    });
    for (auto& sp : impl_->scripts) {
        Script& s = *sp;
        if (!s.enabled || !s.running) continue;
        for (size_t i = 0; i < s.timers.size() && s.running; i++) {
            if (now < s.timers[i].next) continue;
            s.timers[i].next = now + s.timers[i].every;
            lua_rawgeti(s.L, LUA_REGISTRYINDEX, s.timers[i].ref);
            guard::call(s.name.c_str(), [&] { impl_->call(s, 0); });
        }
        impl_->flush(s);
    }
}

void Engine::keys(int vk, bool down) {
    impl_->fire("key", [&](lua_State* L) {
        lua_createtable(L, 0, 2);
        field(L, "vk", double(vk));
        field(L, "down", down);
        return 1;
    });
}

void Engine::serverChanged(const std::string& name, const std::string& host, bool joined) {
    impl_->fire("server", [&](lua_State* L) {
        lua_createtable(L, 0, 3);
        field(L, "name", name);
        field(L, "host", host);
        field(L, "joined", joined);
        return 1;
    });
}

void Engine::events() {
    for (auto& e : game::events()) {
        switch (e.kind) {
        case game::EventKind::Hit:
            impl_->fire("hit", [&](lua_State* L) {
                lua_createtable(L, 0, 5);
                field(L, "reach", e.reach);
                field(L, "crit", e.crit);
                field(L, "crystal", e.crystal);
                field(L, "damage", e.value);
                field(L, "target", e.text);
                return 1;
            });
            break;
        case game::EventKind::Hurt:
            impl_->fire("hurt", [&](lua_State* L) {
                lua_createtable(L, 0, 1);
                field(L, "damage", e.value);
                return 1;
            });
            break;
        case game::EventKind::Kill:
            impl_->fire("kill", [&](lua_State* L) {
                lua_createtable(L, 0, 1);
                field(L, "name", e.text);
                return 1;
            });
            break;
        case game::EventKind::Death: impl_->fire("death", [](lua_State*) { return 0; }); break;
        case game::EventKind::Respawn: impl_->fire("respawn", [](lua_State*) { return 0; }); break;
        case game::EventKind::Chat:
            impl_->fire("chat", [&](lua_State* L) {
                std::string plain = text::strip(e.text);
                lua_pushlstring(L, plain.data(), plain.size());
                return 1;
            });
            break;
        case game::EventKind::Sound:
            impl_->fire("sound", [&](lua_State* L) {
                lua_createtable(L, 0, 2);
                field(L, "id", e.text);
                field(L, "distance", e.value);
                return 1;
            });
            break;
        default: break;
        }
    }
}

void Engine::draw(ImDrawList* dl) {
    auto ds = ImGui::GetIO().DisplaySize;
    float k = ui::scale();
    ImFont* f = fonts::hud();
    for (auto& sp : impl_->scripts) {
        if (!sp->enabled || !sp->running) continue;
        for (auto& h : sp->hud) {
            ImVec2 at{h.x * ds.x, h.y * ds.y};
            if (h.bar) {
                ImVec2 size{h.w * k, h.h * k};
                dl->AddRectFilled(at, at + size, ImGui::GetColorU32(h.fill), size.y * 0.4f);
                dl->AddRectFilled(at, at + ImVec2(size.x * h.fraction, size.y), ImGui::GetColorU32(h.color), size.y * 0.4f);
                continue;
            }
            float size = fonts::hudSize() * h.scale * k;
            ImVec2 ts = f->CalcTextSizeA(size, FLT_MAX, 0.f, h.text.c_str());
            ImVec2 p = at - ImVec2(ts.x * float(h.align) * 0.5f, 0.f);
            if (h.background) dl->AddRectFilled(p - ImVec2(5 * k, 3 * k), p + ts + ImVec2(5 * k, 3 * k), ImGui::GetColorU32(h.fill), 6 * k);
            if (h.shadow) dl->AddText(f, size, p + ImVec2(1.f * k, 1.f * k), IM_COL32(0, 0, 0, int(160 * h.color.w)), h.text.c_str());
            dl->AddText(f, size, p, ImGui::GetColorU32(h.color), h.text.c_str());
        }
    }
}

std::vector<Info> Engine::list() const {
    std::vector<Info> out;
    for (auto& s : impl_->scripts) {
        Info i;
        i.name = s->name;
        i.error = s->error;
        i.enabled = s->enabled;
        i.running = s->running;
        i.hud = int(s->hud.size());
        i.handlers = int(s->handlers.size() + s->timers.size());
        i.memory = s->used;
        out.push_back(std::move(i));
    }
    return out;
}

void Engine::enable(const std::string& name, bool on) {
    for (auto& s : impl_->scripts) {
        if (s->name != name) continue;
        s->enabled = on;
        if (on) {
            guard::call(name.c_str(), [&] { impl_->load(*s); });
            impl_->disabled.erase(name);
        } else {
            impl_->close(*s);
            impl_->disabled.insert(name);
        }
    }
}

void Engine::reload(const std::string& name) {
    for (auto& s : impl_->scripts)
        if (s->name == name && s->enabled) guard::call(name.c_str(), [&] { impl_->load(*s); });
}

void Engine::shutdown() {
    if (!impl_) return;
    for (auto& s : impl_->scripts) {
        impl_->flush(*s);
        impl_->close(*s);
    }
    impl_->scripts.clear();
}

}
