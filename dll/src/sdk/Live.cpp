#include "Providers.hpp"
#include "Memory.hpp"
#include "core/Bg.hpp"
#include "core/Client.hpp"
#include "core/Guard.hpp"
#include "core/Log.hpp"
#include "core/Paths.hpp"
#include "hook/Hook.hpp"
#include "hook/FreeCamera.hpp"
#include "hook/Input.hpp"
#include "hook/Net.hpp"
#include "render/Fonts.hpp"
#include "render/Ui.hpp"
#include "sig/Sigs.hpp"
#include "SynchedItem.hpp"
#include "BlockLookup.hpp"
#include "Dimension.hpp"
#include "HotbarSlot.hpp"
#include "ResidentPages.hpp"
#include "HiddenFlag.hpp"
#include <imgui.h>
#include "modules/flarial/FlarialModules.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <vector>

namespace game {

namespace {

class Live;
Live* self = nullptr;
bool (*originalAttack)(void*, void*) = nullptr;
void* (*originalAddMessage)(void*, void*, int) = nullptr;

int off(const char* name) { return sigs::offset(name, -1); }

// Fields that live behind other objects carry their pointer path as "<field>.via0", "<field>.via1", ...
// (1.26.52: position and view angles share an object at player>0x138>0x990; the copy reached through the
// ClientInstance only refreshes every few seconds).
uintptr_t follow(uintptr_t p, const char* name) {
    char key[96];
    for (int k = 0; k < 4 && p; k++) {
        std::snprintf(key, sizeof(key), "%s.via%d", name, k);
        int o = off(key);
        if (o < 0) break;
        p = mem::pointer(p + o);
    }
    return p;
}

// The inventory objects hang off the player through an entity registry whose layout changes from session to
// session, so no fixed pointer path reaches them. They are found once per world instead: a heap sweep for
// their vtables ("<name>.vtable", relative to the image), checked by a vector of item stacks at a known spot.
std::atomic<uintptr_t> handObj{0}, armorObj{0}, attrArray{0};
std::atomic<bool> sweeping{false};
// the sweep can find several of each; they are kept so the one that holds the player's real items can be told apart
std::mutex candLock;
std::vector<uintptr_t> handCands, armorCands;

uintptr_t imageBase() { return reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)); }

size_t imageSize() {
    static const size_t size = [] {
        uintptr_t base = imageBase();
        auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
        return size_t(reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew)->OptionalHeader.SizeOfImage);
    }();
    return size;
}

bool stackVector(uintptr_t holder, size_t count) {
    uintptr_t begin = mem::pointer(holder), end = mem::pointer(holder + 8);
    uintptr_t stackVt = imageBase() + uintptr_t(off("stack.vtable"));
    return begin && end - begin == count * size_t(off("stack.size")) && mem::pointer(begin) == stackVt;
}

bool validHand(uintptr_t o) {
    return o && mem::pointer(o + off("hand.tag")) == imageBase() + uintptr_t(off("hand.vtable")) && stackVector(o + off("hand.items"), 36);
}

bool validArmor(uintptr_t o) {
    return o && mem::pointer(o) == imageBase() + uintptr_t(off("armor.vtable")) && stackVector(o + off("armor.items"), size_t(off("armor.count")));
}

// Attribute instances (0x88 bytes: vtable, Attribute definition, ..., max at +0x78, value at +0x7c) sit in one
// array per entity. Only players carry the player.* attributes, and only the local one is sent them by a server.
uintptr_t attrDef(const char* name) { return imageBase() + uintptr_t(sigs::offset(std::string("attr.") + name, 0)); }

uintptr_t attrIn(uintptr_t array, const char* name) {
    uintptr_t vt = imageBase() + uintptr_t(off("attr.vtable")), def = attrDef(name);
    int size = off("attr.size");
    if (size <= 0) return 0;
    for (int k = 0; k < 32; k++) {
        uintptr_t e = array + uintptr_t(k) * size;
        if (mem::pointer(e) != vt) return 0;
        if (mem::pointer(e + 8) == def) return e;
    }
    return 0;
}

bool validAttrs(uintptr_t array) {
    return array && attrIn(array, "hunger") && attrIn(array, "health");
}

// Regions of the given size class only: the objects looked for are small heap objects, and on 1.26.52 they sit in
// regions below 1 MB that make up a fifth of the private memory. Those are swept first.
template <class Fn, class Stop>
void eachPrivateWord(size_t minRegion, size_t maxRegion, Fn&& fn, Stop&& stop) {
    std::vector<uint8_t> buf(1 << 20);
    MEMORY_BASIC_INFORMATION mbi{};
    size_t scanned = 0;
    for (uintptr_t at = 0x10000; !client::unloading() && VirtualQuery(reinterpret_cast<void*>(at), &mbi, sizeof(mbi));) {
        uintptr_t start = reinterpret_cast<uintptr_t>(mbi.BaseAddress), next = start + mbi.RegionSize;
        bool ok = mbi.State == MEM_COMMIT && mbi.Type == MEM_PRIVATE && (mbi.Protect & PAGE_READWRITE) && !(mbi.Protect & PAGE_GUARD) &&
                  mbi.RegionSize >= minRegion && mbi.RegionSize < maxRegion;
        for (uintptr_t chunk = start; ok && chunk < next && !client::unloading(); chunk += buf.size()) {
            size_t n = std::min<size_t>(buf.size(), next - chunk);
            resident::runs(chunk, n, [&](uintptr_t begin, size_t bytes) {
                if (!mem::readBytes(begin, buf.data(), bytes)) return;
                for (size_t i = 0; i + 8 <= bytes; i += 8) {
                    uintptr_t v;
                    std::memcpy(&v, buf.data() + i, 8);
                    fn(begin + i, v);
                }
                scanned += bytes;
            });
            if (stop()) return;
            if (scanned >= 8 << 20) {
                Sleep(1);
                scanned = 0;
            }
        }
        if (next <= at) break;
        at = next;
    }
}

void sweep() {
    auto began = GetTickCount64();
    struct Done {
        ~Done() { sweeping = false; }
    } done;
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    uintptr_t handVt = imageBase() + uintptr_t(off("hand.vtable")), armorVt = imageBase() + uintptr_t(off("armor.vtable"));
    uintptr_t attrVt = imageBase() + uintptr_t(off("attr.vtable")), hungerDef = attrDef("hunger");
    int handTag = off("hand.tag"), attrSize = off("attr.size");
    std::vector<uintptr_t> hands, armors, attrs;
    if (validAttrs(attrArray)) attrs.push_back(attrArray);
    auto collect = [&](uintptr_t at, uintptr_t v) {
        if (v == handVt && validHand(at - handTag)) hands.push_back(at - handTag);
        else if (v == armorVt && validArmor(at)) armors.push_back(at);
        else if (v == hungerDef && mem::pointer(at - 8) == attrVt) {
            uintptr_t e = at - 8;
            for (int k = 0; k < 64 && mem::pointer(e - attrSize) == attrVt; ++k) e -= attrSize;
            if (validAttrs(e) && std::find(attrs.begin(), attrs.end(), e) == attrs.end()) attrs.push_back(e);
        }
    };
    // what is known so far goes out at once; the readers tell the real inventory from a stale copy by its content
    // (chooseInventory), so the HUD has data after the first, short pass instead of after the whole sweep
    auto publish = [&](bool final) {
        if (!validHand(handObj) && !hands.empty()) handObj = hands.front();
        if (!validArmor(armorObj) && !armors.empty()) armorObj = armors.front();
        if (!validAttrs(attrArray) && !attrs.empty()) attrArray = attrs.front();
        std::scoped_lock g(candLock);
        handCands = hands;
        armorCands = armors;
        (void)final;
    };
    constexpr size_t small = 1 << 20, medium = 16 << 20;
    auto found = [&] { return !hands.empty() && !armors.empty() && !attrs.empty(); };
    eachPrivateWord(0, small, collect, [] { return false; });
    publish(false);
    if (!found()) eachPrivateWord(small, medium, collect, found);
    publish(false);
    if (!found()) eachPrivateWord(medium, ~size_t(0), collect, found);
    if (client::unloading()) return;
    publish(true);
    logger::info("live: inventory {} of {}, armor {} of {}, stats {} of {}", handObj ? "found" : "missing", hands.size(), armorObj ? "found" : "missing",
                 armors.size(), attrArray ? "found" : "missing", attrs.size());
    logger::info("live inventory discovery took {} ms", GetTickCount64() - began);
}
class Live : public Provider {
public:
    Live() { self = this; }
    ~Live() override {
        for (auto& [id, h] : hidden_) h.until = 0;
        applyHidden();
        detach(false);
        self = nullptr;
    }

    unsigned supports() const override {
        unsigned m = 0;
        if (sigs::address("LocalPlayer") && off("player.posX") >= 0) m |= unsigned(Domain::Player) | unsigned(Domain::Camera);
        if ((sigs::address("Level") || off("level.via0") >= 0) && off("level.time") >= 0) m |= unsigned(Domain::World);
        if ((m & unsigned(Domain::Player)) && off("hit.via0") >= 0) m |= unsigned(Domain::Target) | unsigned(Domain::Combat);
        if ((m & unsigned(Domain::Player)) && sigs::address("AttackEntity")) m |= unsigned(Domain::Combat);
        if ((m & unsigned(Domain::Player)) && off("hand.vtable") >= 0) m |= unsigned(Domain::Inventory);
        if ((m & unsigned(Domain::Player)) && off("chat.via0") >= 0) m |= unsigned(Domain::Chat);
        if ((m & unsigned(Domain::Player)) && off("player.registry") >= 0) m |= unsigned(Domain::Others) | unsigned(Domain::Tab);
        if ((m & unsigned(Domain::Player)) && off("scoreboard.via0") >= 0) m |= unsigned(Domain::Scoreboard);
        if ((m & unsigned(Domain::Player)) && off("pool.effects") != -1) m |= unsigned(Domain::Effects);
        return m;
    }

    bool derived() const override { return true; }

    void use(unsigned mask) override {
        wantAttack_ = (mask & unsigned(Domain::Combat)) != 0;
        wantChat_ = (mask & unsigned(Domain::Chat)) != 0;
        if ((mask & unsigned(Domain::Chat)) && !chatHooked_ && sigs::address("GuiAddMessage") && off("chat.lines") >= 0) {
            chatHooked_ = hook::create("GuiAddMessage", reinterpret_cast<void*>(sigs::address("GuiAddMessage")), addMessage, &originalAddMessage);
            if (chatHooked_) hook::enableAll();
        }
        if (wantAttack_ && !hooked_ && sigs::address("AttackEntity")) {
            hooked_ = hook::create("AttackEntity", reinterpret_cast<void*>(sigs::address("AttackEntity")), attack, &originalAttack);
            if (hooked_) hook::enableAll();
        }
    }

