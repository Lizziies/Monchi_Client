#include "Game.hpp"
#include "Providers.hpp"
#include "hook/Input.hpp"
#include "render/Ui.hpp"
#include "server/Rules.hpp"
#include "sdk/Memory.hpp"
#include "sig/Sigs.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace game {

static State cur;
static std::vector<Event> frameEvents;
static std::unique_ptr<Provider> demoProvider;
static std::unique_ptr<Provider> liveProvider;
static bool demoOn = false;
static std::array<int, 12> leases{};
static int64_t seenClick = 0;
static float lastHealth = -1.f;
static bool dead = false;
static std::string demoName;
static std::vector<std::function<bool(const std::string&)>> chatFilters;

static Provider& active() { return demoOn ? *demoProvider : *liveProvider; }

static unsigned leaseMask() {
    unsigned m = 0;
    for (size_t i = 0; i < leases.size(); i++)
        if (leases[i] > 0) m |= 1u << i;
    return m;
}

void init() {
    demoProvider = makeDemo();
    liveProvider = makeLive();
}

void shutdown() {
    liveProvider.reset();
    demoProvider.reset();
}

bool demo() { return demoOn; }

void setDemoServer(const std::string& name) { demoName = name; }

const std::string& demoServer() { return demoName; }

void filterChat(std::function<bool(const std::string&)> hide) { chatFilters.push_back(std::move(hide)); }

static std::function<bool(std::string&, std::string&)> decorChat;
void setChatDecor(std::function<bool(std::string&, std::string&)> decor) { decorChat = std::move(decor); }
bool chatDecor(std::string& sender, std::string& body) { return decorChat && decorChat(sender, body); }

static std::atomic<bool> hudChatHidden{false};
void hideChatHud(bool hide) { hudChatHidden = hide; }
bool chatHudHidden() { return hudChatHidden.load(); }

static std::atomic<bool> nativeChat{false};
void drawNativeChat(bool drawn) { nativeChat = drawn; }
bool nativeChatDrawn() { return nativeChat.load(); }

bool chatHidden(const std::string& text) {
    for (auto& f : chatFilters)
        if (f(text)) return true;
    return false;
}

void setDemo(bool on) {
    if (on == demoOn) return;
    demoOn = on;
    cur = State{};
    lastHealth = -1.f;
    dead = false;
    active().use(leaseMask());
}

bool ready(unsigned mask) {
    if (demoOn || !mask) return true;
    if (!liveProvider) return false;
    return (liveProvider->supports() & mask) == mask;
}

bool has(Domain d) { return (cur.have & unsigned(d)) != 0; }

void lease(unsigned mask, int delta) {
    for (size_t i = 0; i < leases.size(); i++)
        if (mask & (1u << i)) leases[i] = std::max(0, leases[i] + delta);
    if (demoProvider && liveProvider) active().use(leaseMask());
}

const State& state() { return cur; }

const std::vector<Event>& events() { return frameEvents; }

static void push(Event e) {
    e.time = cur.time;
    frameEvents.push_back(std::move(e));
}

static void deriveFromPlayer() {
    int64_t click = input::lastClickQpc();
    if (click && click != seenClick) {
        seenClick = click;
        push({EventKind::Swing});
    }

    if (!cur.player.statsKnown) {
        lastHealth = -1.f;
        return;
    }
    float hp = cur.player.health;
    if (lastHealth >= 0.f) {
        if (hp < lastHealth - 0.01f && hp > 0.f) {
            Event e{EventKind::Hurt};
            e.value = lastHealth - hp;
            push(std::move(e));
        }
        if (hp <= 0.f && !dead) push({EventKind::Death});
        if (hp > 0.f && dead) push({EventKind::Respawn});
    }
    dead = hp <= 0.f;
    lastHealth = hp;
}

constexpr double hitGap = 0.48;

// the target keeps its damage immunity for 10 ticks, a second click inside that is no new hit
constexpr double hitWindow = 0.48;

static void absorb(const Event& e) {
    auto& c = cur.combat;
    switch (e.kind) {
    case EventKind::Hit:
        if (e.crystal) break;
        if (e.time - c.lastHitAt < hitGap && e.text == c.lastTarget) break;
        c.lastTarget = e.text;
        c.lastActor = e.actor;
        c.combo++;
        c.bestCombo = std::max(c.bestCombo, c.combo);
        c.hits++;
        if (e.crit) c.crits++;
        c.lastCrit = e.crit;
        c.damageDealt += e.value;
        c.lastReach = e.reach;
        c.bestReach = std::max(c.bestReach, e.reach);
        c.reaches[size_t(c.reachCount) % c.reaches.size()] = e.reach;
        c.reachCount++;
        c.lastHitAt = e.time;
        break;
    case EventKind::Swing: c.swings++; break;
    case EventKind::Hurt:
        c.combo = 0;
        c.taken++;
        c.damageTaken += e.value;
        c.lastHurtAt = e.time;
        break;
    case EventKind::Kill:
        c.kills++;
        c.streak++;
        c.bestStreak = std::max(c.bestStreak, c.streak);
        break;
    case EventKind::Death:
        c.deaths++;
        c.streak = 0;
        c.combo = 0;
        c.lastDeath = cur.player.pos;
        c.hasDeath = true;
        break;
    case EventKind::Chat:
        if (chatHidden(e.text)) break;
        cur.chat.push_back({e.text, e.time, e.native});
        if (cur.chat.size() > 200) cur.chat.erase(cur.chat.begin(), cur.chat.begin() + 50);
        break;
    default: break;
    }
}

