#pragma once

#include "gui/Notify.hpp"
#include "hook/GameInput.hpp"
#include "hook/Input.hpp"
#include "modules/Module.hpp"
#include "modules/common/Needs.hpp"
#include "modules/common/Options.hpp"
#include "modules/common/Text.hpp"
#include "render/Ui.hpp"
#include "sdk/Game.hpp"
#include "sdk/Inject.hpp"
#include "InventoryKeys.hpp"

#include <windows.h>

#include <algorithm>
#include <bitset>
#include <atomic>
#include <chrono>
#include <set>

class InventoryLock : public Module {
public:
    InventoryLock()
        : Module("Inventory Lock", "Keeps tools and other valuables from being dropped by accident: dropping needs a quick double press of the drop key.",
                 Category::Comfort, {"input"}) {
        sub("Inventory");
        require(need::inventory, need::sigs({"LocalPlayer"}));
        dropKey_.visible = [this] { return !gameKey_.b || !gameDrop_; };
    }

    void onEnable() override { mcopt::refresh(); readDropKey(); }
    bool screenKeys() const override { return true; }

    // The game reads the drop key from GameInput, so the key is taken out of those readings while a protected item is in
    // the hand. The second press inside the time lets it through until the key comes up again. Whether the item counts
    // is decided here on the render thread; the key handler runs on the window thread and only looks at the result.
    void onFrame() override {
        if (notice_.exchange(false) && toast_.b)
            notify::push(i18n::tr("Inventory Lock"), i18n::fmt("Press {} again to drop it", keyName()), notify::Kind::Info, 1.6f);
        bool free = mcopt::cursorFree();
        if (wasFree_ && !free) mcopt::refresh();
        wasFree_ = free;
        if (unsigned v = mcopt::version(); v != seenOptions_) {
            seenOptions_ = v;
            readDropKey();
        }
        int drop = dropKey();
        // in the inventory the item under the cursor is not known, so every drop there needs the second press
        bool inventory = game::state().screen == game::Screen::Inventory;
        screenNow_ = int(game::state().screen);
        handLocked_ = locked(game::state().player.held(), game::state().player.slot);
        guarded_ = drop && (inventory || (!free && handLocked_));
        if (!drop) return;
        if (!input::down(drop)) pass_ = false;
        if (guarded_ && !pass_) gameinput::drop(drop);
    }

    void onKey(KeyEvent& ev) override {
        if (!ev.down || ev.repeat || ev.vk != dropKey() || inject::ours()) return;
        if (pressed() && !gameinput::active()) ev.cancel = true;
    }

    // the drop key can be a mouse button: the game's options store those as negative numbers
    void onMouse(MouseEvent& ev) override {
        if (!ev.down || ev.wheel) return;
        int vk = ev.button == MouseButton::Left ? VK_LBUTTON : ev.button == MouseButton::Right ? VK_RBUTTON :
                 ev.button == MouseButton::Middle ? VK_MBUTTON : ev.button == MouseButton::X1 ? VK_XBUTTON1 :
                 ev.button == MouseButton::X2 ? VK_XBUTTON2 : 0;
        if (!vk || vk != dropKey()) return;
        if (pressed() && !gameinput::active()) ev.cancel = true;
    }

    void drawSettings() override {
        ImGui::Spacing();
        ImGui::TextDisabled("%s", i18n::fmt("Drops blocked so far: {}", blocked_.load()).c_str());
    }

private:
    // true when this press is held back: the first one on a protected item
    bool pressed() {
        if (!dropKey()) return false;
        auto screen = game::Screen(screenNow_.load());
        if (screen != game::Screen::None && screen != game::Screen::Inventory) return false;
        if (screen != game::Screen::Inventory && !handLocked_) return false;
        double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        if (now - last_ < gap_.f / 1000.0) {
            last_ = -100.0;
            pass_ = true;
            return false;
        }
        last_ = now;
        blocked_++;
        notice_ = true;
        return true;
    }

    // what the frame saw last; the key handler on the window thread reads only these
    std::atomic<int> screenNow_{0};
    std::atomic<bool> handLocked_{false};