    void update(State& s, std::vector<Event>& ev) override {
        {
            std::scoped_lock g(lock_);
            for (auto& e : pending_) ev.push_back(std::move(e));
            pending_.clear();
        }
        unsigned have = supports();
        playerPtr_ = 0;
        if (have & unsigned(Domain::Player)) readPlayer(s);
        if (playerPtr_) readSelfFlags(s.player);
        std::array<uintptr_t, 32> attacks{};
        unsigned count = flarialModules::attacks(attacks.data(), unsigned(attacks.size()));
        for (unsigned k = 0; k < count; ++k) attacked(reinterpret_cast<void*>(attacks[k]));
        {
            std::scoped_lock g(lock_);
            for (auto& e : pending_) ev.push_back(std::move(e));
            pending_.clear();
        }
        if (playerPtr_ && (have & unsigned(Domain::Target))) readTarget(s, ev);
        if (playerPtr_ && (have & unsigned(Domain::Inventory))) {
            readStats(s);
            readInventory(s, ev);
            readUse(s, ev);
        }
        if (playerPtr_ && (have & unsigned(Domain::Others))) readEntities(s);
        if (playerPtr_ && s.time >= nextSkinRead_) readPlatforms(s);
        s.camera.live = playerPtr_ && readCamera(s.camera);
        // a camera away from the eyes is a third person view, also while freelook or a forced perspective is on
        if (s.camera.live && s.player.view == View::First && distance(s.camera.pos, eye_) > 1.f) s.player.view = View::Back;
        if (playerPtr_ && (have & unsigned(Domain::Effects))) readEffects(s);
        if (playerPtr_ && (have & unsigned(Domain::Scoreboard))) readScoreboard(s);
        if (playerPtr_) applyHidden();
        else { s.skin = {}; nextSkinRead_ = 0.0; }
        if (have & unsigned(Domain::World)) readWorld(s);
        if (playerPtr_ && (have & unsigned(Domain::Chat))) readChat(ev);
        if (playerPtr_ && (have & unsigned(Domain::Chat))) sweepChatHud();
        else chatPrimed_ = false;
        bool playing = input::gameplay();
        // menu screens keep sending (LAN discovery, Xbox Live, server list pings), so traffic only counts
        // as "in a world" when it carried on from a moment the player was actually playing
        // a server transfer (lobby to game) drops the session for a moment, so only a longer silence counts
        bool online = net::session();
        uint64_t now = GetTickCount64();
        if (online) offlineSince_ = 0;
        else if (!offlineSince_) offlineSince_ = now;
        bool left = !online && now - offlineSince_ > 8000;
        bool grabbed = input::grabbed();
        if (grabbed) onlineSincePlay_ = online || (onlineSincePlay_ && !left);
        else if (onlineSincePlay_ && left) {
            onlineSincePlay_ = false;
            input::forgetPlay();
            playing = false;
        }
        s.inWorld = playerPtr_ != 0 || (!sigs::address("LocalPlayer") && (playing || onlineSincePlay_));
        s.screen = s.inWorld && !grabbed && !ui::wantsCursor() ? Screen::Other : Screen::None;
        int screen = flarialModules::screen();
        input::screenHint(screen);
        if (s.inWorld && screen) {
            // a server's form (shops, game menus) has a screen name of its own and leaves the hud named as the top
            // screen, but it shows the cursor like every menu does
            bool form = screen == 1 && input::cursorShown() && !ui::wantsCursor();
            s.screen = screen == 1 ? (form ? Screen::Other : Screen::None) : screen == 2 ? Screen::Pause :
                screen == 3 ? Screen::Inventory : screen == 4 ? Screen::Chat : Screen::Other;
        }
    }

    void attacked(void* actor) {
        if (!wantAttack_ || !playerPtr_) return;
        uintptr_t address = reinterpret_cast<uintptr_t>(actor);
        // on a server the struck entity lives in the registry the crosshair's ids belong to, not in the player's
        uintptr_t reg = 0;
        uint32_t id = 0xffffffff;
        for (uintptr_t r : {entityReg_, mem::pointer(playerPtr_ + off("player.registry"))}) {
            if (!r || id != 0xffffffff) continue;
            uintptr_t owners = pool(r, "pool.owner");
            uintptr_t begin = owners ? mem::pointer(owners + 0x20) : 0;
            uintptr_t end = owners ? mem::pointer(owners + 0x28) : 0;
            for (uintptr_t at = begin; begin && at < end && at < begin + 65536; at += 4) {
                uint32_t candidate = mem::get<uint32_t>(at, 0xffffffff);
                uintptr_t owner = component(owners, candidate, 8);
                if (owner && mem::pointer(owner) == address) { id = candidate; reg = r; break; }
            }
        }
        if (id == 0xffffffff) return;
        Target target;
        describe(target, reg, id);
        Event e{EventKind::Hit};
        e.reach = reachTo(address);
        e.crit = !onGround_ && fallSpeed_ < 0.f && !sprinting_;
        e.actor = uintptr_t(id) | (uintptr_t(1) << 40);
        e.crystal = target.name == "ender_crystal" || target.name == "minecraft:ender_crystal";
        std::scoped_lock g(lock_);
        pending_.push_back(std::move(e));
    }

private:
    static bool attack(void* gm, void* actor) {
        if (self) self->attacked(actor);
        return originalAttack(gm, actor);
    }

    // What the gui's message list looks like: its length and its two ends. The list holds at most a hundred lines and
    // loses the oldest for a new one, so the length alone does not tell that a line came in.
    struct ChatMark {
        bool ok = false;
        size_t count = 0;
        std::string first, last;
    };

    // The launcher shows the figure with the skin worn last. The file holds the picture and the arm width, nothing that
    // says whose it is.
    static void keepSkin(const Skin& skin) {
        bg::run([skin] {
            std::string out = "MSKN";
            uint32_t head[3] = {uint32_t(skin.width), uint32_t(skin.height), skin.slim ? 1u : 0u};
            out.append(reinterpret_cast<const char*>(head), sizeof(head));
            out.append(reinterpret_cast<const char*>(skin.rgba.data()), skin.rgba.size());
            auto file = paths::cache() / L"skin.bin", part = paths::cache() / L"skin.bin.part";
            {
                std::ofstream f(part, std::ios::binary | std::ios::trunc);
                f.write(out.data(), std::streamsize(out.size()));
                if (!f) return;
            }
            std::error_code ec;
            std::filesystem::rename(part, file, ec);
        });
    }

    static ChatMark chatMark(uintptr_t gui) {
        ChatMark m;
        uintptr_t vec = gui + uintptr_t(off("chat.lines"));
        uintptr_t begin = mem::pointer(vec), end = mem::pointer(vec + 8);
        int size = off("chat.size");
        if (size <= 0 || !begin || end < begin || (end - begin) % size || (end - begin) / size > 1000) return m;
        m.ok = true;
        m.count = (end - begin) / size;
        if (!m.count) return m;
        m.first = text(begin + off("chat.text"));
        m.last = text(begin + (m.count - 1) * size + off("chat.text"));
        return m;
    }

    // 1.26.52, static: every way the game shows a line (chat, system, whisper, ten callers) ends in this one function
    // (0x155aad0), which appends the finished line to the list at +0x150 of its first argument. The line is read from
    // there after the game has built it, so it is exactly what the game shows.
    // The game's strings come from its own allocator (static: its string code calls an allocator object, not the
    // C runtime), so a text of ours must never end up inside one by assignment from here. The game's own routine
    // for that (0x9daf0: string, new length, unused, source bytes) makes the buffer itself and frees the old one.
    static void put(uintptr_t string, const std::string& value) {
        auto assign = reinterpret_cast<void (*)(void*, size_t, void*, const char*)>(sigs::address("GameStringAssign"));
        size_t len = mem::get<size_t>(string + 16), cap = mem::get<size_t>(string + 24);
        if (!assign || cap < 15 || len > cap || cap > (1u << 20) || value.empty()) return;
        assign(reinterpret_cast<void*>(string), value.size(), nullptr, value.data());
    }

    static void decorate(uintptr_t message) {
        int sender = off("chat.sender"), body = off("chat.body");
        if (sender < 0 || body < 0 || !sigs::address("GameStringAssign")) return;
        std::string from = text(message + uintptr_t(sender)), line = text(message + uintptr_t(body));
        std::string newFrom = from, newLine = line;
        if (!game::chatDecor(newFrom, newLine)) return;
        if (newFrom != from) put(message + uintptr_t(sender), newFrom);
        if (newLine != line) put(message + uintptr_t(body), newLine);
        static bool told = false;
        if (!told) {
            told = true;
            logger::info("live: a chat line got a Monchi user's style before the game showed it");
        }
    }

    static void* addMessage(void* gui, void* message, int a3) {
        guard::call("chat decor", [&] { decorate(reinterpret_cast<uintptr_t>(message)); });
        ChatMark before;
        bool listen = self && self->wantChat_;
        bool native = false;
        if (listen) {
            self->chatFromHook_ = true;
            before = chatMark(reinterpret_cast<uintptr_t>(gui));
            guard::call("chat line", [&] { native = self->incoming(reinterpret_cast<uintptr_t>(message), before.ok); });
        }
        void* result = originalAddMessage(gui, message, a3);
        if (listen && self) guard::call("chat line", [&] { self->chatAdded(reinterpret_cast<uintptr_t>(gui), before, native); });
        return result;
    }

    // one character of UTF-8; the number of bytes it took, 0 for a broken sequence
    static int utf8(unsigned& code, const char* at, const char* end) {
        auto byte = [&](int k) { return unsigned(static_cast<unsigned char>(at[k])); };
        unsigned lead = byte(0);
        int n = lead < 0x80 ? 1 : (lead >> 5) == 6 ? 2 : (lead >> 4) == 14 ? 3 : (lead >> 3) == 30 ? 4 : 0;
        if (!n || end - at < n) return 0;
        code = n == 1 ? lead : lead & (0xffu >> (n + 1));
        for (int k = 1; k < n; k++) {
            if ((byte(k) >> 6) != 2) return 0;
            code = (code << 6) | (byte(k) & 0x3f);
        }
        return n;
    }

    static bool drawable(const std::string& value) {
        const char* at = value.data();
        const char* end = at + value.size();
        while (at < end) {
            unsigned code = 0;
            int n = utf8(code, at, end);
            if (n <= 0) return false;
            at += n;
            if (code == 0xA7 && at < end) {
                // a color code: the sign and the character after it are not drawn
                unsigned skip = 0;
                int m = utf8(skip, at, end);
                at += m > 0 ? m : 1;
                continue;
            }
            if (!fonts::covers(code)) return false;
        }
        return true;
    }

    // 1.26.52, static: the message carries the sender at +0x08, the text at +0x28 and at +0x90 the seconds it stays in
    // the hud (displayClientMessage passes 10); the list entry gets them at +0xfc, GuiData::tick (0x1557c40) counts
    // them down and HudScreenController::tick (0x4ea1e09) only shows entries that have time left. A line that Better
    // Chat takes over starts with none, so the game's hud never shows it while its chat screen still lists it.
    // Returns true for a line with characters only the game's own font has; Better Chat has the game draw that one
    // inside its box, and without the core it stays in the game's hud.
    bool incoming(uintptr_t message, bool listReadable) {
        int sender = off("chat.sender"), body = off("chat.body"), life = off("chat.life");
        if (sender < 0 || body < 0) return false;
        std::string from = text(message + uintptr_t(sender)), line = text(message + uintptr_t(body));
        bool native = !drawable(from) || !drawable(line);
        if (native && nativeLogged_ < 3) {
            nativeLogged_++;
            std::string codes;
            const char* at = line.data();
            const char* end = at + line.size();
            for (int k = 0; k < 24 && at < end; k++) {
                unsigned code = 0;
                int n = utf8(code, at, end);
                if (n <= 0) break;
                at += n;
                codes += std::format("{}{:X}", k ? " " : "", code);
            }
            logger::info("live: a chat line has characters the menu fonts lack ({}): {}",
                         game::nativeChatDrawn() ? "the game's font draws it in Better Chat" : "it stays in the game's chat", codes);
        }
        if ((!native || game::nativeChatDrawn()) && listReadable && life >= 0 && game::chatHudHidden()) mem::write<float>(message + uintptr_t(life), 0.f);
        return native;
    }

