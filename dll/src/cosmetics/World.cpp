#include "World.hpp"

#include "Cosmetics.hpp"
#include "core/Log.hpp"
#include "core/Paths.hpp"
#include "modules/flarial/FlarialModules.hpp"
#include "modules/online/Online.hpp"
#include "render/Ui.hpp"
#include "sdk/Game.hpp"

#include <windows.h>
#include <psapi.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <memory>

namespace cosmetics::world {

namespace {

// a player is 32 model pixels and 1.8 blocks tall
constexpr float unit = 1.8f / 32.f;
constexpr float reach = 48.f;
constexpr size_t most = 4000;

struct Wearer {
    Rig rig;
    float head = 0.f;
    float walkFrom = 0.f, walkTo = 0.f;
    double walkAt = 0.0;
    bool seen = false;
    double at = 0.0;
};

std::map<std::string, std::unique_ptr<Wearer>> wearers;
std::map<const ImTextureData*, const std::string*> paths_;
unsigned pathsFor = 0;
std::string white;
bool sent = false;
bool legsFlipped = false;

// A result the game hands back per draw once went unreleased and the game grew to 17 GB. If memory runs away while
// this draws, drawing stops for the session.
//
// Memory also grows without us (a world loading, chunks, a server's packs), so a fixed limit is not enough. When
// memory climbs fast while the cosmetics draw, drawing pauses for a few seconds. If the climb stops with it, it was
// ours and the cosmetics stay off; if it goes on, it was the game and drawing resumes.
bool runaway = false;
double memoryChecked = 0.0;
std::array<size_t, 10> memoryRing{};
int memoryFilled = 0, memoryAt = 0;
double pausedUntil = 0.0, trustedUntil = 0.0;
size_t usageAtPause = 0, growthBeforePause = 0;

size_t privateBytes() {
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof(counters);
    return GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters)) ? counters.PrivateUsage : 0;
}

// One pause proves nothing: joining a server loads a gigabyte in ten seconds and is over by the time the pause ends.
// The climb has to come back when drawing resumes and stop again when it pauses, twice, before the cosmetics are
// blamed.
int suspicion = 0;

bool memoryRunsAway() {
    if (runaway) return true;
    double now = ui::time();
    if (pausedUntil > 0.0) {
        if (now < pausedUntil) return true;
        size_t usage = privateBytes();
        size_t grown = usage > usageAtPause ? usage - usageAtPause : 0;
        pausedUntil = 0.0;
        memoryFilled = 0;
        memoryChecked = now;
        // the pause is as long as the stretch that raised the alarm, so the two numbers compare directly
        if (grown * 3 < growthBeforePause) {
            if (++suspicion >= 2) {
                runaway = true;
                logger::error("world cosmetics: twice memory grew while they were drawn ({} MB in 10 s) and stopped without them ({} MB), switched off for this session",
                              growthBeforePause >> 20, grown >> 20);
                return true;
            }
            logger::info("world cosmetics: memory grew {} MB in 10 s with them and {} MB without; drawing resumes to see whether it comes back", growthBeforePause >> 20,
                         grown >> 20);
            return false;
        }
        suspicion = 0;
        trustedUntil = now + 120.0;
        logger::info("world cosmetics: memory grew {} MB in 10 s with them and {} MB without, the growth is the game's own, drawing goes on", growthBeforePause >> 20,
                     grown >> 20);
        return false;
    }
    if (now - memoryChecked < 1.0) return false;
    // samples only count while they follow each other: after a world change or a pause the old ones say nothing
    if (now - memoryChecked > 2.5) {
        memoryFilled = 0;
        if (now - memoryChecked > 30.0) suspicion = 0;
    }
    memoryChecked = now;
    size_t usage = privateBytes();
    if (!usage) return false;
    size_t oldest = memoryRing[size_t(memoryAt)];
    memoryRing[size_t(memoryAt)] = usage;
    memoryAt = (memoryAt + 1) % int(memoryRing.size());
    if (memoryFilled < int(memoryRing.size())) {
        memoryFilled++;
        return false;
    }
    if (usage <= oldest || usage - oldest < (size_t(300) << 20)) {
        // ten calm seconds of drawing clear an earlier suspicion
        suspicion = 0;
        return false;
    }
    if (now < trustedUntil) return false;
    growthBeforePause = usage - oldest;
    usageAtPause = usage;
    pausedUntil = now + 10.0;
    logger::warn("world cosmetics: memory grew {} MB in 10 s (now {} MB), drawing pauses for 10 s to see whether it is the cause", growthBeforePause >> 20, usage >> 20);
    return true;
}

