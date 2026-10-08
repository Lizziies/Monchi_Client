#include "Providers.hpp"
#include "hook/Input.hpp"

#include <algorithm>
#include <cmath>
#include <random>

namespace game {

namespace {

constexpr unsigned all = unsigned(Domain::Player) | unsigned(Domain::Inventory) | unsigned(Domain::Effects) | unsigned(Domain::Target) |
                         unsigned(Domain::World) | unsigned(Domain::Combat) | unsigned(Domain::Chat) | unsigned(Domain::Scoreboard) |
                         unsigned(Domain::Tab) | unsigned(Domain::Camera) | unsigned(Domain::Others) | unsigned(Domain::Light);

class Demo : public Provider {
public:
    unsigned supports() const override { return all; }
    bool derived() const override { return false; }

    void update(State& s, std::vector<Event>& ev) override {
        double dt = s.dt;
        s.inWorld = true;
        t_ += dt;
        if (!init_) setup(s);

        movement(s, dt);
        fight(s, ev, dt);
        survival(s, dt);
        world(s, dt);
        target(s);
        chatter(s, ev);
        uses(ev);
        sounds(s, ev);
        nearby(s);
        shots(s);
        light(s);
        confirms(ev);
        effects(s, dt);
        scoreboard(s);
    }

private:
    static Item item(const char* name, int count, int maxDamage = 0, int aux = 0, bool glint = false) {
        Item i;
        i.name = name;
        i.count = count;
        i.maxDamage = maxDamage;
        i.aux = aux;
        i.enchanted = glint;
        return i;
    }

    void setup(State& s) {
        init_ = true;
        auto& p = s.player;
        p.name = "MonchiPlayer";
        p.pos = {128.5f, 64.f, -42.5f};
        p.hotbar[0] = item("diamond_sword", 1, 1561, 0, true);
        p.hotbar[1] = item("bow", 1, 384, 0, true);
        p.hotbar[2] = item("golden_apple", 8);
        p.hotbar[3] = item("ender_pearl", 12);
        p.hotbar[4] = item("splash_potion", 24, 0, 21);
        p.hotbar[5] = item("diamond_pickaxe", 1, 1561);
        p.hotbar[6] = item("oak_planks", 64);
        p.hotbar[7] = item("cooked_beef", 32);
        p.hotbar[8] = item("water_bucket", 1);
        p.armor[0] = item("diamond_helmet", 1, 363, 0, true);
        p.armor[1] = item("diamond_chestplate", 1, 528, 0, true);
        p.armor[2] = item("diamond_leggings", 1, 495, 0, true);
        p.armor[3] = item("diamond_boots", 1, 429, 0, true);
        p.offhand = item("totem_of_undying", 2);
        p.main.assign(27, Item{});
        p.main[0] = item("arrow", 48);
        p.main[1] = item("totem_of_undying", 3);
        p.main[2] = item("splash_potion", 16, 0, 21);
        p.main[3] = item("iron_ingot", 34);
        p.main[4] = item("diamond", 9);
        p.main[5] = item("golden_apple", 5);
        p.main[6] = item("ender_pearl", 4);
        p.level = 17;
        p.fov = 70.f;
        s.world.packs = {"Monchi Pack", "Vanilla"};
        s.world.players = 11;
        s.world.entities = 41;
        s.world.biome = "plains";
        s.world.name = "Monchi Lobby";
        s.world.id.clear();
        s.world.time = 1000;
        s.world.day = 12;
        s.player.maxHealth = 20.f;
        s.scoreboard.title = "Monchi Wars";
        for (int i = 0; i < 12; i++) {
            static const char* names[] = {"Teammate", "Opponent", "Bystander", "Luna", "Max", "Kiki", "Noah", "Mia", "Finn", "Lea", "Tim", "Emma"};
            TabEntry e;
            e.name = names[i];
            e.ping = 20 + (i * 17) % 90;
            e.platform = Platform(1 + i % 3);
            e.hasHead = true;
            face(e);
            s.tab.push_back(e);
        }
        s.tab.push_back({"MonchiPlayer", 32});
        opponentHp_ = 20.f;
    }