    void chatAdded(uintptr_t gui, const ChatMark& before, bool native) {
        ChatMark after = chatMark(gui);
        // a list that cannot be read leaves the reader in charge
        if (!after.ok) chatFromHook_ = false;
        if (!after.ok || !after.count || after.last.empty()) return;
        if (before.ok && after.count == before.count && after.first == before.first && after.last == before.last) return;
        if (!chatLogged_.exchange(true)) logger::info("live: chat lines come from the game's own message function");
        // The time handed over with the message does not keep the line off the game's hud in every case. The entry
        // the game has just appended carries the time the hud goes by at +0xfc; with none left the hud never shows
        // it, the chat screen still lists it.
        if (int life = off("chat.entryLife"); life > 0 && game::chatHudHidden() && (!native || game::nativeChatDrawn())) {
            uintptr_t begin = mem::pointer(gui + uintptr_t(off("chat.lines")));
            if (begin) mem::write<float>(begin + (after.count - 1) * size_t(off("chat.size")) + uintptr_t(life), 0.f);
        }
        Event e{EventKind::Chat};
        e.text = after.last;
        e.native = native;
        std::scoped_lock g(lock_);
        pending_.push_back(std::move(e));
    }

    static float f(uintptr_t base, const char* name, float fallback = 0.f) {
        int o = off(name);
        if (o < 0) return fallback;
        uintptr_t at = follow(base, name);
        return at ? mem::get<float>(at + o, fallback) : fallback;
    }

    static int i(uintptr_t base, const char* name, int fallback = 0) {
        int o = off(name);
        if (o < 0) return fallback;
        uintptr_t at = follow(base, name);
        return at ? mem::get<int>(at + o, fallback) : fallback;
    }

    static bool b(uintptr_t base, const char* name, bool fallback = false) {
        int o = off(name);
        if (o < 0) return fallback;
        uintptr_t at = follow(base, name);
        if (!at) return fallback;
        // some states are one value of a shared enum byte ("<field>.is")
        char key[96];
        std::snprintf(key, sizeof(key), "%s.is", name);
        uint8_t v = mem::get<uint8_t>(at + o, fallback ? 1 : 0);
        int is = off(key);
        return is >= 0 ? v == is : v != 0;
    }

    void readPlayer(State& s) {
        uintptr_t p = mem::pointer(sigs::address("LocalPlayer"));
        if (!p) return;
        uintptr_t pb = follow(p, "player.posX");
        if (!pb) return;
        playerPtr_ = p;
        auto& pl = s.player;
        int px = off("player.posX");
        pl.pos = {mem::get<float>(pb + px), mem::get<float>(pb + px + 4), mem::get<float>(pb + px + 8)};
        // the game keeps the player's position at eye level, feet + 1.62 even while sneaking
        if (off("player.eyeLevel") > 0) pl.pos.y -= 1.62f;
        if (int vx = off("player.velX"); vx >= 0) {
            uintptr_t vb = follow(p, "player.velX");
            pl.vel = {mem::get<float>(vb + vx) * 20.f, mem::get<float>(vb + vx + 4) * 20.f, mem::get<float>(vb + vx + 8) * 20.f};
        } else {
            deriveVelocity(pl);
        }
        // without state flags the movement itself tells: walking tops out at 4.3 blocks/s, sprinting at 5.6,
        // and standing on something keeps the vertical speed at zero
        if (off("has.MoveState") > 0 && off("player.sprinting") < 0) {
            float flat = std::hypot(pl.vel.x, pl.vel.z);
            pl.onGround = std::fabs(pl.vel.y) < 0.05f;
            pl.sprinting = flat > 4.9f;
            pl.sneaking = input::grabbed() && (input::down(VK_LSHIFT) || input::down(VK_RSHIFT));
        }
        // the pick ray right behind the eye position points where the player looks, scaled to the pick range
        if (off("player.ray") > 0) {
            float dx = mem::get<float>(pb + px + 12), dy = mem::get<float>(pb + px + 16), dz = mem::get<float>(pb + px + 20);
            float len = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (len > 0.01f && std::isfinite(len)) {
                pl.pitch = std::asin(std::clamp(-dy / len, -1.f, 1.f)) * 57.29578f;
                pl.yaw = std::atan2(-dx, dz) * 57.29578f;
            }
        }
        pl.fov = f(p, "player.fov", pl.fov);
        pl.view = View(std::clamp(i(p, "player.view", int(pl.view)), 0, 2));
        pl.health = f(p, "player.health", pl.health);
        pl.maxHealth = f(p, "player.maxHealth", pl.maxHealth);
        pl.hunger = f(p, "player.hunger", pl.hunger);
        pl.saturation = f(p, "player.saturation", pl.saturation);
        pl.eyeHeight = f(p, "player.eyeHeight", pl.sneaking ? 1.27f : 1.62f);
        pl.air = i(p, "player.air", pl.air);
        pl.level = i(p, "player.level", pl.level);
        pl.dimension = i(p, "player.dimension", pl.dimension);
        pl.onGround = b(p, "player.onGround", pl.onGround);
        pl.sprinting = b(p, "player.sprinting", pl.sprinting);
        pl.sneaking = b(p, "player.sneaking", pl.sneaking);
        onGround_ = pl.onGround;
        sprinting_ = pl.sprinting;
        fallSpeed_ = pl.vel.y;
        eye_ = pl.eye();
    }

    // The player's pick result (1.26.52: player+0x1e8): ray start = eye (3 float), ray (3 float), type (int, 0 block,
    // 1 entity, 3 nothing), face, block position (3 int), hit point (3 float), entity reference (+0x38, entity id
    // at +0x48). The game attacks whatever this points at when the attack button goes down, so a press while it
    // holds an entity is a swing at that entity.
    void readTarget(State& s, std::vector<Event>& ev) {
        uintptr_t h = follow(playerPtr_, "hit");
        auto& t = s.target;
        if (!h) {
            t = {};
            return;
        }
        int type = mem::get<int>(h + 0x18, 3);
        Vec3 from{mem::get<float>(h), mem::get<float>(h + 4), mem::get<float>(h + 8)};
        Vec3 at{mem::get<float>(h + 0x2c), mem::get<float>(h + 0x30), mem::get<float>(h + 0x34)};
        uintptr_t id = mem::get<uint32_t>(h + 0x48) | (uintptr_t(1) << 40);
        if (type != 1) t = {};
        t.kind = type == 0 ? Target::Kind::Block : type == 1 ? Target::Kind::Entity : Target::Kind::None;
        t.pos = at;
        t.distance = t.kind == Target::Kind::None ? 0.f : distance(from, at);
        t.breakProgress = 0.f;
        if (t.kind == Target::Kind::Block) {
            t.blockX = mem::get<int>(h + 0x20);
            t.blockY = mem::get<int>(h + 0x24);
            t.blockZ = mem::get<int>(h + 0x28);
            t.breakProgress = breakProgress(t);
            // The name is read out of the chunk the game has cached at that moment (BlockLookup.hpp), so it can take
            // a few frames to come. Not in the first second of a world or once the player is gone: while a world is
            // torn down the chunk is half freed.
            double now = s.time;
            if (playerPtr_ != blockPlayer_) {
                blockPlayer_ = playerPtr_;
                blockSince_ = now;
                blockName_.clear();
            }
            bool moved = t.blockX != blockAt_[0] || t.blockY != blockAt_[1] || t.blockZ != blockAt_[2];
            if (moved) blockName_.clear();
            if (now - blockSince_ > 1.0 && (moved || blockName_.empty() || now - blockAsked_ > 0.25)) {
                blockAt_ = {t.blockX, t.blockY, t.blockZ};
                blockAsked_ = now;
                uintptr_t reg = mem::pointer(playerPtr_ + off("player.registry"));
                uintptr_t owner = component(pool(reg, "pool.owner"), selfId(reg), 8);
                std::string found = blockLookup::name(owner ? mem::pointer(owner) : 0, imageBase(), imageSize(),
                    {off("actor.dimension"), off("dimension.blockSource"), off("dimension.blockSourceAccessor"),
                     off("blockSource.lookup"), off("block.type"), off("block.name")},
                    {t.blockX, t.blockY, t.blockZ});
                if (!found.empty()) blockName_ = std::move(found);
            }
            t.name = blockName_;
            if (!t.name.empty() && !blockNamesLogged_) {
                blockNamesLogged_ = true;
                logger::info("live: block name lookup verified, first block {}", t.name);
            }
        }

        uint32_t rawId = mem::get<uint32_t>(h + 0x48);
        uintptr_t reg = mem::pointer(h + off("hit.registry"));
        // only a pick on an entity carries a live reference; aimed at a block or the sky the field holds whatever was
        // there before, and taking that over dropped every hidden crystal the moment the crosshair left it
        if (reg && type == 1) entityReg_ = reg;
        if (t.kind == Target::Kind::Entity && reg) describe(t, reg, rawId);
        followHits(reg, ev);

        bool press = input::down(VK_LBUTTON) && input::grabbed() && !ui::wantsCursor();
        bool edge = press && !wasPressed_;
        wasPressed_ = press;
        if (!edge || t.kind != Target::Kind::Entity) return;
        hitId_ = rawId;
        hitAt_ = GetTickCount64();
        hitHealth_ = t.health;
        hitHurt_ = 99;
        hitName_ = t.name;
        hitReach_ = t.distance;
        Event e{EventKind::Hit};
        e.reach = t.distance;
        e.crit = !onGround_ && fallSpeed_ < 0.f && !sprinting_;
        e.crystal = t.name == "ender_crystal" || t.name == "minecraft:ender_crystal";
        if (std::any_of(ev.begin(), ev.end(), [&](const Event& hit) {
            return hit.kind == EventKind::Hit && hit.actor == id;
        })) return;
        e.actor = id;
        e.hasPos = true;
        e.pos = at;
        ev.push_back(std::move(e));
    }

    // Entities live in an entt registry (1.26.52: the pick result's weak reference points at it). Its pool map is a
    // dense_map whose packed nodes sit at "registry.pools" (32 bytes: next, component type hash, shared_ptr to the
    // pool). A pool keeps sparse pages of (version | position) at +0x08, the packed entity list at +0x20 and payload
    // pages at +0x50 (1024 components per page). Ids are 18 bits of entity and 14 of version; a free slot in the
    // packed list carries the version 0x3fff.
    uintptr_t pool(uintptr_t reg, const char* name) {
        uint32_t key = uint32_t(off(name));
        auto& cache = pools_[name];
        if (cache.first == reg && cache.second) return cache.second;
        uintptr_t begin = mem::pointer(reg + off("registry.pools")), end = mem::pointer(reg + off("registry.pools") + 8);
        cache = {reg, 0};
        for (uintptr_t n = begin; n && n < end && n < begin + 0x10000 * 32; n += 32)
            if (mem::get<uint32_t>(n + 8) == key) {
                cache.second = mem::pointer(n + 0x10);
                break;
            }
        return cache.second;
    }

    static uintptr_t component(uintptr_t p, uint32_t id, int size) {
        if (!p) return 0;
        constexpr uint32_t mask = 0x3ffff;
        uint32_t idx = id & mask;
        uintptr_t sparse = mem::pointer(p + 8), sparseEnd = mem::pointer(p + 0x10);
        if (!sparse || (idx / 4096) * 8 >= sparseEnd - sparse) return 0;
        uintptr_t page = mem::pointer(sparse + (idx / 4096) * 8);
        uint32_t entry = page ? mem::get<uint32_t>(page + (idx % 4096) * 4, 0xffffffff) : 0xffffffff;
        if (entry == 0xffffffff) return 0;
        uint32_t pos = entry & mask;
        uintptr_t packed = mem::pointer(p + 0x20);
        if (mem::get<uint32_t>(packed + uintptr_t(pos) * 4) != id) return 0;
        uintptr_t payload = mem::pointer(mem::pointer(p + 0x50) + (pos / 1024) * 8);
        return payload ? payload + uintptr_t(pos % 1024) * size : 0;
    }