    static bool tool(const std::string& n) {
        static const char* parts[] = {"_sword", "_pickaxe", "_axe", "_shovel", "_hoe", "bow", "crossbow", "trident", "shears", "fishing_rod", "flint_and_steel", "mace"};
        for (auto p : parts)
            if (n.find(p) != std::string::npos) return true;
        return false;
    }

    static bool gear(const std::string& n) {
        static const char* parts[] = {"_helmet", "_chestplate", "_leggings", "_boots", "elytra", "totem_of_undying", "shield", "turtle_helmet"};
        for (auto p : parts)
            if (n.find(p) != std::string::npos) return true;
        return false;
    }

    bool locked(const game::Item& it, int slot) const {
        for (auto& s : text::split(slots_.text, ','))
            if (std::atoi(s.c_str()) == slot + 1) return true;
        if (it.empty()) return false;
        if (mode_.i == 1) return true;
        if (mode_.i == 2) return it.maxDamage > 0 || (gear_.b && gear(it.name));
        return tool(it.name) || (gear_.b && gear(it.name));
    }

    int dropKey() const { return gameKey_.b && gameDrop_ ? gameDrop_ : dropKey_.i; }

    void readDropKey() {
        if (!gameKey_.b || !mcopt::options()) return;
        int key = mcopt::gameKey("drop");
        if (key != gameDrop_) logger::info("inventory lock: the game's drop key is {}", key > VK_XBUTTON2 ? std::format("key {}", key) : key ? std::format("mouse button {}", key) : "not usable");
        gameDrop_ = key;
    }

    std::string keyName() const {
        int k = dropKey();
        return (k >= 'A' && k <= 'Z') || (k >= '0' && k <= '9') ? std::string(1, char(k)) : i18n::tr("the drop key");
    }

    Setting& mode_ = choice("mode", "Protect", {"Tools only", "All items", "Everything with durability"});
    Setting& gear_ = toggleSetting("gear", "Also armor, shield and totems", true);
    Setting& slots_ = textSetting("slots", "Always protect these hotbar slots (1 to 9, comma)", "");
    Setting& dropKey_ = keySetting("dropKey", "Drop key", 'Q');
    Setting& gap_ = slider("gap", "Time for the second press (ms)", 300.f, 100.f, 800.f, "%.0f ms");
    Setting& toast_ = toggleSetting("toast", "Show a hint when a drop is blocked", true);
    Setting& gameKey_ = toggleSetting("gameKey", "Use the drop key from the game options", true);
    double last_ = -100.0;
    std::atomic<int> blocked_{0};
    std::atomic<bool> notice_{false};
    int gameDrop_ = 0;
    bool wasFree_ = false;
    std::atomic<bool> guarded_{false};
    std::atomic<bool> pass_{false};
    unsigned seenOptions_ = 0;
};

class ModernKeybinds : public Module {
public:
    ModernKeybinds()
        : Module("Modern Keybind Handling", "Keeps your movement keys going after you close the inventory, chat or a menu while still holding them.", Category::Comfort,
                 {"input"}) {
        sub("Movement");
    }

    void onFrame() override {
        if (!inject::focused() || !game::state().inWorld) {
            resumed_.reset();
            running_ = false;
            due_ = 0.0;
            wasFree_ = false;
            return;
        }
        bool free = ui::wantsCursor() || game::state().screen != game::Screen::None;
        if (free) { resumed_.reset(); due_ = 0.0; }
        double now = ui::time();
        if (!free && !wasFree_) sprintBefore_ = game::state().player.sprinting;
        if (free && game::state().screen != game::Screen::None) closed_ = game::state().screen;
        if (wasFree_ && !free && wanted(closed_)) due_ = now + delay_.f / 1000.0;
        if (wasFree_ && !free) closed_ = game::Screen::None;
        wasFree_ = free;
        if (free || !inject::focused()) {
            due_ = 0.0;
            running_ = false;
            return;
        }
        if (due_ > 0.0 && now >= due_) {
            due_ = 0.0;
            running_ = true;
            counted_ = false;
            startedAt_ = now;
        }
        if (running_) replay(now);
        if (!running_)
            for (auto k : keys()) {
                if (!k.enabled || !input::down(k.vk)) resumed_.reset(k.vk);
                if (resumed_[k.vk]) gameinput::hold(k.vk);
            }
    }