    static void face(TabEntry& e) {
        unsigned h = 2166136261u;
        for (unsigned char c : e.name) h = (h ^ c) * 16777619u;
        auto channel = [&](int shift, unsigned lo, unsigned span) { return lo + ((h >> shift) % span); };
        uint32_t hair = 0xFF000000u | (channel(0, 30, 120) << 16) | (channel(8, 20, 90) << 8) | channel(16, 10, 80);
        uint32_t skin = 0xFF000000u | (channel(4, 150, 80) << 16) | (channel(12, 100, 70) << 8) | channel(20, 70, 60);
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++) {
                uint32_t c = y < 2 ? hair : skin;
                if (y == 4 && (x == 2 || x == 5)) c = 0xFFFFFFFFu;
                if (y == 4 && (x == 1 || x == 6)) c = 0xFF402010u;
                if (y == 6 && x >= 3 && x <= 4) c = 0xFF5A3030u;
                e.head[size_t(y * 8 + x)] = c;
            }
    }

    void movement(State& s, double dt) {
        auto& p = s.player;
        float k = float(t_);
        sprint_ = std::fmod(k, 24.f) < 14.f;
        sneak_ = std::fmod(k, 40.f) > 36.f;
        float speed = sneak_ ? 1.3f : sprint_ ? 5.6f : 4.3f;
        float heading = k * 0.18f;
        Vec3 v{std::cos(heading) * speed, 0.f, std::sin(heading) * speed};
        bool jump = std::fmod(k, 3.f) < 0.45f;
        v.y = jump ? (std::fmod(k, 3.f) < 0.22f ? 6.f : -6.f) : 0.f;
        p.vel = v;
        p.pos.x += v.x * float(dt);
        p.pos.z += v.z * float(dt);
        p.pos.y = 64.f + (jump ? 1.1f * std::sin(std::fmod(k, 3.f) / 0.45f * 3.1416f) : 0.f);
        p.onGround = !jump;
        p.sprinting = sprint_;
        p.sneaking = sneak_;
        p.eyeHeight = sneak_ ? 1.27f : 1.62f;
        p.yaw = std::fmod(heading * 57.2958f + 90.f, 360.f);
        if (p.yaw > 180.f) p.yaw -= 360.f;
        p.pitch = 14.f * std::sin(k * 0.6f);
        p.slot = int(std::fmod(k / 6.f, 9.f));
        p.dimension = 0;
        p.view = View::First;
    }