    void describe(Target& t, uintptr_t reg, uint32_t id) {
        t.name.clear();
        t.isPlayer = false;
        if (uintptr_t def = component(pool(reg, "pool.identifier"), id, off("identifier.size"))) {
            t.name = text(def + off("identifier.name"));
            t.isPlayer = t.name == "player";
        }
        if (t.isPlayer) {
            std::string shown = playerName(reg, id);
            if (!shown.empty()) t.name = shown;
        }
        t.fuse = t.name == "tnt" ? fuse(reg, id) : 0.f;
        t.hasBox = box(reg, id, t.boxMin, t.boxMax);
        // ActorRotationComponent: pitch, yaw in degrees, then the previous tick's
        if (uintptr_t r = component(pool(reg, "pool.rotation"), id, off("rotation.size"))) {
            t.lookPitch = mem::get<float>(r);
            t.lookYaw = mem::get<float>(r + 4);
        }
        uintptr_t attrs = component(pool(reg, "pool.attributes"), id, off("attributes.size"));
        uintptr_t first = attrs ? mem::pointer(attrs + off("attributes.list")) : 0;
        if (uintptr_t hp = first ? attrIn(first, "health") : 0) {
            t.health = mem::get<float>(hp + off("attr.value"));
            t.maxHealth = mem::get<float>(hp + off("attr.max"));
        } else {
            t.health = 0.f;
            t.maxHealth = 0.f;
        }
    }

    // SynchedActorData keeps a vector of data items (vtable, type byte at +8, id at +0xa, value at +0x10); item 4
    // is the name tag, a string (type 4)
    std::string playerName(uintptr_t reg, uint32_t id) {
        uintptr_t data = component(pool(reg, "pool.synched"), id, off("synched.size"));
        if (!data) return {};
        uintptr_t begin = mem::pointer(data), end = mem::pointer(data + 8);
        int slot = off("synched.name");
        if (!begin || end <= begin + uintptr_t(slot) * 8) return {};
        uintptr_t item = mem::pointer(begin + uintptr_t(slot) * 8);
        if (!item || mem::get<uint8_t>(item + 8) != 4) return {};
        return text(item + 0x10);
    }

    float fuse(uintptr_t reg, uint32_t id) {
        int slot = off("synched.fuse");
        if (slot < 0 || slot > 255) return 0.f;
        uintptr_t data = component(pool(reg, "pool.synched"), id, off("synched.size"));
        if (!data) return 0.f;
        uintptr_t begin = mem::pointer(data), end = mem::pointer(data + 8);
        if (!begin || end <= begin || end - begin > 256 * 8 || (end - begin) % 8 ||
            uintptr_t(slot) >= (end - begin) / 8) return 0.f;
        uintptr_t item = mem::pointer(begin + uintptr_t(slot) * 8);
        std::array<uint8_t, 16> bytes{};
        if (!item || !mem::readBytes(item, bytes.data(), bytes.size())) return 0.f;
        auto ticks = synchedItem::integer(bytes, uint16_t(slot));
        if (!ticks || *ticks < 0) return 0.f;
        return (float(*ticks) + 1.f) / 20.f;
    }

    // The game mode (1.26.52: actor+0xaa0) keeps the block being broken at +0x10 and its progress (0..1) at +0x24.
    // The actor object is the one the registry's ActorOwnerComponent holds, not the LocalPlayer global.
    float breakProgress(const Target& t) {
        uintptr_t reg = mem::pointer(playerPtr_ + off("player.registry"));
        uintptr_t owner = component(pool(reg, "pool.owner"), selfId(reg), 8);
        uintptr_t gm = owner ? mem::pointer(mem::pointer(owner) + off("actor.gameMode")) : 0;
        if (!gm) return 0.f;
        int at = off("gameMode.block");
        if (mem::get<int>(gm + at) != t.blockX || mem::get<int>(gm + at + 4) != t.blockY || mem::get<int>(gm + at + 8) != t.blockZ) return 0.f;
        float p = mem::get<float>(gm + off("gameMode.progress"));
        return std::isfinite(p) ? std::clamp(p, 0.f, 1.f) : 0.f;
    }

    // AABBShapeComponent holds the box as min and max corner, then width and height. It moves in ticks, so it is
    // shifted by how far the interpolated render position has moved past the tick position.
    bool box(uintptr_t reg, uint32_t id, Vec3& lo, Vec3& hi) {
        uintptr_t b = component(pool(reg, "pool.aabb"), id, off("aabb.size"));
        if (!b) return false;
        float v[6];
        if (!mem::read(b, v)) return false;
        Vec3 shift;
        uintptr_t st = component(pool(reg, "pool.state"), id, off("state.size"));
        uintptr_t r = component(pool(reg, "pool.render"), id, off("render.size"));
        if (st && r) shift = {mem::get<float>(r) - mem::get<float>(st), mem::get<float>(r + 4) - mem::get<float>(st + 4), mem::get<float>(r + 8) - mem::get<float>(st + 8)};
        lo = {v[0] + shift.x, v[1] + shift.y, v[2] + shift.z};
        hi = {v[3] + shift.x, v[4] + shift.y, v[5] + shift.z};
        return hi.x > lo.x && hi.y > lo.y && hi.z > lo.z;
    }

    // The camera the frame is drawn from is the entity with a GameCameraComponent. Its CameraComponent (0x120
    // bytes) holds the orientation as a quaternion at +0x30 (forward is -z), the position at +0x40 and the vertical
    // field of view in radians at +0x50, so third person, freelook and zoom project right.
    bool readCamera(Camera& cam) {
        uintptr_t reg = mem::pointer(playerPtr_ + off("player.registry"));
        uintptr_t game = pool(reg, "pool.gameCamera");
        uintptr_t packed = game ? mem::pointer(game + 0x20) : 0;
        if (!packed || mem::pointer(game + 0x28) <= packed) return false;
        uintptr_t c = component(pool(reg, "pool.camera"), mem::get<uint32_t>(packed), off("camera.size"));
        float v[9];
        if (!c || !mem::read(c + off("camera.orientation"), v)) return false;
        float x = v[0], y = v[1], z = v[2], w = v[3];
        float norm = x * x + y * y + z * z + w * w;
        if (!std::isfinite(norm) || std::fabs(norm - 1.f) > 0.01f || !std::isfinite(v[4] + v[5] + v[6])) return false;
        Vec3 fwd{-2.f * (x * z + w * y), -2.f * (y * z - w * x), -(1.f - 2.f * (x * x + y * y))};
        cam.pos = {v[4], v[5], v[6]};
        cam.pitch = std::asin(std::clamp(-fwd.y, -1.f, 1.f)) * 57.29578f;
        cam.yaw = std::atan2(-fwd.x, fwd.z) * 57.29578f;
        float fov = mem::get<float>(c + off("camera.fov"));
        if (fov > 0.1f && fov < 3.f) cam.fov = fov * 57.29578f;
        return true;
    }

    // only the local player has a LocalPlayerComponent, an empty tag, so that pool's single entity is us
    // 1.26.52, live: MobBodyRotationComponent is {body yaw, last tick's}, ActorRotationComponent {pitch, yaw, last
    // tick's two}, ActorWalkAnimationComponent {multiplier, last speed, speed, position}; the pool ids are the
    // FNV-1a hashes of those names. The render position is at eye height like the tick position, the box gives the feet.
    bool pose(uintptr_t reg, uint32_t id, Pose& out) {
        out.known = false;
        if (off("pool.bodyRotation") == -1) return false;
        uintptr_t st = component(pool(reg, "pool.state"), id, off("state.size"));
        uintptr_t r = component(pool(reg, "pool.render"), id, off("render.size"));
        uintptr_t body = component(pool(reg, "pool.bodyRotation"), id, 8);
        uintptr_t rot = component(pool(reg, "pool.rotation"), id, off("rotation.size"));
        if (!st || !r || !body || !rot) return false;
        out.feet = {mem::get<float>(r), mem::get<float>(r + 4), mem::get<float>(r + 8)};
        if (uintptr_t box = component(pool(reg, "pool.aabb"), id, off("aabb.size"))) out.feet.y -= mem::get<float>(st + 4) - mem::get<float>(box + 4);
        else out.feet.y -= 1.62f;
        out.body = mem::get<float>(body);
        out.pitch = mem::get<float>(rot);
        out.head = mem::get<float>(rot + 4);
        if (uintptr_t walk = off("pool.walk") != -1 ? component(pool(reg, "pool.walk"), id, 16) : 0) {
            out.walkSpeed = mem::get<float>(walk + 8);
            out.walkPos = mem::get<float>(walk + 12);
        }
        out.known = std::isfinite(out.feet.x) && std::isfinite(out.feet.y) && std::isfinite(out.feet.z) && std::isfinite(out.body) && std::isfinite(out.head);
        return out.known;
    }

    uint32_t selfId(uintptr_t reg) {
        uintptr_t p = pool(reg, "pool.localPlayer");
        uintptr_t packed = p ? mem::pointer(p + 0x20) : 0;
        return packed && mem::pointer(p + 0x28) > packed ? mem::get<uint32_t>(packed, 0xffffffff) : 0xffffffff;
    }

    static float health(uintptr_t attrs, float* max) {
        uintptr_t first = attrs ? mem::pointer(attrs + off("attributes.list")) : 0;
        uintptr_t hp = first ? attrIn(first, "health") : 0;
        if (!hp) return 0.f;
        if (max) *max = mem::get<float>(hp + off("attr.max"));
        return mem::get<float>(hp + off("attr.value"));
    }

