#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace game {

struct Vec3 {
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
};

inline float distance(const Vec3& a, const Vec3& b) {
    float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

enum class Domain : unsigned {
    Player = 1,
    Inventory = 2,
    Effects = 4,
    Target = 8,
    World = 16,
    Combat = 32,
    Chat = 64,
    Scoreboard = 128,
    Tab = 256,
    Camera = 512,
    Others = 1024,
    Light = 2048,
};

constexpr unsigned operator|(Domain a, Domain b) { return unsigned(a) | unsigned(b); }
constexpr unsigned operator|(unsigned a, Domain b) { return a | unsigned(b); }

struct Item {
    std::string name;
    int aux = 0;
    int count = 0;
    int damage = 0;
    int maxDamage = 0;
    bool enchanted = false;

    bool empty() const { return count <= 0 || name.empty(); }
    int left() const { return maxDamage > 0 ? maxDamage - damage : 0; }
    float fraction() const { return maxDamage > 0 ? float(maxDamage - damage) / float(maxDamage) : 1.f; }
};

struct Effect {
    std::string id;
    int amplifier = 0;
    float seconds = 0.f;
    float total = 0.f;
    bool good = true;
    uint32_t color = 0xFFFFFF;
    bool infinite = false;
};

// How the game poses a player in the frame being drawn: where the feet stand (interpolated), body and head in
// degrees, and the walk animation the legs swing with.
struct Pose {
    bool known = false;
    Vec3 feet;
    float body = 0.f;
    float head = 0.f;
    float pitch = 0.f;
    float walkPos = 0.f;
    float walkSpeed = 0.f;
};

enum class Screen { None, Inventory, Chat, Pause, Other };
enum class View { First, Back, Front };
enum class Mode { Survival, Creative, Adventure, Spectator };

struct Player {
    std::string name;
    int team = 0;
    Vec3 pos;
    Vec3 vel;
    Pose pose;
    float eyeHeight = 1.62f;
    float yaw = 0.f;
    float pitch = 0.f;
    float health = 20.f;
    float maxHealth = 20.f;
    float absorption = 0.f;
    float hunger = 20.f;
    float saturation = 5.f;
    int air = 300;
    int maxAir = 300;
    float xp = 0.f;
    int level = 0;
    int dimension = 0;
    int slot = 0;
    float fov = 70.f;
    Mode mode = Mode::Survival;
    View view = View::First;
    bool onGround = true;
    bool sprinting = false;
    bool sneaking = false;
    bool swimming = false;
    bool gliding = false;
    bool emoting = false;
    bool flying = false;
    bool inWater = false;
    bool onFire = false;
    bool usingItem = false;
    bool blocking = false;
    float useProgress = 0.f;
    // false while health and hunger are still defaults (live data not found yet)
    bool statsKnown = true;
    bool hasBox = false;
    Vec3 boxMin;
    Vec3 boxMax;
    std::array<Item, 9> hotbar;
    std::array<Item, 4> armor;
    Item offhand;
    std::vector<Item> main;
    std::vector<Effect> effects;

    Vec3 eye() const { return {pos.x, pos.y + eyeHeight, pos.z}; }
    const Item& held() const {
        static const Item none;
        return handEmpty ? none : hotbar[size_t(slot) % hotbar.size()];
    }
    bool handEmpty = false;
};

struct Target {
    enum class Kind { None, Block, Entity };
    Kind kind = Kind::None;
    std::string name;
    Vec3 pos;
    int blockX = 0;
    int blockY = 0;
    int blockZ = 0;
    float distance = 0.f;
    bool isPlayer = false;
    int team = 0;
    int armor = 0;
    float health = 0.f;
    float maxHealth = 20.f;
    float breakProgress = 0.f;
    float fuse = 0.f;
    bool hasBox = false;
    Vec3 boxMin;
    Vec3 boxMax;
    float lookYaw = 0.f;
    float lookPitch = 0.f;
    int skinSize = 0;
    std::vector<uint32_t> skin;
};

struct World {
    int time = 1000;
    int day = 1;
    bool raining = false;
    bool thundering = false;
    std::string biome = "plains";
    int entities = 0;
    int players = 0;
    int ping = 0;
    float tps = 0.f;
    std::string name;
    std::string id;
    std::vector<std::string> packs;
};

struct ChatLine {
    std::string text;
    double time = 0.0;
    // has characters only the game can draw (a server's own glyphs): the game's chat keeps showing it
    bool native = false;
};

struct Scoreboard {
    std::string title;
    std::vector<std::pair<std::string, int>> lines;
};

enum class Platform { Unknown, Desktop, Mobile, Console };

struct TabEntry {
    std::string name;
    int ping = 0;
    Mode mode = Mode::Survival;
    Platform platform = Platform::Unknown;
    bool hasHead = false;
    std::array<uint32_t, 64> head{};
};

struct Combat {
    int combo = 0;
    int bestCombo = 0;
    int hits = 0;
    int swings = 0;
    int taken = 0;
    int kills = 0;
    int deaths = 0;
    int streak = 0;
    int bestStreak = 0;
    int crits = 0;
    float damageDealt = 0.f;
    float damageTaken = 0.f;
    float lastReach = 0.f;
    float bestReach = 0.f;
    std::string lastTarget;
    uintptr_t lastActor = 0;
    double lastHitAt = -100.0;
    double lastHurtAt = -100.0;
    bool lastCrit = false;
    Vec3 lastDeath;
    bool hasDeath = false;
    std::array<float, 10> reaches{};
    int reachCount = 0;
};

struct Other {
    uintptr_t id = 0;
    std::string name;
    std::string kind;
    bool isPlayer = false;
    Vec3 pos;
    // blocks per second, and where the head looks
    Vec3 vel;
    float yaw = 0.f;
    Pose pose;
    float health = 20.f;
    float maxHealth = 20.f;
    int team = 0;
};

struct Projectile {
    uintptr_t id = 0;
    int kind = 0;
    bool mine = false;
    Vec3 pos;
    Vec3 vel;
};

struct LightGrid {
    int radius = 0;
    int baseX = 0;
    int baseY = 0;
    int baseZ = 0;
    std::vector<uint8_t> level;

    int size() const { return radius * 2 + 1; }
    bool valid() const { return radius > 0 && level.size() == size_t(size() * size()); }
    int at(int dx, int dz) const { return level[size_t((dz + radius) * size() + dx + radius)]; }
};

struct Camera {
    Vec3 pos;
    float yaw = 0.f;
    float pitch = 0.f;
    float fov = 70.f;
    float aspect = 16.f / 9.f;
    bool live = false;
};

enum class EventKind { Hit, Hurt, Kill, Death, TotemPop, Swing, BowRelease, ItemUse, Chat, Respawn, Confirm, Sound };

struct Event {
    EventKind kind = EventKind::Hit;
    double time = 0.0;
    float value = 0.f;
    float reach = 0.f;
    float damage = 0.f;
    bool crit = false;
    bool crystal = false;
    uintptr_t actor = 0;
    bool hasPos = false;
    Vec3 pos;
    std::string text;
    std::string item;
    bool native = false;
};

struct Skin {
    int width = 0, height = 0;
    bool slim = false;
    std::vector<unsigned char> rgba;
};

struct State {
    Skin skin;
    Player player;
    Target target;
    World world;
    Combat combat;
    Camera camera;
    Screen screen = Screen::None;
    std::vector<ChatLine> chat;
    Scoreboard scoreboard;
    std::vector<TabEntry> tab;
    std::vector<Other> others;
    std::vector<Projectile> shots;
    LightGrid light;
    std::string server;
    unsigned have = 0;
    bool inWorld = false;
    double time = 0.0;
    double dt = 0.016;
};

}