    void fight(State& s, std::vector<Event>& ev, double dt) {
        auto& p = s.player;
        double phase = std::fmod(t_, 16.0);
        fighting_ = phase > 4.0 && phase < 12.0;
        p.usingItem = false;
        p.useProgress = 0.f;
        if (phase > 13.0 && phase < 14.2) {
            p.usingItem = true;
            p.useProgress = float((phase - 13.0) / 1.0);
            if (p.useProgress > 1.f) p.useProgress = 1.f;
            drawing_ = true;
        } else if (drawing_) {
            drawing_ = false;
            Event e{EventKind::BowRelease};
            e.value = 1.f;
            ev.push_back(e);
        }

        if (respawnAt_ > 0.0) {
            if (t_ >= respawnAt_) {
                respawnAt_ = 0.0;
                p.health = 20.f;
                ev.push_back({EventKind::Respawn});
            }
            return;
        }
        if (!fighting_) {
            p.blocking = false;
            return;
        }

        swingTimer_ -= dt;
        if (swingTimer_ <= 0.0) {
            swingTimer_ = 0.09 + dist_(rng_) * 0.1;
            ev.push_back({EventKind::Swing});
            if (dist_(rng_) < 0.72) {
                Event e{EventKind::Hit};
                e.reach = std::max(0.5f, oppDist_ - 0.3f + (float(dist_(rng_)) - 0.5f) * 0.2f);
                e.crit = dist_(rng_) < 0.2;
                e.value = e.crit ? 9.f : 6.f;
                e.text = "Opponent";
                e.actor = 0x7000;
                ev.push_back(e);
                float ms = float(s.world.ping) + float(dist_(rng_)) * 12.f;
                acks_.push_back({t_ + ms / 1000.0, ms, e.reach});
                opponentHp_ -= e.value * 0.5f;
                auto& sword = p.hotbar[0];
                sword.damage = std::min(sword.maxDamage - 1, sword.damage + 1);
                if (opponentHp_ <= 0.f) {
                    Event k{EventKind::Kill};
                    k.text = "Opponent";
                    k.actor = 0x7000;
                    ev.push_back(k);
                    opponentHp_ = 20.f;
                }
            }
        }

        crystalTimer_ -= dt;
        if (crystalTimer_ <= 0.0) {
            crystalTimer_ = 2.5 + dist_(rng_) * 3.0;
            Event c{EventKind::Hit};
            c.crystal = true;
            c.reach = 3.1f + float(dist_(rng_)) * 0.8f;
            c.text = "End Crystal";
            c.actor = 0x5000 + uintptr_t(crystalIdx_++ % 6) * 0x40;
            ev.push_back(c);
        }

        hurtTimer_ -= dt;
        if (hurtTimer_ <= 0.0) {
            hurtTimer_ = 0.5 + dist_(rng_) * 0.5;
            if (dist_(rng_) < 0.45) {
                Event e{EventKind::Hurt};
                e.value = 1.5f + dist_(rng_) * 3.f;
                e.reach = std::max(0.5f, oppDist_ - 0.3f);
                e.text = "Opponent";
                ev.push_back(e);
                p.health -= e.value;
                for (auto& a : p.armor) a.damage = std::min(a.maxDamage - 1, a.damage + (dist_(rng_) < 0.3 ? 1 : 0));
                if (p.health <= 0.f) {
                    p.health = 0.f;
                    ev.push_back({EventKind::Death});
                    respawnAt_ = t_ + 2.5;
                } else if (p.health < 5.f && p.offhand.count > 0 && dist_(rng_) < 0.4) {
                    p.health = 8.f;
                    Event tp{EventKind::TotemPop};
                    ev.push_back(tp);
                    p.offhand.count--;
                    if (p.offhand.count == 0) p.offhand.count = 2;
                }
            }
        }
        p.blocking = std::fmod(t_, 3.0) < 0.5;
    }

    void sounds(State& s, std::vector<Event>& ev) {
        if (t_ < nextSound_) return;
        nextSound_ = t_ + 0.5 + dist_(rng_) * 1.4;
        static const char* ids[] = {"step.stone", "random.hurt", "random.door_open", "random.explode", "random.bow", "random.chestopen", "random.levelup", "mob.zombie.say"};
        Event e{EventKind::Sound};
        e.text = ids[soundIdx_++ % 8];
        double a = dist_(rng_) * 6.2832;
        float d = 3.f + float(dist_(rng_)) * 12.f;
        e.hasPos = true;
        e.pos = {s.player.pos.x + std::cos(float(a)) * d, s.player.pos.y, s.player.pos.z + std::sin(float(a)) * d};
        e.value = d;
        ev.push_back(std::move(e));
    }

    struct Flight {
        uintptr_t id;
        int kind;
        bool mine;
        Vec3 from;
        Vec3 dir;
        double born;
        double life;
        float speed;
    };

    void shots(State& s) {
        if (t_ >= nextShot_) {
            nextShot_ = t_ + 3.5;
            auto& p = s.player;
            float yaw = p.yaw * 0.0174533f, pitch = (p.pitch - 12.f) * 0.0174533f;
            Vec3 dir{-std::sin(yaw) * std::cos(pitch), -std::sin(pitch), std::cos(yaw) * std::cos(pitch)};
            bool pearl = flights_.size() % 3 == 2;
            flights_.push_back({uintptr_t(0x9000 + shotId_++), pearl ? 1 : 0, !pearl, p.eye(), dir, t_, pearl ? 1.4 : 1.1, pearl ? 22.f : 30.f});
        }
        s.shots.clear();
        for (size_t i = 0; i < flights_.size();) {
            auto& f = flights_[i];
            double age = t_ - f.born;
            if (age > f.life) {
                flights_.erase(flights_.begin() + long(i));
                continue;
            }
            float a = float(age);
            Projectile pr;
            pr.id = f.id;
            pr.kind = f.kind;
            pr.mine = f.mine;
            pr.pos = {f.from.x + f.dir.x * f.speed * a, f.from.y + f.dir.y * f.speed * a - 4.9f * a * a, f.from.z + f.dir.z * f.speed * a};
            pr.vel = {f.dir.x * f.speed, f.dir.y * f.speed - 9.8f * a, f.dir.z * f.speed};
            s.shots.push_back(pr);
            i++;
        }
    }