    // Every actor carries an identifier component, so its pool's packed list is the list of loaded entities.
    // Positions come from the render position (interpolated between ticks); the state vector holds the tick
    // position, the one before and the motion, and the box bottom tells how far the position sits above the feet.
    void readEntities(State& s) {
        uintptr_t reg = mem::pointer(playerPtr_ + off("player.registry"));
        uint32_t self = selfId(reg);
        if (std::string me = playerName(reg, self); !me.empty()) s.player.name = me;
        s.player.hasBox = box(reg, self, s.player.boxMin, s.player.boxMax);
        pose(reg, self, s.player.pose);
        s.others.clear();
        s.shots.clear();
        s.tab.clear();
        if (!s.player.name.empty()) s.tab.push_back(TabEntry{s.player.name});
        if (!s.tab.empty()) s.tab.front().platform = Platform::Desktop;
        s.world.entities = 0;
        s.world.players = 0;
        uintptr_t ids = pool(reg, "pool.identifier");
        if (!ids) return;
        uintptr_t packed = mem::pointer(ids + 0x20), packedEnd = mem::pointer(ids + 0x28);
        if (!packed || packedEnd <= packed || packedEnd - packed > 4 * 20000) return;
        std::vector<uint32_t> list((packedEnd - packed) / 4);
        if (!mem::readBytes(packed, list.data(), list.size() * 4)) return;
        uintptr_t states = pool(reg, "pool.state"), boxes = pool(reg, "pool.aabb"), render = pool(reg, "pool.render");
        uintptr_t attrPool = pool(reg, "pool.attributes");
        uint64_t now = GetTickCount64();
        std::map<uint32_t, uint64_t> seen;
        for (uint32_t id : list) {
            if ((id >> 18) == 0x3fff) continue;
            uintptr_t def = component(ids, id, off("identifier.size"));
            uintptr_t st = component(states, id, off("state.size"));
            if (!def || !st) continue;
            auto first = firstSeen_.find(id);
            bool fresh = first == firstSeen_.end();
            seen[id] = fresh ? now : first->second;
            if (id == self) continue;
            std::string kind = text(def + off("identifier.name"));
            Vec3 tick{mem::get<float>(st), mem::get<float>(st + 4), mem::get<float>(st + 8)};
            Vec3 motion{mem::get<float>(st + 24), mem::get<float>(st + 28), mem::get<float>(st + 32)};
            Vec3 pos = tick;
            if (uintptr_t r = component(render, id, off("render.size"))) pos = {mem::get<float>(r), mem::get<float>(r + 4), mem::get<float>(r + 8)};
            if (uintptr_t box = component(boxes, id, off("aabb.size"))) pos.y -= tick.y - mem::get<float>(box + 4);
            s.world.entities++;
            bool arrow = kind == "arrow", pearl = kind == "ender_pearl", trident = kind == "thrown_trident";
            if (arrow || pearl || trident) {
                Projectile pr;
                pr.id = id;
                pr.kind = arrow ? 0 : pearl ? 1 : 2;
                pr.pos = pos;
                pr.vel = {motion.x * 20.f, motion.y * 20.f, motion.z * 20.f};
                // the shooter is not linked here; a projectile that shows up right at the eyes was ours
                bool& mine = mine_[id];
                if (fresh) mine = distance(pos, eye_) < 2.5f;
                pr.mine = mine;
                s.shots.push_back(pr);
                continue;
            }
            float max = 0.f, hp = health(component(attrPool, id, off("attributes.size")), &max);
            if (max <= 0.f) continue;
            Other o;
            o.id = id;
            o.kind = kind;
            o.isPlayer = kind == "player";
            o.name = o.isPlayer ? playerName(reg, id) : kind;
            o.pos = pos;
            o.vel = {motion.x * 20.f, motion.y * 20.f, motion.z * 20.f};
            if (o.isPlayer)
                if (uintptr_t r = component(pool(reg, "pool.rotation"), id, off("rotation.size"))) o.yaw = mem::get<float>(r + 4);
            if (o.isPlayer) pose(reg, id, o.pose);
            o.health = hp;
            o.maxHealth = max;
            if (o.isPlayer) {
                s.world.players++;
                if (!o.name.empty()) s.tab.push_back(TabEntry{o.name});
            }
            s.others.push_back(std::move(o));
        }
        readPlatforms(s);
        firstSeen_ = std::move(seen);
        std::erase_if(mine_, [&](auto& kv) { return !firstSeen_.count(kv.first); });
        s.world.players++;
    }

    void readPlatforms(State& s) {
        uintptr_t level = playerPtr_;
        if (!level) return;
        // 1.26.52 live: the LocalPlayer signature stores Level; its +0x4e0 points to an MSVC unordered_map.
        // The list head is +8, size +16; entry name +0x38, skin shared_ptr +0xa0.
        uintptr_t map = mem::pointer(level + 0x4E0);
        if (!map) return;
        auto count = mem::get<uint64_t>(map + 16);
        uintptr_t head = mem::pointer(map + 8), node = head ? mem::pointer(head) : 0;
        if (!count || count > 256 || !node) return;
        std::map<std::string, Platform> devices;
        for (uint64_t i = 0; i < count; ++i) {
            if (!node || node == head) return;
            std::string name = text(node + 0x38);
            int device = mem::get<int>(node + 0x98, -1);
            if (name.empty() || name.size() > 128 || device < -1 || device > 15 || device == 0 || device == 6) return;
            Platform platform = Platform::Unknown;
            if (device == 3 || device == 7 || device == 8 || device == 15) platform = Platform::Desktop;
            else if (device == 1 || device == 2 || device == 4 || device == 14) platform = Platform::Mobile;
            else if (device == 11 || device == 12 || device == 13) platform = Platform::Console;
            if (name == s.player.name && s.time >= nextSkinRead_) {
                nextSkinRead_ = s.time + 1.0;
                uintptr_t skin = mem::pointer(node + 0xA0);
                int w = skin ? mem::get<int>(skin + 0xA4) : 0;
                int h = skin ? mem::get<int>(skin + 0xA8) : 0;
                uintptr_t pixels = skin ? mem::pointer(skin + 0xC0) : 0;
                uint64_t bytes = skin ? mem::get<uint64_t>(skin + 0xC8) : 0;
                if ((w == 64 || w == 128 || w == 256) && (h == w || h * 2 == w) && bytes == uint64_t(w) * h * 4 && pixels) {
                    Skin current;
                    auto model = text(skin + 0x80);
                    auto patch = text(skin + 0x60);
                    current.slim = model.find("slim") != std::string::npos || patch.find("slim") != std::string::npos;
                    current.width = w;
                    current.height = h;
                    current.rgba.resize(size_t(bytes));
                    if (mem::readBytes(pixels, current.rgba.data(), current.rgba.size())) {
                        if (current.rgba != s.skin.rgba || current.slim != s.skin.slim) keepSkin(current);
                        s.skin = std::move(current);
                    }
                }
            }
            devices.emplace(std::move(name), platform);
            node = mem::pointer(node);
        }
        if (node != head || !devices.contains(s.player.name)) return;
        std::vector<TabEntry> entries;
        entries.reserve(devices.size());
        for (const auto& [name, platform] : devices) {
            auto old = std::find_if(s.tab.begin(), s.tab.end(), [&](const TabEntry& e) { return e.name == name; });
            TabEntry entry = old != s.tab.end() ? *old : TabEntry{name};
            entry.platform = platform;
            entries.push_back(std::move(entry));
        }
        s.tab = std::move(entries);
    }

    // MobEffectsComponent is a vector of effect instances indexed by effect id (0x90 bytes each: id, duration in
    // ticks at +4, amplifier at +0x20); unused slots stay zero
    void readEffects(State& s) {
        static const char* names[] = {"", "speed", "slowness", "haste", "mining_fatigue", "strength", "instant_health", "instant_damage",
                                      "jump_boost", "nausea", "regeneration", "resistance", "fire_resistance", "water_breathing",
                                      "invisibility", "blindness", "night_vision", "hunger", "weakness", "poison", "wither",
                                      "health_boost", "absorption", "saturation", "levitation", "fatal_poison", "conduit_power",
                                      "slow_falling", "bad_omen", "village_hero", "darkness", "trial_omen", "wind_charged",
                                      "weaving", "oozing", "infested", "raid_omen"};
        static const uint32_t colors[] = {0, 0x7CAFC6, 0x5A6C81, 0xD9C043, 0x4A4217, 0x932423, 0xF82423, 0x430A09, 0x22FF4C, 0x551D4A,
                                          0xCD5CAB, 0x99453A, 0xE49A3A, 0x2E5299, 0x7F8392, 0x1F1F23, 0x1F1FA1, 0x587653, 0x484D48,
                                          0x4E9331, 0x352A27, 0xF87D23, 0x2552A5, 0xF82423, 0xCEFFFF, 0x4E9331, 0x1DC2D1, 0xFFEFD1,
                                          0x0B6138, 0x44FF44, 0x292721, 0x1BBDB5, 0xBDC9FF, 0x78695A, 0x99FFA3, 0x8C9B8C, 0xDE4058};
        static const bool bad[] = {false, false, true, false, true, false, false, true, false, true, false, false, false, false,
                                   false, true, false, true, true, true, true, false, false, false, true, true, false, false,
                                   true, false, true, true, true, true, true, true, true};
        uintptr_t reg = mem::pointer(playerPtr_ + off("player.registry"));
        uint32_t self = selfId(reg);
        auto& list = s.player.effects;
        list.clear();
        uintptr_t fx = component(pool(reg, "pool.effects"), self, off("effects.size"));
        if (!fx) return;
        uintptr_t begin = mem::pointer(fx), end = mem::pointer(fx + 8);
        int size = off("effect.size");
        if (size <= 0 || !begin || end <= begin || (end - begin) / size > 64) return;
        for (uintptr_t e = begin; e + size <= end; e += size) {
            uint32_t id = mem::get<uint32_t>(e);
            int ticks = mem::get<int>(e + off("effect.duration"));
            if (id == 0 || id >= std::size(names) || ticks == 0 || ticks < -1) continue;
            Effect fxe;
            fxe.id = names[id];
            fxe.amplifier = mem::get<int>(e + off("effect.amplifier"));
            fxe.infinite = ticks == -1;
            fxe.seconds = fxe.infinite ? 0.f : float(ticks) / 20.f;
            // the total is not kept, so the longest time seen since the effect started stands in for it
            float& longest = effectTotal_[id];
            if (fxe.seconds > longest) longest = fxe.seconds;
            fxe.total = fxe.infinite ? 0.f : longest;
            fxe.good = !bad[id];
            fxe.color = colors[id];
            list.push_back(std::move(fxe));
        }
        std::erase_if(effectTotal_, [&](auto& kv) { return std::none_of(list.begin(), list.end(), [&](auto& e) { return e.id == names[kv.first]; }); });
    }


    // The client scoreboard (1.26.52: player+0x490) keeps its display slots in an unordered_map at +0x18 (list head,
    // nodes: next, prev, slot name at +0x10, objective at +0x30, sort order at +0x38). An objective has its scores
    // in an unordered_map whose list head is at +0x20 (nodes: scoreboard id at +0x10, identity at +0x18, score at
    // +0x20), its name at +0x58 and its display name at +0x78. A fake player identity holds its name at +0x28.
    template <class Fn>
    static void eachNode(uintptr_t head, Fn&& fn) {
        if (!head) return;
        uintptr_t n = mem::pointer(head);
        for (int k = 0; n && n != head && k < 256; k++, n = mem::pointer(n)) fn(n);
    }

    void readScoreboard(State& s) {
        auto& board = s.scoreboard;
        board.title.clear();
        board.lines.clear();
        uintptr_t sb = follow(playerPtr_, "scoreboard");
        if (!sb) return;
        uintptr_t objective = 0;
        bool ascending = false;
        eachNode(mem::pointer(sb + off("scoreboard.slots")), [&](uintptr_t n) {
            if (objective || text(n + 0x10) != "sidebar") return;
            objective = mem::pointer(n + 0x30);
            ascending = mem::get<uint8_t>(n + 0x38) == 0;
        });
        if (!objective) return;
        board.title = text(objective + off("objective.title"));
        eachNode(mem::pointer(objective + off("objective.scores")), [&](uintptr_t n) {
            uintptr_t id = mem::pointer(n + 0x18);
            std::string name = id ? text(id + off("identity.name")) : std::string();
            if (!name.empty()) board.lines.push_back({std::move(name), mem::get<int>(n + 0x20)});
        });
        std::stable_sort(board.lines.begin(), board.lines.end(), [&](auto& a, auto& b) { return ascending ? a.second < b.second : a.second > b.second; });
    }