void update() {
    if (!demoProvider) return;
    cur.time = ui::time();
    cur.dt = std::clamp((double)ui::dt(), 0.0005, 0.25);
    frameEvents.clear();

    Provider& p = active();
    p.update(cur, frameEvents);
    cur.have = p.supports();
    if (demoOn) cur.have |= unsigned(Domain::Camera);

    if (p.derived() && (cur.have & unsigned(Domain::Player))) deriveFromPlayer();
    for (auto& e : frameEvents) {
        if (e.time == 0.0) e.time = cur.time;
        absorb(e);
    }

    cur.server = demoOn && !demoName.empty() ? demoName : rules::status().server;
    if (!cur.camera.live) {
        cur.camera.pos = cur.player.eye();
        cur.camera.yaw = cur.player.yaw;
        cur.camera.pitch = cur.player.pitch;
        cur.camera.fov = cur.player.fov;
    }
    auto ds = ImGui::GetIO().DisplaySize;
    if (ds.y > 0.f) cur.camera.aspect = ds.x / ds.y;
}

void resetCombat() { cur.combat = Combat{}; }
uintptr_t selfActor() { return demoOn || !liveProvider ? 0 : liveProvider->actor(); }

// The totem activation overlay plays while a tick counter on the player is above zero (1.26.52: the player's totem
// handler 0x476db90 sets it to 40 next to the copied item at +0x1028, the hud renderer 0x46de660 skips the overlay
// when it is not positive, the player tick counts it down). Display state only, nothing of it reaches the server.
bool clearTotemAnimation() {
    int at = sigs::offset("player.totemAnimation", -1);
    uintptr_t actor = at > 0 ? selfActor() : 0;
    if (!actor) return false;
    if (mem::get<int>(actor + uintptr_t(at), 0) <= 0) return true;
    return mem::write<int>(actor + uintptr_t(at), 0);
}

void resetHitCounts() {
    cur.combat.hits = 0;
    cur.combat.swings = 0;
    cur.combat.crits = 0;
}

namespace {

struct Basis {
    Vec3 fwd, right, up;
    float tanHalf;
};

Basis basis() {
    const auto& c = cur.camera;
    float yaw = c.yaw * 0.0174533f, pitch = c.pitch * 0.0174533f;
    float cp = std::cos(pitch), sp = std::sin(pitch), cy = std::cos(yaw), sy = std::sin(yaw);
    Basis b;
    b.fwd = {-sy * cp, -sp, cy * cp};
    b.right = {-cy, 0.f, -sy};
    b.up = {-(b.fwd.y * b.right.z - b.fwd.z * b.right.y), -(b.fwd.z * b.right.x - b.fwd.x * b.right.z), -(b.fwd.x * b.right.y - b.fwd.y * b.right.x)};
    b.tanHalf = std::tan(c.fov * 0.0174533f * 0.5f);
    return b;
}

Vec3 toCamera(const Basis& b, const Vec3& p) {
    const auto& c = cur.camera;
    Vec3 d{p.x - c.pos.x, p.y - c.pos.y, p.z - c.pos.z};
    return {d.x * b.right.x + d.y * b.right.y + d.z * b.right.z, d.x * b.up.x + d.y * b.up.y + d.z * b.up.z,
            d.x * b.fwd.x + d.y * b.fwd.y + d.z * b.fwd.z};
}

ImVec2 toScreen(const Basis& b, const Vec3& v) {
    auto ds = ImGui::GetIO().DisplaySize;
    return {ds.x * 0.5f + v.x / (v.z * b.tanHalf * cur.camera.aspect) * ds.x * 0.5f, ds.y * 0.5f - v.y / (v.z * b.tanHalf) * ds.y * 0.5f};
}

}

std::optional<ImVec2> project(const Vec3& p) {
    Basis b = basis();
    Vec3 v = toCamera(b, p);
    if (v.z < 0.05f) return std::nullopt;
    return toScreen(b, v);
}

bool projectLine(const Vec3& a, const Vec3& b2, ImVec2& out0, ImVec2& out1) {
    Basis b = basis();
    Vec3 p = toCamera(b, a), q = toCamera(b, b2);
    constexpr float cut = 0.05f;
    if (p.z < cut && q.z < cut) return false;
    auto clip = [&](Vec3& from, const Vec3& to) {
        float t = (cut - from.z) / (to.z - from.z);
        from = {from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t, cut};
    };
    if (p.z < cut) clip(p, q);
    if (q.z < cut) clip(q, p);
    out0 = toScreen(b, p);
    out1 = toScreen(b, q);
    return true;
}

}