    void light(State& s) {
        auto& g = s.light;
        const int r = 8;
        if (g.radius != r) {
            g.radius = r;
            g.level.assign(size_t((2 * r + 1) * (2 * r + 1)), 0);
        }
        g.baseX = int(std::floor(s.player.pos.x));
        g.baseY = int(std::floor(s.player.pos.y)) - 1;
        g.baseZ = int(std::floor(s.player.pos.z));
        static const int torches[][2] = {{-5, -4}, {3, 5}, {6, -2}, {-2, 2}};
        for (int dz = -r; dz <= r; dz++)
            for (int dx = -r; dx <= r; dx++) {
                int wx = g.baseX + dx, wz = g.baseZ + dz;
                int best = ((wx * 7 + wz * 13) % 5 == 0) ? 3 : 0;
                for (auto& t : torches) {
                    int d = std::abs(dx - t[0]) + std::abs(dz - t[1]);
                    best = std::max(best, 14 - d);
                }
                g.level[size_t((dz + r) * (2 * r + 1) + dx + r)] = uint8_t(std::clamp(best, 0, 15));
            }
    }

    void nearby(State& s) {
        auto& p = s.player;
        p.team = 1;
        oppDist_ = 2.9f + 0.45f * std::sin(float(t_) * 1.7f);
        float yaw = p.yaw * 0.0174533f;
        Vec3 fwd{-std::sin(yaw), 0.f, std::cos(yaw)};
        auto place = [&](const char* name, float ahead, float side, int team, float health) {
            Other o;
            o.id = uintptr_t(team) + 100;
            o.name = name;
            o.kind = "player";
            o.isPlayer = true;
            o.health = health;
            o.team = team;
            o.pos = {p.pos.x + fwd.x * ahead - fwd.z * side, p.pos.y, p.pos.z + fwd.z * ahead + fwd.x * side};
            s.others.push_back(o);
        };
        s.others.clear();
        place("Teammate", 1.8f, -1.2f, 1, 20.f);
        place("Opponent", oppDist_, 0.f, 2, 9.f + 5.f * std::sin(float(t_) * 0.8f));
        place("Bystander", 8.f, 3.f, 3, 17.f);
    }

    void confirms(std::vector<Event>& ev) {
        for (size_t i = 0; i < acks_.size();) {
            if (acks_[i].due > t_) {
                i++;
                continue;
            }
            Event e{EventKind::Confirm};
            e.value = acks_[i].ms;
            e.reach = acks_[i].reach;
            e.damage = 2.f + float(i % 5);
            e.text = "Opponent";
            ev.push_back(e);
            acks_.erase(acks_.begin() + long(i));
        }
    }

    void survival(State& s, double dt) {
        auto& p = s.player;
        if (!fighting_ && p.health > 0.f && p.health < 20.f) p.health = std::min(20.f, p.health + float(dt) * 0.8f);
        p.absorption = std::fmod(t_, 30.0) < 10.0 ? 4.f : 0.f;
        p.hunger = 20.f - std::fmod(float(t_) * 0.15f, 8.f);
        p.saturation = std::max(0.f, 5.f - std::fmod(float(t_) * 0.3f, 6.f));
        p.xp = std::fmod(float(t_) * 0.03f, 1.f);
        p.air = 300;
        p.onFire = false;
    }

    void world(State& s, double dt) {
        auto& w = s.world;
        timeAcc_ += dt * 20.0;
        w.time = int(std::fmod(1000.0 + timeAcc_, 24000.0));
        w.day = 12 + int((1000.0 + timeAcc_) / 24000.0);
        w.raining = std::fmod(t_, 90.0) > 60.0;
        w.thundering = std::fmod(t_, 180.0) > 150.0;
        w.ping = 38 + int(10.0 * std::sin(t_ * 0.8) + 4.0 * std::sin(t_ * 3.1));
        w.tps = 20.f - float(0.4 * (1.0 + std::sin(t_ * 0.35)));
        w.entities = 41 + int(6.0 * std::sin(t_ * 0.2));
        static const char* biomes[] = {"plains", "forest", "desert", "taiga"};
        w.biome = biomes[int(t_ / 45.0) % 4];
    }