    // Freelook stops the system that turns the player to match the camera. The native way skips the camera's
    // player-update function (Flarial's hook). Without its signatures the camera entities are unlinked instead:
    // the system finds cameras through an entt view, which compares the version bits of the
    // UpdatePlayerFromCameraComponent pool's sparse entry with the entity, so flipping one version bit makes the
    // view skip the camera while plain lookups (position bits only) still work. Either way the cameras get their
    // old angles back (CameraDirectLookComponent: yaw, pitch in radians) before the link returns, otherwise the
    // player would snap to where the camera looked.
public:
    bool detach(bool on, bool moveHead = false) {
        uintptr_t reg = playerPtr_ ? mem::pointer(playerPtr_ + off("player.registry")) : 0;
        if (detached_.reg && detached_.reg != reg) {
            freecam::set(false);
            detached_ = {};
        }
        if (!on) {
            if (!detached_.reg) return freecam::set(false);
            for (auto& [at, angles] : detached_.angles) mem::write(at, angles);
            for (auto& [at, entry] : detached_.entries) mem::write(at, entry);
            freecam::set(false);
            detached_ = {};
            return false;
        }
        if (!reg) return freecam::set(false);
        uintptr_t look = pool(reg, "pool.cameraLook");
        if (!detached_.reg) {
            eachEntity(look, [&](uint32_t id) {
                if (uintptr_t c = component(look, id, off("cameraLook.size"))) detached_.angles.push_back({c, mem::get<uint64_t>(c)});
            });
            if (detached_.angles.empty()) return false;
            detached_.reg = reg;
        }
        if (!moveHead) {
            uintptr_t head = component(pool(reg, "pool.headRotation"), selfId(reg), 8);
            if (head) {
                if (!detached_.head) { detached_.head = head; detached_.headAngles = mem::get<uint64_t>(head); }
                if (head == detached_.head) mem::write(head, detached_.headAngles);
            }
        } else detached_.head = 0;
        if (detached_.entries.empty() && freecam::set(true)) return true;
        return unlinkCameras(reg);
    }

private:
    bool unlinkCameras(uintptr_t reg) {
        uintptr_t link = pool(reg, "pool.cameraLink");
        if (!link) return false;
        eachEntity(link, [&](uint32_t id) {
            uintptr_t at = sparseEntry(link, id);
            if (!at) return;
            uint32_t entry = mem::get<uint32_t>(at);
            auto known = std::find_if(detached_.entries.begin(), detached_.entries.end(), [&](auto& e) { return e.first == at; });
            if (known == detached_.entries.end()) detached_.entries.push_back({at, entry});
            else if (entry == (known->second ^ (1u << 18))) return;
            else known->second = entry;
            mem::write(at, entry ^ (1u << 18));
        });
        return !detached_.entries.empty();
    }

public:
    // Hiding an entity sets its invisible status flag (ActorDataFlagComponent, a bitset; bit 5 is INVISIBLE) on
    // this client only, every frame until the time is up, then puts the old bit back.
    // The registry the crosshair's entity ids belong to. On a server it is not the one reached through the player
    // pointer.
    uintptr_t hideRegistry() {
        uintptr_t own = mem::pointer(playerPtr_ + off("player.registry"));
        return entityReg_ && pool(entityReg_, "pool.flags") ? entityReg_ : own;
    }

    void hide(uint32_t id, uint64_t ms) {
        auto& h = hidden_[id];
        h.reg = hideRegistry();
        h.until = std::max(h.until, GetTickCount64() + ms);
        applyHidden();
    }

    void unhide(uint32_t id) {
        auto it = hidden_.find(id);
        if (it == hidden_.end()) return;
        it->second.until = 0;
        applyHidden();
    }

    int hiddenCount() const { return int(hidden_.size()); }

private:
    struct Hidden {
        uintptr_t reg = 0;
        uint64_t until = 0;
        int was = -1;
    };
    std::map<uint32_t, Hidden> hidden_;
    bool hideLogged_ = false, hideFailLogged_ = false;
    bool chatHidden_ = false;
    int chatSweeps_ = 0;
    uintptr_t entityReg_ = 0;

    void applyHidden() {
        if (hidden_.empty()) return;
        uintptr_t reg = hideRegistry();
        uintptr_t flags = pool(reg, "pool.flags");
        int bit = off("flags.invisible");
        if (!flags || bit < 0 || bit >= off("flags.size") * 8) {
            if (!hideFailLogged_) logger::warn("live: entity hiding not possible, the flags of the entities were not found (registry {:#x})", reg);
            hideFailLogged_ = true;
            hidden_.clear();
            return;
        }
        uint64_t now = GetTickCount64();
        for (auto it = hidden_.begin(); it != hidden_.end();) {
            if (it->second.reg != reg) {
                it = hidden_.erase(it);
                continue;
            }
            uintptr_t c = component(flags, it->first, off("flags.size"));
            if (!c) {
                it = hidden_.erase(it);
                continue;
            }
            uintptr_t at = c + uintptr_t(bit / 8);
            uint8_t v = mem::get<uint8_t>(at), mask = uint8_t(1u << (bit % 8));
            if (it->second.was < 0) it->second.was = (v & mask) != 0;
            bool done = now >= it->second.until;
            uint8_t want = hiddenFlag::value(v, mask, it->second.was != 0, done);
            if (want != v && mem::write(at, want) && !hideLogged_) {
                hideLogged_ = true;
                logger::info("live: local entity hiding applied to entity {}, invisible bit {}", it->first, bit);
            }
            it = done ? hidden_.erase(it) : std::next(it);
        }
    }

    struct Detached {
        uintptr_t reg = 0;
        uintptr_t head = 0;
        uint64_t headAngles = 0;
        std::vector<std::pair<uintptr_t, uint32_t>> entries;
        std::vector<std::pair<uintptr_t, uint64_t>> angles;
    };
    Detached detached_;

    static uintptr_t sparseEntry(uintptr_t p, uint32_t id) {
        uint32_t idx = id & 0x3ffff;
        uintptr_t sparse = mem::pointer(p + 8), sparseEnd = mem::pointer(p + 0x10);
        if (!sparse || (idx / 4096) * 8 >= sparseEnd - sparse) return 0;
        uintptr_t page = mem::pointer(sparse + (idx / 4096) * 8);
        return page ? page + (idx % 4096) * 4 : 0;
    }

    template <class Fn>
    static void eachEntity(uintptr_t p, Fn&& fn) {
        if (!p) return;
        uintptr_t packed = mem::pointer(p + 0x20), end = mem::pointer(p + 0x28);
        for (uintptr_t a = packed; a && a < end && a < packed + 4 * 256; a += 4)
            if (uint32_t id = mem::get<uint32_t>(a, 0xffffffff); (id >> 18) != 0x3fff) fn(id);
    }

    // a hit counts as landed when the struck entity loses health shortly after; down to zero is a kill
    void followHits(uintptr_t reg, std::vector<Event>& ev) {
        if (!hitId_ || !reg) return;
        if (GetTickCount64() - hitAt_ > 1500) {
            hitId_ = 0;
            return;
        }
        uintptr_t attrs = component(pool(reg, "pool.attributes"), hitId_, off("attributes.size"));
        uintptr_t first = attrs ? mem::pointer(attrs + off("attributes.list")) : 0;
        uintptr_t hp = first ? attrIn(first, "health") : 0;
        float now = hp ? mem::get<float>(hp + off("attr.value")) : 0.f;
        if (!hp || now >= hitHealth_ - 0.01f) {
            // Many servers (Zeqa) do not send the health of other players. The hurt animation is sent everywhere:
            // the struck entity's hurt timer jumps to its start.
            int at = off("actor.hurtTime");
            uintptr_t owner = at > 0 ? component(pool(reg, "pool.owner"), hitId_, 8) : 0;
            uintptr_t actor = owner ? mem::pointer(owner) : 0;
            if (!actor) return;
            int hurt = mem::get<int16_t>(actor + uintptr_t(at));
            if (hurt < 0 || hurt > 10) return;
            bool fresh = hitHurt_ != 99 && hurt >= 8 && hurt > hitHurt_;
            hitHurt_ = hitHurt_ == 99 ? hurt : std::min(hitHurt_, hurt);
            if (!fresh) return;
            Event c{EventKind::Confirm};
            c.value = float(GetTickCount64() - hitAt_);
            c.reach = hitReach_;
            c.actor = hitId_;
            c.text = hitName_;
            ev.push_back(c);
            hitId_ = 0;
            return;
        }
        Event c{EventKind::Confirm};
        c.value = float(GetTickCount64() - hitAt_);
        c.damage = hitHealth_ - now;
        c.reach = hitReach_;
        c.actor = hitId_;
        c.text = hitName_;
        ev.push_back(c);
        if (now <= 0.f) {
            Event k{EventKind::Kill};
            k.actor = hitId_;
            k.text = hitName_;
            ev.push_back(std::move(k));
        }
        hitId_ = 0;
    }

    // ItemStack (1.26.52, 0x98 bytes): +0x8 weak pointer to the Item, +0x10 CompoundTag user data, +0x20 aux,
    // +0x22 count. Item: +0x128 full name ("minecraft:arrow"), +0x150 max damage.
    static std::string text(uintptr_t at) {
        size_t len = mem::get<size_t>(at + 16), cap = mem::get<size_t>(at + 24);
        if (len == 0 || len > 4096 || cap < len || (cap <= 15 && len > 15)) return {};
        uintptr_t p = cap > 15 ? mem::pointer(at) : at;
        std::string out(len, '\0');
        return p && mem::readBytes(p, out.data(), len) ? out : std::string{};
    }

    // user data is a CompoundTag: a std::map<std::string, tag> (node: left, parent, right, color, isnil, key at
    // +0x20, tag at +0x40 with its payload 8 bytes in)
    static uintptr_t findTag(uintptr_t tag, const char* key) {
        uintptr_t head = mem::pointer(tag + 8);
        if (!head) return 0;
        uintptr_t stack[32];
        int n = 0, seen = 0;
        if (uintptr_t root = mem::pointer(head + 8); root && root != head) stack[n++] = root;
        while (n > 0 && seen++ < 64) {
            uintptr_t node = stack[--n];
            if (mem::get<uint8_t>(node + 0x19, 1)) continue;
            if (text(node + 0x20) == key) return node + 0x40;
            for (int side : {0, 16}) {
                uintptr_t c = mem::pointer(node + side);
                if (c && c != head && n < 32) stack[n++] = c;
            }
        }
        return 0;
    }

    static Item item(uintptr_t stack) {
        Item it;
        int count = mem::get<uint8_t>(stack + off("stack.count"));
        uintptr_t def = mem::pointer(mem::pointer(stack + off("stack.item")));
        if (!count || !def) return it;
        it.name = text(def + off("item.name"));
        if (it.name.starts_with("minecraft:")) it.name.erase(0, 10);
        it.count = count;
        it.aux = mem::get<int16_t>(stack + off("stack.aux"));
        it.maxDamage = mem::get<int16_t>(def + off("item.maxDamage"));
        if (uintptr_t tag = mem::pointer(stack + off("stack.tag"))) {
            if (uintptr_t d = findTag(tag, "Damage")) it.damage = mem::get<int>(d + 8);
            it.enchanted = findTag(tag, "ench") != 0;
        }
        return it;
    }

    // The HUD keeps the received chat lines in a std::vector of GuiMessage (1.26.52: 0x110 bytes, finished line
    // with sender as a std::string at +0x90). New lines are the ones after the last line seen; the vector is
    // trimmed from the front, so the last seen line is searched from the end.
    // Lines the game already shows when Better Chat takes over would stay on the game's hud until their time ran out,
    // next to the same lines in Better Chat. When the hud's chat is switched off their time is taken away at once; the
    // chat screen still lists them.
    void sweepChatHud() {
        bool hidden = game::chatHudHidden();
        if (hidden && !chatHidden_) chatSweeps_ = 20;
        chatHidden_ = hidden;
        if (!hidden || chatSweeps_ <= 0) return;
        chatSweeps_--;
        int life = off("chat.entryLife"), size = off("chat.size");
        if (life <= 0 || size <= 0) return;
        uintptr_t vec = follow(playerPtr_, "chat") + off("chat.lines");
        uintptr_t begin = mem::pointer(vec), end = mem::pointer(vec + 8);
        if (!begin || end < begin || (end - begin) % size || (end - begin) / size > 1000) return;
        for (uintptr_t e = begin; e < end; e += size)
            if (mem::get<float>(e + uintptr_t(life)) > 0.f) mem::write<float>(e + uintptr_t(life), 0.f);
    }