const std::string& whitePath() {
    if (!white.empty()) return white;
    static const unsigned char png[] = {137, 80, 78, 71, 13, 10, 26, 10, 0,  0,  0,   13,  73,  72,  68,  82, 0,  0,  0,   2,   0,  0,  0,  2,
                                        8,   6,  0,  0,  0,  114, 182, 13, 36, 0,  0,   0,   14,  73,  68,  65, 84, 120, 156, 99,  248, 15, 5,  12,
                                        48,  6,  0,  143, 130, 15, 241, 60, 165, 86, 81,  0,   0,   0,   0,   73, 69, 78, 68,  174, 66, 96, 130};
    auto file = paths::root() / L"cosmetics" / L"white.png";
    std::error_code ec;
    if (!std::filesystem::exists(file, ec)) std::ofstream(file, std::ios::binary).write(reinterpret_cast<const char*>(png), sizeof(png));
    white = logger::narrow(file.wstring());
    return white;
}

const std::string* pathOf(const ImTextureData* texture) {
    if (pathsFor != generation()) {
        pathsFor = generation();
        paths_.clear();
        for (auto& item : items())
            if (item.texture && !item.texturePath.empty()) paths_[item.texture] = &item.texturePath;
    }
    auto it = paths_.find(texture);
    return it == paths_.end() ? &whitePath() : it->second;
}

ImVec4 color(uint32_t rgb) { return {((rgb >> 16) & 255) / 255.f, ((rgb >> 8) & 255) / 255.f, (rgb & 255) / 255.f, 1.f}; }

// what sits on the head turns and nods with it, everything else stays with the body
void wornOf(const std::vector<online::Worn>& list, std::vector<Worn>& onBody, std::vector<Worn>& onHead) {
    for (auto& w : list) {
        const Item* item = find(w.id);
        if (!item) continue;
        Worn worn{item, {}};
        for (size_t i = 0; i < item->tints.size(); i++) worn.tints.push_back(i < w.tint.size() ? color(w.tint[i]) : item->tints[i].color);
        (item->slot == "head" || item->slot == "face" ? onHead : onBody).push_back(std::move(worn));
    }
}

float turned(float from, float to) { return std::fmod(to - from + 540.f, 360.f) - 180.f; }

struct Sorted {
    float away;
    const std::string* texture;
    flarialModules::WorldVertex v[4];
};