    void target(State& s) {
        auto& t = s.target;
        double phase = std::fmod(t_, 16.0);
        if (phase > 4.0 && phase < 12.0) {
            t.kind = Target::Kind::Entity;
            t.name = "Opponent";
            t.isPlayer = true;
            t.distance = std::max(0.5f, oppDist_ - 0.3f);
            t.skinSize = 64;
            t.skin.assign(64 * 64, 0u);
            for (int y = 0; y < 16; y++)
                for (int x = 0; x < 32; x++) t.skin[size_t(y * 64 + x)] = (y < 8 ? 0xFF3A2A1Au : 0xFFC89A78u) | (x % 8 == 3 && y % 8 == 4 ? 0x00FFFFFFu : 0u);
            t.team = 2;
            t.armor = std::fmod(t_, 32.0) < 24.0 ? 4 : 3;
            t.health = std::max(0.f, opponentHp_);
            t.maxHealth = 20.f;
            t.pos = {s.player.pos.x + 2.f, s.player.pos.y, s.player.pos.z};
        } else if (phase > 12.2 && phase < 13.9) {
            t.kind = Target::Kind::Entity;
            t.name = "tnt";
            t.isPlayer = false;
            t.distance = 4.f;
            t.fuse = float(std::max(0.0, 4.0 - (phase - 12.2) * 2.4));
            float yaw = s.player.yaw * 0.0174533f;
            t.pos = {s.player.pos.x - std::sin(yaw) * 4.f, s.player.pos.y + 0.5f, s.player.pos.z + std::cos(yaw) * 4.f};
        } else if (phase > 14.5) {
            t.kind = Target::Kind::Block;
            t.name = "stone";
            t.blockX = int(s.player.pos.x) + 1;
            t.blockY = 63;
            t.blockZ = int(s.player.pos.z);
            t.distance = 3.f;
            t.breakProgress = float(std::fmod(t_ * 0.7, 1.0));
        } else {
            t.kind = Target::Kind::None;
            t.breakProgress = 0.f;
        }
    }

    struct Line {
        double at;
        const char* text;
    };

    void script(std::vector<Event>& ev, const Line* lines, size_t count, double period) {
        double now = std::fmod(t_, period), prev = scriptPhase_;
        scriptPhase_ = now;
        for (size_t i = 0; i < count; i++) {
            double at = lines[i].at;
            bool fire = now >= prev ? (at > prev && at <= now) : (at > prev || at <= now);
            if (!fire) continue;
            Event e{EventKind::Chat};
            e.text = lines[i].text;
            ev.push_back(std::move(e));
        }
    }

    void serverChat(const std::string& server, std::vector<Event>& ev) {
        static const Line hive[] = {
            {3, "§e[!] §fGet Hive+ for extra perks at shop.playhive.com"},
            {6, "§a» §7Steve joined the game"},
            {9, "§7[§6Hive+§7] §eAlex§f: hello everyone"},
            {11, "Luna: gg"},
            {14, "§bMika §7sent you a friend request. §eType /friend accept Mika"},
            {17, "§6Mika §7invited you to their party! §e/party accept Mika"},
            {20, "§eVote for a map: §bAquatic§7, §bLighthouse§7, §bCastle"},
            {25, "§eYou are the §cMurderer§e!"},
            {32, "§7Teaming is not allowed. §cNo Teaming§7!"},
            {40, "§cYou have been eliminated!"},
            {50, "§a§lGame OVER!"},
            {55, "§7Custom server code: §eHVE42X"},
        };
        static const Line zeqa[] = {
            {3, "§e[!] §fJoin our discord at discord.gg/zeqa"},
            {6, "§a+ §7Steve joined the server"},
            {10, "§bAlex §7sent you a duel request. §eType /duel accept Alex"},
            {14, "§bMika §7sent you a friend request. §eType /friend accept Mika"},
            {20, "§7Alex has a §c5§7 kill streak!"},
            {28, "§a§lAlex §7has won the duel!"},
        };
        if (server == "The Hive") script(ev, hive, std::size(hive), 60.0);
        else script(ev, zeqa, std::size(zeqa), 40.0);
    }