    void onDisable() override {
        running_ = false;
        resumed_.reset();
        due_ = 0.0;
        wasFree_ = false;
    }

    void drawSettings() override {
        ImGui::Spacing();
        ImGui::TextDisabled("%s", i18n::fmt("Keys restored so far: {}", restored_).c_str());
    }

private:
    struct Key {
        int vk;
        bool enabled;
    };

    // The game keeps ignoring a key that was already down when its screen closed. Through GameInput the key is taken out
    // of the readings for a moment and let back in, so the game sees a fresh press; a double tap on W brings sprint back.
    // Without GameInput the old way, a synthetic key down, is the only one left.
    void replay(double now) {
        if (!gameinput::active()) {
            for (auto& k : keys()) {
                if (!k.enabled || !input::down(k.vk)) continue;
                inject::key(k.vk, true);
                restored_++;
            }
            running_ = false;
            return;
        }
        int step = int((now - startedAt_) / 0.055);
        bool busy = false;
        for (auto& k : keys()) {
            if (!k.enabled || !input::down(k.vk)) continue;
            resumed_.set(k.vk);
            bool tap = k.vk == 'W' && sprintBefore_ && sprint_.b;
            int steps = tap ? 4 : 2;
            if (step >= steps) { gameinput::hold(k.vk); continue; }
            busy = true;
            if (step % 2 == 0) gameinput::drop(k.vk);
            else gameinput::hold(k.vk);
        }
        if (busy && !counted_) {
            counted_ = true;
            restored_++;
        }
        if (!busy) running_ = false;
    }

    // without screen data every closed screen counts as "other"
    bool wanted(game::Screen s) const {
        switch (s) {
        case game::Screen::Inventory: return inventory_.b;
        case game::Screen::Pause: return pause_.b;
        case game::Screen::Chat: return chat_.b;
        default: return others_.b;
        }
    }

    std::array<Key, 7> keys() const {
        return {{{'W', forward_.b}, {'A', left_.b}, {'S', back_.b}, {'D', right_.b}, {VK_SPACE, jump_.b}, {VK_LSHIFT, sneak_.b}, {VK_LCONTROL, sprint_.b}}};
    }

    Setting& forward_ = toggleSetting("forward", "Forward", true);
    Setting& left_ = toggleSetting("left", "Left", true);
    Setting& back_ = toggleSetting("back", "Back", true);
    Setting& right_ = toggleSetting("right", "Right", true);
    Setting& jump_ = toggleSetting("jump", "Jump", true);
    Setting& sneak_ = toggleSetting("sneak", "Sneak", true);
    Setting& sprint_ = toggleSetting("sprint", "Sprint", true);
    Setting& delay_ = slider("delay", "Delay after closing (ms)", 80.f, 20.f, 400.f, "%.0f ms");
    Setting& inventory_ = toggleSetting("afterInventory", "After inventories and chests", true);
    Setting& pause_ = toggleSetting("afterPause", "After the pause menu", true);
    Setting& chat_ = toggleSetting("afterChat", "After the chat", true);
    Setting& others_ = toggleSetting("afterOthers", "After all other screens", true);
    game::Screen closed_ = game::Screen::None;
    bool wasFree_ = false;
    double due_ = 0.0;
    double startedAt_ = 0.0;
    bool running_ = false;
    bool counted_ = false;
    bool sprintBefore_ = false;
    int restored_ = 0;
    std::bitset<256> resumed_;
};

class JavaInventoryHotkeys : public Module {
public:
    JavaInventoryHotkeys()
        : Module("Disable Inventory Hotkeys", "Disables hotbar item-swapping shortcuts in the inventory without changing your Minecraft bindings.", Category::Comfort, {"input"}) {
        sub("Inventory");
    }

    const char* formerName() const override { return "Java Inventory Hotkeys"; }

    void load(const nlohmann::json& j) override {
        Module::load(j);
        if (!j.contains("inventoryBindingsVersion")) disableVanilla_.b = true;
    }

    nlohmann::json save() const override {
        auto j = Module::save();
        j["inventoryBindingsVersion"] = 1;
        return j;
    }