void place(const std::string& name, const std::vector<online::Worn>& list, const game::Pose& pose, game::Vec3 velocity, bool sprint, bool sneak, bool air,
           game::Vec3 eye, std::vector<Sorted>& out, bool slim = false) {
    if (list.empty() || !pose.known || game::distance(pose.feet, eye) > reach) return;
    std::vector<Worn> onBody, onHead;
    wornOf(list, onBody, onHead);
    if (onBody.empty() && onHead.empty()) return;

    auto& slot = wearers[name];
    if (!slot) slot = std::make_unique<Wearer>();
    Wearer& w = *slot;
    double now = ui::time();
    float dt = w.seen ? float(std::clamp(now - w.at, 0.001, 0.05)) : 0.016f;
    if (!w.seen || now - w.at > 1.0) {
        w.head = pose.head;
        w.walkFrom = w.walkTo = pose.walkPos;
        w.rig.clear();
    }
    // The walk animation advances once per tick. Between two ticks the game draws it in between, so the legs here
    // do the same from the moment the value was seen to change.
    if (pose.walkPos != w.walkTo) {
        w.walkFrom = std::fabs(pose.walkPos - w.walkTo) < 3.f ? w.walkTo : pose.walkPos;
        w.walkTo = pose.walkPos;
        w.walkAt = now;
    }
    float walk = w.walkFrom + (w.walkTo - w.walkFrom) * float(std::clamp((now - w.walkAt) / 0.05, 0.0, 1.0));
    // the game's own humanoid walk: cos(position * 38.17 degrees) * speed, legs 1.4 times that, opposite to each other
    float swing = std::cos(walk * 0.6662f) * std::clamp(pose.walkSpeed, 0.f, 1.f) * 57.3f * (legsFlipped ? -1.f : 1.f);

    float a = pose.body * 3.14159265f / 180.f;
    float fx = -std::sin(a), fz = std::cos(a), lx = std::cos(a), lz = std::sin(a);
    Moving m;
    m.fwd = velocity.x * fx + velocity.z * fz;
    m.side = velocity.x * lx + velocity.z * lz;
    m.up = velocity.y;
    m.turn = turned(w.head, pose.head) / dt;
    m.sprint = sprint;
    m.sneak = sneak;
    m.air = air;
    m.posed = true;
    m.legLeft = -swing * 1.4f;
    m.legRight = swing * 1.4f;
    m.armLeft = swing;
    m.armRight = -swing;
    w.rig.step(dt, m);
    w.head = pose.head;
    w.at = now;
    w.seen = true;

    // pose.feet comes from RenderPositionComponent; adding tick velocity extrapolates an already rendered position.
    const game::Vec3 feet = pose.feet;

    auto emit = [&](const std::vector<Worn>& worn, bool head) {
        if (worn.empty()) return;
        float yaw = (head ? pose.head : pose.body) * 3.14159265f / 180.f;
        float hx = -std::sin(yaw), hz = std::cos(yaw), sx = std::cos(yaw), sz = std::sin(yaw);
        float nod = head ? pose.pitch * 3.14159265f / 180.f : 0.f, c = std::cos(nod), s = std::sin(nod);
        for (auto& quad : mesh(worn, w.rig, slim)) {
            if (out.size() >= most) return;
            Sorted q{};
            game::Vec3 middle;
            ImU32 packed = ImGui::ColorConvertFloat4ToU32(quad.color);
            for (int i = 0; i < 4; i++) {
                float x = quad.points[i].x, y = quad.points[i].y, z = quad.points[i].z;
                if (head) {
                    // around the neck, 24 pixels up: looking down brings the top forward and the face down
                    float up = y - 24.f;
                    y = 24.f + up * c - z * s;
                    z = up * s + z * c;
                }
                x *= unit;
                y *= unit;
                z *= unit;
                q.v[i] = {feet.x + sx * x + hx * z, feet.y + y, feet.z + sz * x + hz * z, quad.uv[i].x, quad.uv[i].y, packed};
                middle.x += q.v[i].x * 0.25f;
                middle.y += q.v[i].y * 0.25f;
                middle.z += q.v[i].z * 0.25f;
            }
            q.away = game::distance(middle, eye);
            q.texture = pathOf(quad.texture);
            out.push_back(q);
        }
    };
    emit(onBody, false);
    emit(onHead, true);
}

}

void update(bool self, bool others, const std::vector<online::Worn>& mine) {
    auto& st = game::state();
    if (!st.inWorld || !flarialModules::worldMeshReady() || (!self && !others) || memoryRunsAway()) return stop();
    game::Vec3 eye = st.camera.live ? st.camera.pos : st.player.eye();
    std::vector<Sorted> quads;

    auto& pl = st.player;
    if (self && pl.view != game::View::First && !pl.name.empty())
        place(pl.name, mine, pl.pose, pl.vel, pl.sprinting, pl.sneaking, !pl.onGround, eye, quads, st.skin.slim);
    if (others)
        for (auto& o : st.others) {
            if (!o.isPlayer || o.name.empty()) continue;
            online::User user;
            if (!online::find(o.name, user) || user.worn.empty()) continue;
            float flat = std::sqrt(o.vel.x * o.vel.x + o.vel.z * o.vel.z);
            place(o.name, user.worn, o.pose, o.vel, flat > 5.f, false, std::fabs(o.vel.y) > 1.5f, eye, quads);
        }

    double now = ui::time();
    std::erase_if(wearers, [&](auto& entry) { return now - entry.second->at > 5.0; });
    if (quads.empty()) return stop();

    // The material blends and does not write depth, so the order is the picture: farthest first over everything
    // worn by everyone, cut into runs that share a texture, each quad from both sides in case the material culls.
    std::stable_sort(quads.begin(), quads.end(), [](const Sorted& a, const Sorted& b) { return a.away > b.away; });
    std::vector<flarialModules::WorldBatch> batches;
    const std::string* current = nullptr;
    for (auto& q : quads) {
        if (q.texture != current) {
            current = q.texture;
            batches.push_back({*current, {}});
        }
        auto& vertices = batches.back().vertices;
        for (int i = 0; i < 4; i++) vertices.push_back(q.v[i]);
        for (int i = 3; i >= 0; i--) vertices.push_back(q.v[i]);
    }
    flarialModules::worldMesh(batches);
    sent = true;
}

void flipLegs(bool flip) { legsFlipped = flip; }

void stop() {
    if (!sent) return;
    sent = false;
    flarialModules::worldMesh({});
}

int state() { return runaway ? 2 : flarialModules::worldMeshState(); }

}