    void chatter(State& s, std::vector<Event>& ev) {
        const auto& server = demoServer();
        if (server == "The Hive" || server == "Zeqa") {
            serverChat(server, ev);
            return;
        }
        if (t_ < nextChat_) return;
        static const char* lines[] = {"<Luna> gg", "<Max> who has the pearl?", "§eThe game starts in 5 seconds", "<Kiki> nice kill",
                                      "§6Round 3 of 5 is starting", "<Noah> lag?", "§aMonchi Wars: You won!", "<MonchiPlayer> good luck!"};
        Event e{EventKind::Chat};
        e.text = lines[chatIdx_++ % 8];
        ev.push_back(e);
        nextChat_ = t_ + 4.0 + dist_(rng_) * 4.0;
        (void)s;
    }

    void uses(std::vector<Event>& ev) {
        if (t_ < nextUse_) return;
        nextUse_ = t_ + 7.0 + dist_(rng_) * 4.0;
        Event e{EventKind::ItemUse};
        e.item = dist_(rng_) < 0.7 ? "ender_pearl" : "chorus_fruit";
        ev.push_back(e);
    }

    void effects(State& s, double) {
        auto& list = s.player.effects;
        list.clear();
        auto make = [&](const char* id, int amp, float total, double period, double offset, bool good, uint32_t color) {
            float left = float(total - std::fmod(t_ + offset, period));
            if (left <= 0.f) return;
            Effect e;
            e.id = id;
            e.amplifier = amp;
            e.total = total;
            e.seconds = left;
            e.good = good;
            e.color = color;
            list.push_back(e);
        };
        make("speed", 1, 180.f, 190.0, 0.0, true, 0x7CAFC6);
        make("strength", 0, 90.f, 120.0, 20.0, true, 0x932423);
        make("regeneration", 1, 25.f, 40.0, 5.0, true, 0xCD5CAB);
        make("fire_resistance", 0, 60.f, 100.0, 30.0, true, 0xE49A3A);
        make("slowness", 0, 12.f, 70.0, 40.0, false, 0x5A6C81);
    }

    void scoreboard(State& s) {
        s.scoreboard.lines.clear();
        s.scoreboard.title = "Monchi Wars";
        if (demoServer() == "The Hive") {
            s.scoreboard.title = "BED WARS";
            s.scoreboard.lines.push_back({"Mode: Solos", 0});
            s.scoreboard.lines.push_back({"Map: Aquatic", 0});
            s.scoreboard.lines.push_back({"Kills", s.combat.kills});
            return;
        }
        s.scoreboard.lines.push_back({"Kills", s.combat.kills});
        s.scoreboard.lines.push_back({"Deaths", s.combat.deaths});
        s.scoreboard.lines.push_back({"Players", s.world.players});
        s.scoreboard.lines.push_back({"Round", 3});
        s.scoreboard.lines.push_back({"monchi.example", 0});
    }

    struct Ack {
        double due;
        float ms;
        float reach;
    };
    std::vector<Ack> acks_;
    float oppDist_ = 3.f;
    std::mt19937 rng_{1234};
    std::uniform_real_distribution<double> dist_{0.0, 1.0};
    bool init_ = false;
    double t_ = 0.0;
    double timeAcc_ = 0.0;
    double swingTimer_ = 0.0;
    double hurtTimer_ = 0.0;
    double respawnAt_ = 0.0;
    double nextChat_ = 2.0;
    double nextUse_ = 5.0;
    int chatIdx_ = 0;
    double scriptPhase_ = -1.0;
    double nextShot_ = 2.0;
    int shotId_ = 0;
    std::vector<Flight> flights_;
    double nextSound_ = 1.0;
    int soundIdx_ = 0;
    double crystalTimer_ = 3.0;
    int crystalIdx_ = 0;
    float opponentHp_ = 20.f;
    bool fighting_ = false;
    bool drawing_ = false;
    bool sprint_ = false;
    bool sneak_ = false;
};

}

std::unique_ptr<Provider> makeDemo() { return std::make_unique<Demo>(); }

}