    void readChat(std::vector<Event>& ev) {
        // once the hook has seen a line it is the only source, or every line would arrive twice
        if (chatFromHook_) return;
        uintptr_t vec = follow(playerPtr_, "chat") + off("chat.lines");
        uintptr_t begin = mem::pointer(vec), end = mem::pointer(vec + 8);
        int size = off("chat.size");
        // an empty list has no storage yet; it still counts as seen, or the first line would only prime the reader
        if (size <= 0 || end < begin || (end - begin) % size || (end - begin) / size > 1000 || (!begin && !mem::readable(vec, 16))) return;
        size_t count = (end - begin) / size;
        auto line = [&](size_t k) { return text(begin + k * size + off("chat.text")); };
        if (!chatPrimed_) {
            chatPrimed_ = true;
            chatLast_ = count ? line(count - 1) : std::string();
            chatCount_ = count;
            return;
        }
        if (count == 0) {
            chatLast_.clear();
            chatCount_ = 0;
            return;
        }
        if (count == chatCount_ && line(count - 1) == chatLast_) return;
        size_t from = 0;
        if (!chatLast_.empty())
            for (size_t k = count; k-- > 0;)
                if (line(k) == chatLast_) {
                    from = k + 1;
                    break;
                }
        for (size_t k = from; k < count; k++) {
            Event e{EventKind::Chat};
            e.text = line(k);
            if (!e.text.empty()) ev.push_back(std::move(e));
        }
        chatLast_ = line(count - 1);
        chatCount_ = count;
    }

    void readStats(State& s) {
        uintptr_t array = attrArray;
        if (off("player.registry") >= 0) {
            uintptr_t reg = mem::pointer(playerPtr_ + off("player.registry"));
            uintptr_t attrs = component(pool(reg, "pool.attributes"), selfId(reg), off("attributes.size"));
            if (uintptr_t first = attrs ? mem::pointer(attrs + off("attributes.list")) : 0; validAttrs(first)) {
                array = first;
                attrArray = first;
            }
        }
        auto& pl = s.player;
        pl.statsKnown = validAttrs(array);
        if (!pl.statsKnown) return;
        auto value = [&](const char* name, float fallback) {
            uintptr_t e = attrIn(array, name);
            return e ? mem::get<float>(e + off("attr.value"), fallback) : fallback;
        };
        if (uintptr_t h = attrIn(array, "health")) {
            pl.health = mem::get<float>(h + off("attr.value"), pl.health);
            pl.maxHealth = mem::get<float>(h + off("attr.max"), pl.maxHealth);
        }
        pl.absorption = value("absorption", pl.absorption);
        pl.hunger = value("hunger", pl.hunger);
        pl.saturation = value("saturation", pl.saturation);
        pl.level = int(value("level", float(pl.level)));
        pl.xp = value("experience", pl.xp);
    }

    // Using an item is the use button held while the hand holds something usable: a bow draws for one second to
    // full power, a thrown pearl or potion shows as the held stack shrinking right after a press.
    void readUse(State& s, std::vector<Event>& ev) {
        auto& pl = s.player;
        const Item& held = pl.held();
        int screen = flarialModules::screen();
        bool playing = screen ? screen == 1 : input::grabbed();
        bool press = input::down(VK_RBUTTON) && playing && !ui::wantsCursor();
        double now = ui::time();
        bool drawable = held.name == "bow" || held.name == "crossbow" || held.name == "trident";
        bool usable = drawable || held.name == "shield" || held.name.find("potion") != std::string::npos ||
                      held.name.starts_with("cooked_") || held.name.find("apple") != std::string::npos || held.name == "bread";
        if (press && (!usePressed_ || (!held.name.empty() && held.name != pressItem_))) {
            useStart_ = now;
            pressItem_ = held.name;
            pressCount_ = held.count;
            pressSlot_ = pl.slot;
        }
        if (!press && usePressed_ && pressItem_ == "bow" && now - useStart_ > 0.1) {
            Event e{EventKind::BowRelease};
            e.value = float(std::min(1.0, (now - useStart_) / 1.0));
            e.item = "bow";
            ev.push_back(std::move(e));
        }
        usePressed_ = press;
        pl.usingItem = press && usable && held.name == pressItem_;
        pl.useProgress = pl.usingItem ? float(std::min(1.0, (now - useStart_) / (drawable ? 1.0 : 1.6))) : 0.f;
        // a press that made the held stack shrink used one up
        if (!pressItem_.empty() && now - useStart_ < 0.6 && pl.slot == pressSlot_ &&
            (held.name == pressItem_ || held.name.empty()) && held.count < pressCount_) {
            Event e{EventKind::ItemUse};
            e.item = pressItem_;
            ev.push_back(std::move(e));
            pressCount_ = held.count;
        }
    }

    // a totem used up shows as one fewer totem in the hands while health is low
    void countTotems(const Player& pl, std::vector<Event>& ev) {
        int n = 0;
        for (const Item* it : {&pl.offhand, &pl.held()})
            if (it->name == "totem_of_undying") n += it->count;
        if (totems_ >= 0 && n < totems_ && pl.health <= 8.f) ev.push_back(Event{EventKind::TotemPop});
        totems_ = n;
    }

    // Of several inventory objects only one is the player's: it holds the items and its copy of the selected stack matches
    // one hotbar slot. The pick by reference count alone can land on a stale or preview copy with an empty hotbar.
    static int handScore(uintptr_t h) {
        if (!validHand(h)) return -1;
        int size = off("stack.size"), filled = 0, score = 0;
        uintptr_t items = mem::pointer(h + off("hand.items"));
        for (int k = 0; k < 36; k++)
            if (!item(items + uintptr_t(k) * size).empty()) filled++;
        uintptr_t heldItem = mem::pointer(h + off("hand.held") + off("stack.item"));
        if (heldItem)
            for (int k = 0; k < 9; k++)
                if (mem::pointer(items + uintptr_t(k) * size + off("stack.item")) == heldItem) score = 50;
        return filled + score;
    }

    static int armorScore(uintptr_t a) {
        if (!validArmor(a)) return -1;
        int size = off("stack.size"), filled = 0;
        uintptr_t items = mem::pointer(a + off("armor.items"));
        for (int k = 0; k < 4; k++)
            if (!item(items + uintptr_t(k) * size).empty()) filled++;
        return filled;
    }

    void chooseInventory(uint64_t now) {
        if (now - chosenAt_ < 1000) return;
        chosenAt_ = now;
        std::vector<uintptr_t> hands, armors;
        {
            std::scoped_lock g(candLock);
            hands = handCands;
            armors = armorCands;
        }
        auto best = [](std::vector<uintptr_t>& cands, std::atomic<uintptr_t>& current, int (*score)(uintptr_t)) {
            if (cands.size() < 2) return false;
            uintptr_t pick = current;
            int top = pick ? score(pick) : -1;
            for (uintptr_t c : cands) {
                int sc = score(c);
                if (sc > top) pick = c, top = sc;
            }
            if (pick == current) return false;
            current = pick;
            return true;
        };
        if (best(hands, handObj, handScore)) logger::info("live: switched to the inventory that holds the player's items");
        if (best(armors, armorObj, armorScore)) logger::info("live: switched to the armor that holds the player's pieces");
    }

    // Our own entity's ActorDataFlagComponent: one bit per actor flag, numbered like the game's ActorFlags (read live on
    // 1.26.52.3: a standing player has 19 can climb, 35 breathing, 48 collision and 49 gravity set). Auto Perspective
    // needs the three states below, which nothing else tells.
    void readSelfFlags(Player& pl) {
        int glide = off("flags.gliding"), swim = off("flags.swimming"), emote = off("flags.emoting");
        if (glide < 0 && swim < 0 && emote < 0) return;
        uintptr_t reg = off("player.registry") >= 0 ? mem::pointer(playerPtr_ + off("player.registry")) : 0;
        uintptr_t flags = reg ? component(pool(reg, "pool.flags"), selfId(reg), off("flags.size")) : 0;
        if (!flags) {
            pl.gliding = pl.swimming = pl.emoting = false;
            return;
        }
        uint64_t bits[2] = {mem::get<uint64_t>(flags), mem::get<uint64_t>(flags + 8)};
        auto set = [&](int n) { return n >= 0 && n < 128 && ((bits[n / 64] >> (n % 64)) & 1) != 0; };
        pl.gliding = set(glide);
        pl.swimming = set(swim);
        pl.emoting = set(emote);
        int state = (pl.gliding ? 1 : 0) | (pl.swimming ? 2 : 0) | (pl.emoting ? 4 : 0);
        if (state && !(flagsLogged_ & state)) {
            flagsLogged_ |= state;
            logger::info("live: player state from the actor flags: {}{}{}", pl.gliding ? "gliding " : "", pl.swimming ? "swimming " : "", pl.emoting ? "emoting" : "");
        }
    }

    // The object the "LocalPlayer" signature leads to is the level (read live on 1.26.52.3: the player actor's level
    // pointer at +0x1d8 is that object). The actor itself is what the owner component of our own entity points at;
    // it is looked up again twice a second, a new world brings a new one.
    uintptr_t actor() override { return selfActor(); }

    uintptr_t selfActor() {
        uint64_t now = GetTickCount64();
        if (actorFor_ == playerPtr_ && now - actorAt_ < 500) return actor_;
        actorFor_ = playerPtr_;
        actorAt_ = now;
        uintptr_t reg = playerPtr_ && off("player.registry") >= 0 ? mem::pointer(playerPtr_ + off("player.registry")) : 0;
        uintptr_t owner = reg ? component(pool(reg, "pool.owner"), selfId(reg), 8) : 0;
        actor_ = owner ? mem::pointer(owner) : 0;
        return actor_;
    }

    // 1.26.52: the player actor keeps its PlayerInventory at +0x5b8 (static, LocalPlayer constructor 0x475a388), and
    // that holds the selected hotbar slot as an int at +0x10 (static, more than forty readers, 0x1f8de2 for one; read
    // live: 3 with the fourth slot selected, the container id next to it 0)
    bool readSelectedSlot(Player& pl) {
        int at = off("player.inventory"), field = off("inventory.selected");
        uintptr_t actor = at > 0 && field >= 0 ? selfActor() : 0;
        if (!actor) return false;
        uintptr_t inventory = mem::pointer(actor + uintptr_t(at));
        int slot = inventory ? mem::get<int>(inventory + uintptr_t(field), -1) : -1;
        if (slot < 0 || slot > 8) return false;
        if (slot != pl.slot && !slotLogged_) {
            slotLogged_ = true;
            logger::info("live: the selected hotbar slot follows the player's inventory (now {})", slot + 1);
        }
        pl.slot = slot;
        pl.handEmpty = pl.hotbar[size_t(slot)].empty();
        return true;
    }

    // 1.26.52: the hand object's own offhand field stays empty. The offhand item is slot 1 of the offhand container in
    // our entity's ActorEquipmentComponent (16 bytes: offhand container, armor container; items vector at +0x1a0),
    // read live with a totem in the offhand and netherite armor in slots 0-3 of the second container.
    void readOffhand(Player& pl) {
        int slot = off("equipment.offhandSlot"), items = off("container.items");
        if (slot < 0 || items < 0 || !playerPtr_ || off("player.registry") < 0) return;
        uintptr_t reg = mem::pointer(playerPtr_ + off("player.registry"));
        uintptr_t eq = reg ? component(pool(reg, "pool.equipment"), selfId(reg), 16) : 0;
        uintptr_t box = eq ? mem::pointer(eq) : 0;
        uintptr_t first = box ? mem::pointer(box + uintptr_t(items)) : 0;
        if (first) pl.offhand = item(first + uintptr_t(slot) * uintptr_t(off("stack.size")));
    }