    void onEnable() override { mcopt::refresh(); load(); }
    bool screenKeys() const override { return true; }

    // the hotbar keys may have been changed in the game's settings, which is a menu like any other
    void onFrame() override {
        bool free = mcopt::cursorFree();
        if (wasFree_ && !free) mcopt::refresh();
        wasFree_ = free;
        if (unsigned v = mcopt::version(); v != seenOptions_) {
            seenOptions_ = v;
            load();
        }
        if (offhandNow_ != offhand_.i) refreshKeys();
        inventory_ = game::state().screen == game::Screen::Inventory;
        if (!inventory_) typing_ = false;
        if (disableVanilla_.b && inventory_ && !typing_) {
            for (int key : blockedKeys_) gameinput::drop(key);
        }
    }

    // The game does not tell whether a text field (recipe search, anvil name) has the focus. A letter typed in the
    // inventory is taken as the start of text: from then on the digits go through, until the next click or until
    // the inventory closes.
    void onKey(KeyEvent& ev) override {
        if (!ev.down || ev.repeat || inject::ours() || !inventory_) return;
        if (ev.vk == offhandNow_) return;
        bool slotKey = ev.vk >= 0 && ev.vk < 256 && blockedNow_[size_t(ev.vk)].load();
        if (!slotKey && ((ev.vk >= 'A' && ev.vk <= 'Z') || ev.vk == VK_SPACE || ev.vk == VK_BACK)) typing_ = true;
        if (disableVanilla_.b && slotKey && !typing_) ev.cancel = true;
    }

    void onMouse(MouseEvent& ev) override {
        if (ev.down && !ev.wheel) typing_ = false;
    }

    void drawSettings() override {
        ImGui::Spacing();
        if (ImGui::SmallButton(i18n::tr("Read the options file again"))) mcopt::refresh();
        if (!found_) {
            ImGui::TextDisabled("%s", i18n::tr("The game options file was not found."));
            return;
        }
        std::string line;
        for (int i = 0; i < 9; i++) line += std::format("{}:{} ", i + 1, bound_[size_t(i)] ? name(bound_[size_t(i)]) : "–");
        ImGui::TextDisabled("%s", line.c_str());

    }

private:
    void refreshKeys() {
        blockedKeys_.clear();
        offhandNow_ = offhand_.i;
        for (int key = 0; key < 256; ++key) {
            bool blocked = inventoryKeys::blocked(key, bound_, offhand_.i);
            blockedNow_[size_t(key)] = blocked;
            if (blocked) blockedKeys_.push_back(key);
        }
    }
    static std::string name(int vk) {
        if ((vk >= '0' && vk <= '9') || (vk >= 'A' && vk <= 'Z')) return std::string(1, char(vk));
        if (vk >= VK_F1 && vk <= VK_F12) return std::format("F{}", vk - VK_F1 + 1);
        return std::format("#{}", vk);
    }

    void load() {
        auto opts = mcopt::options();
        if (!opts) return;
        std::array<int, 9> bound{};
        auto full = opts->find("ctrl_fullkeyboardgameplay");
        int type = full != opts->end() && full->second == "1" ? 1 : 0;
        for (int i = 0; i < 9; i++) {
            auto it = opts->find(std::format("keyboard_type_{}_key.hotbar.{}", type, i + 1));
            if (it != opts->end()) bound[size_t(i)] = mcopt::gameKey(std::format("hotbar.{}", i + 1).c_str());
        }
        bound_ = bound;
        refreshKeys();
        found_ = !opts->empty();
    }

    Setting& disableVanilla_ = toggleSetting("disableVanilla", "Disable inventory hotbar shortcuts", true);
    Setting& offhand_ = keySetting("offhandKey", "Offhand key to keep available", 'F');
    std::vector<int> blockedKeys_;
    std::array<std::atomic<bool>, 256> blockedNow_{};
    std::atomic<int> offhandNow_{'F'};
    std::array<int, 9> bound_{};
    bool found_ = false;
    std::atomic<bool> inventory_{false}, typing_{false};
    bool wasFree_ = false;
    unsigned seenOptions_ = 0;
};