    // 1.26.52: the PlayerInventory at actor +0x5b8 owns the player's 36-slot Inventory at +0xb8, its stacks in the
    // vector at +0x198 (read live: hotbar first, then the 27 main slots). This is the container the server writes to;
    // the hand object found by the heap sweep can be a stale copy there, which left only the offhand readable.
    bool readOwnItems(Player& pl) {
        int at = off("player.inventory"), box = off("inventory.container"), vec = off("inventory.items");
        uintptr_t actor = at > 0 && box >= 0 && vec >= 0 ? selfActor() : 0;
        uintptr_t inventory = actor ? mem::pointer(actor + uintptr_t(at)) : 0;
        uintptr_t container = inventory ? mem::pointer(inventory + uintptr_t(box)) : 0;
        if (!container) return false;
        uintptr_t first = mem::pointer(container + uintptr_t(vec)), last = mem::pointer(container + uintptr_t(vec) + 8);
        uintptr_t size = uintptr_t(off("stack.size"));
        if (!first || last != first + 36 * size || mem::pointer(first) != imageBase() + uintptr_t(off("stack.vtable"))) return false;
        for (int k = 0; k < 9; k++) pl.hotbar[size_t(k)] = item(first + uintptr_t(k) * size);
        pl.main.resize(27);
        for (int k = 0; k < 27; k++) pl.main[size_t(k)] = item(first + uintptr_t(k + 9) * size);
        if (!ownItemsLogged_) {
            ownItemsLogged_ = true;
            logger::info("live: hotbar and inventory follow the player's own container");
        }
        return true;
    }

    void readInventory(State& s, std::vector<Event>& ev) {
        auto& pl = s.player;
        // ten times a second is plenty for counters and armor, and keeps the reads off every frame
        uint64_t now = GetTickCount64();
        auto& player = s.player;
        if (inventoryOwner_ != playerPtr_) {
            inventoryOwner_ = playerPtr_;
            selectedSlotKnown_ = false;
        }
        float rect[4];
        if (ImGui::GetCurrentContext() && flarialModules::hotbar(rect)) {
            if (auto slot = hotbarSlot(ImGui::GetIO().DisplaySize.x, rect[0], rect[2])) {
                player.slot = *slot;
                selectedSlotKnown_ = true;
            }
        }
        if (selectedSlotKnown_) player.handEmpty = player.hotbar[size_t(player.slot)].empty();
        // the inventory's own field is exact and changes with every scroll, so it is read every frame and wins over
        // the slot taken from where the game draws its selection frame
        bool slotKnown = readSelectedSlot(pl) || selectedSlotKnown_;
        if (now - inventoryAt_ < 100) return;
        inventoryAt_ = now;
        int size = off("stack.size");
        chooseInventory(now);
        uintptr_t hand = handObj, armor = armorObj;
        bool lost = !validHand(hand) || !validArmor(armor) || !pl.statsKnown;
        if (lost && !sweeping && (!sweptAt_ || now - sweptAt_ > 30000)) {
            sweptAt_ = now;
            sweeping = true;
            bg::run(sweep);
        }
        if (validHand(hand)) {
            uintptr_t items = mem::pointer(hand + off("hand.items"));
            for (int k = 0; k < 9; k++) pl.hotbar[size_t(k)] = item(items + uintptr_t(k) * size);
            pl.main.resize(27);
            for (int k = 0; k < 27; k++) pl.main[size_t(k)] = item(items + uintptr_t(k + 9) * size);
            pl.offhand = item(hand + off("hand.offhand"));
            readOffhand(pl);
            readOwnItems(pl);
            if (slotKnown) pl.handEmpty = pl.hotbar[size_t(pl.slot) % pl.hotbar.size()].empty();
            // without the inventory's own slot field: right after the offhand sits a copy of a stack, and the slot is
            // taken to be the hotbar stack it copies (does not follow the scroll wheel on 1.26.52)
            uintptr_t held = hand + off("hand.held");
            uintptr_t heldItem = mem::pointer(held + off("stack.item"));
            int heldCount = mem::get<uint8_t>(held + off("stack.count"));
            if (!slotKnown) pl.handEmpty = heldItem == 0;
            int first = -1, matches = 0;
            bool keeps = false;
            for (int k = 0; k < 9 && heldItem && !slotKnown; k++) {
                uintptr_t st = items + uintptr_t(k) * size;
                if (mem::pointer(st + off("stack.item")) != heldItem || mem::get<uint8_t>(st + off("stack.count")) != heldCount) continue;
                if (first < 0) first = k;
                matches++;
                keeps |= k == pl.slot;
            }
            // two equal stacks cannot be told apart by the copy: stay on the slot that was selected
            if (!slotKnown && first >= 0 && !(matches > 1 && keeps)) pl.slot = first;
            countTotems(pl, ev);
        } else if (readOwnItems(pl)) {
            readOffhand(pl);
            if (slotKnown) pl.handEmpty = pl.hotbar[size_t(pl.slot) % pl.hotbar.size()].empty();
            countTotems(pl, ev);
        }
        if (validArmor(armor)) {
            uintptr_t items = mem::pointer(armor + off("armor.items"));
            for (int k = 0; k < 4; k++) pl.armor[size_t(k)] = item(items + uintptr_t(k) * size);
        }
    }

    void readWorld(State& s) {
        s.world.name.clear();
        s.world.id.clear();
        if (off("dimension.idGetter") > 0) s.player.dimension = -1;
        // 1.26.52: the player keeps a pointer to its level, which holds the time of day as an int
        uintptr_t lv = off("level.via0") >= 0 ? (playerPtr_ ? follow(playerPtr_, "level") : 0) : mem::pointer(sigs::address("Level"));
        if (!lv) return;
        if (off("level.name") >= 0) s.world.name = text(lv + off("level.name"));
        if (playerPtr_ && off("level.folder") >= 0) s.world.id = text(playerPtr_ + off("level.folder"));
        uintptr_t reg = playerPtr_ ? mem::pointer(playerPtr_ + off("player.registry")) : 0;
        uintptr_t owner = reg ? component(pool(reg, "pool.owner"), selfId(reg), 8) : 0;
        auto dimension = dimensionRead::id(owner ? mem::pointer(owner) : 0, imageBase(),
            off("actor.dimension"), off("dimension.idGetter"), off("dimension.id"));
        if (dimension) s.player.dimension = *dimension;
        if (dimension && !s.world.name.empty() && !worldIdentityLogged_) {
            worldIdentityLogged_ = true;
            logger::info("live: world identity bound, dimension {}, world {}", *dimension, s.world.name);
        }
        s.world.time = int(((mem::get<int>(lv + off("level.time"), s.world.time) % 24000) + 24000) % 24000);
        s.world.day = mem::get<int>(lv + off("level.time")) / 24000 + 1;
        s.world.raining = f(lv, "level.rain", 0.f) > 0.05f;
        s.world.thundering = f(lv, "level.thunder", 0.f) > 0.05f;
    }

    // without a velocity field the speed comes from how far the position moved, smoothed over a few frames
    void deriveVelocity(Player& pl) {
        LARGE_INTEGER now, freq;
        QueryPerformanceCounter(&now);
        QueryPerformanceFrequency(&freq);
        double dt = lastPosQpc_ ? double(now.QuadPart - lastPosQpc_) / double(freq.QuadPart) : 0.0;
        if (dt > 0.0005 && dt < 0.5) {
            Vec3 v{float((pl.pos.x - lastPos_.x) / dt), float((pl.pos.y - lastPos_.y) / dt), float((pl.pos.z - lastPos_.z) / dt)};
            // a teleport or respawn is not movement
            if (std::fabs(v.x) > 200.f || std::fabs(v.y) > 200.f || std::fabs(v.z) > 200.f) v = {};
            float k = std::clamp(float(dt) * 12.f, 0.f, 1.f);
            vel_ = {vel_.x + (v.x - vel_.x) * k, vel_.y + (v.y - vel_.y) * k, vel_.z + (v.z - vel_.z) * k};
        }
        lastPos_ = pl.pos;
        lastPosQpc_ = now.QuadPart;
        pl.vel = vel_;
    }

    float reachTo(uintptr_t actor) const {
        int a = off("actor.aabbMinX");
        if (a >= 0) {
            float mn[3], mx[3];
            for (int k = 0; k < 3; k++) {
                mn[k] = mem::get<float>(actor + a + 4 * k);
                mx[k] = mem::get<float>(actor + a + 12 + 4 * k);
            }
            float e[3] = {eye_.x, eye_.y, eye_.z}, d2 = 0.f;
            for (int k = 0; k < 3; k++) {
                float c = std::clamp(e[k], mn[k], mx[k]);
                d2 += (c - e[k]) * (c - e[k]);
            }
            return std::sqrt(d2);
        }
        int px = off("actor.posX");
        if (px < 0) return 0.f;
        Vec3 pos{mem::get<float>(actor + px), mem::get<float>(actor + px + 4), mem::get<float>(actor + px + 8)};
        return distance(eye_, pos);
    }

    std::mutex lock_;
    std::vector<Event> pending_;
    double nextSkinRead_ = 0.0;
    uintptr_t playerPtr_ = 0;
    bool slotLogged_ = false;
    bool ownItemsLogged_ = false;
    int nativeLogged_ = 0;
    int flagsLogged_ = 0;
    uintptr_t actor_ = 0, actorFor_ = 0;
    uint64_t actorAt_ = 0;
    bool blockNamesLogged_ = false;
    uintptr_t blockPlayer_ = 0;
    double blockSince_ = 0.0, blockAsked_ = 0.0;
    std::array<int, 3> blockAt_{};
    std::string blockName_;
    bool worldIdentityLogged_ = false;
    Vec3 eye_;
    Vec3 lastPos_;
    Vec3 vel_;
    int64_t lastPosQpc_ = 0;
    bool wasPressed_ = false;
    uintptr_t inventoryOwner_ = 0;
    bool selectedSlotKnown_ = false;
    uint64_t inventoryAt_ = 0;
    uint64_t chosenAt_ = 0;
    std::map<std::string, std::pair<uintptr_t, uintptr_t>> pools_;
    uint32_t hitId_ = 0;
    uint64_t hitAt_ = 0;
    float hitHealth_ = 0.f;
    // the struck entity's hurt timer as last seen, 99 until it has been read once after the hit
    int hitHurt_ = 99;
    float hitReach_ = 0.f;
    std::map<uint32_t, uint64_t> firstSeen_;
    std::map<uint32_t, bool> mine_;
    std::map<uint32_t, float> effectTotal_;
    std::string hitName_;
    uint64_t sweptAt_ = 0;
    int totems_ = -1;
    bool usePressed_ = false;
    double useStart_ = 0.0;
    std::string pressItem_;
    int pressCount_ = 0;
    int pressSlot_ = 0;
    std::string chatLast_;
    size_t chatCount_ = 0;
    bool chatPrimed_ = false;
    float fallSpeed_ = 0.f;
    bool onGround_ = true;
    bool sprinting_ = false;
    bool wantAttack_ = false;
    std::atomic<bool> wantChat_{false}, chatFromHook_{false}, chatLogged_{false};
    bool chatHooked_ = false;
    bool onlineSincePlay_ = false;
    uint64_t offlineSince_ = 0;
    bool hooked_ = false;
};

}

std::unique_ptr<Provider> makeLive() { return std::make_unique<Live>(); }

bool freeCamera(bool on, bool moveHead) { return self && self->detach(on, moveHead); }

void hide(uintptr_t entity, float seconds) {
    if (self && entity) self->hide(uint32_t(entity), uint64_t(std::max(0.f, seconds) * 1000.f));
}

int hidden() { return self ? self->hiddenCount() : 0; }

void unhide(uintptr_t entity) {
    if (self && entity) self->unhide(uint32_t(entity));
}

}
