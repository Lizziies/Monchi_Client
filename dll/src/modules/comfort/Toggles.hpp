#pragma once

#include "core/Config.hpp"
#include "gui/Gui.hpp"
#include "gui/Notify.hpp"
#include "hook/Dx.hpp"
#include "hook/GameInput.hpp"
#include "modules/HudModule.hpp"
#include "modules/common/Keys.hpp"
#include "modules/common/Needs.hpp"
#include "render/Ui.hpp"
#include "sdk/Game.hpp"
#include "sdk/Inject.hpp"

#include <windows.h>

#include <atomic>
#include <cmath>
#include <format>

class StickyKey : public HudModule {
public:
    StickyKey(std::string name, std::string desc, int defaultKey, ImVec2 pos, const char* onText, const char* offText)
        : HudModule(std::move(name), std::move(desc), {"input"}, pos), key_(keySetting("gameKey", "Key in game", defaultKey)),
          onText_(textSetting("onText", "Text when on", onText)), offText_(textSetting("offText", "Text when off", offText)) {
        sub("Movement");
        background_.b = true;
    }

    void onEnable() override { publishInput(); }

    void onKey(KeyEvent& ev) override {
        int key = inputKey_.load(std::memory_order_relaxed);
        if (inject::ours() || ev.vk != key || !key) return;
        if (inputAutomatic_.load(std::memory_order_relaxed)) return;
        if (ev.repeat) {
            if (held_) ev.cancel = true;
            return;
        }
        if (ev.down) {
            if (held_) {
                held_ = false;
                ev.cancel = true;
                inject::key(key, false);
            } else {
                held_ = true;
            }
        } else if (held_) {
            ev.cancel = true;
        }
    }

    void onFrame() override {
        publishInput();
        if (!inject::focused() || game::state().screen != game::Screen::None || gui::capturesKeyboard()) {
            // a key pressed for the player is let go while a screen is open, or it would act in the chat and inventory
            if (autoHeld_) {
                inject::key(key_.i, false);
                autoHeld_ = false;
            }
            return;
        }
        if (mode() == 1) autoUpdate();
        else if (autoHeld_) {
            inject::key(key_.i, false);
            autoHeld_ = false;
        }
        if ((held_ || autoHeld_) && inject::focused()) gameinput::hold(key_.i);
    }

    void onDisable() override {
        if (held_ || autoHeld_) inject::key(inputKey_.load(std::memory_order_relaxed), false);
        held_ = autoHeld_ = false;
    }

    void onRender(ImDrawList* dl) override {
        if (!show_.b && !gui::editingHud()) return;
        bool on = held_ || autoHeld_;
        if (!on && !always_.b && !gui::editingHud()) return;
        HudModule::onRender(dl);
    }

protected:
    virtual int mode() const = 0;
    virtual void autoUpdate() {}

    virtual std::string status(bool on) const { return i18n::tr((on ? onText_ : offText_).text.c_str()); }

    ImVec2 content(ImDrawList* dl, ImVec2 o, float s) override {
        bool on = held_ || autoHeld_;
        return drawText(dl, o, s, status(on), on ? accentColor() : textColor());
    }

    Setting& key_;
    Setting& onText_;
    Setting& offText_;
    Setting& always_ = toggleSetting("always", "Also show when off", false);
    Setting& show_ = toggleSetting("showStatus", "Show status", true);
    std::atomic<bool> held_{false};
    bool autoHeld_ = false;

private:
    void publishInput() {
        int key = key_.i;
        bool automatic = mode() == 1;
        int previous = inputKey_.exchange(key, std::memory_order_relaxed);
        bool wasAutomatic = inputAutomatic_.exchange(automatic, std::memory_order_relaxed);
        if (previous && (previous != key || wasAutomatic != automatic) && (held_ || autoHeld_)) {
            inject::key(previous, false);
            held_ = autoHeld_ = false;
        }
    }

    std::atomic<int> inputKey_{0};
    std::atomic<bool> inputAutomatic_{false};
};

class ToggleSprint : public StickyKey {
public:
    ToggleSprint() : StickyKey("Toggle Sprint", "Sprinting stays on until you press the key again.", VK_LCONTROL, {0.005f, 0.925f}, "[Sprint: on]", "[Sprint: off]") {
        forward_.visible = [this] { return mode_.i == 1; };
        source_.visible = [this] { return state_.b; };
        state_.visible = [] { return need::have("MoveState"); };
    }

protected:
    int mode() const override { return mode_.i == 0 ? 0 : 1; }

    std::string status(bool on) const override {
        if (!state_.b || !need::have("MoveState")) return StickyKey::status(on);
        auto& p = game::state().player;
        const char* what = "Standing";
        if (p.sneaking) what = "Sneaking";
        else if (p.gliding) what = "Gliding";
        else if (p.swimming) what = "Swimming";
        else if (p.sprinting) what = "Sprinting";
        else if (std::abs(p.vel.x) + std::abs(p.vel.z) > 0.01f) what = "Walking";
        std::string out = i18n::tr(what);
        if (source_.b && !p.sneaking && !p.gliding) out += i18n::tr(on ? " (toggled)" : " (vanilla)");
        return out;
    }

    void autoUpdate() override {
        bool forward = mode_.i == 2 || input::down(forward_.i);
        if (forward && !autoHeld_ && inject::focused() && !gui::capturesKeyboard()) {
            inject::key(key_.i, true);
            autoHeld_ = true;
        } else if (!forward && autoHeld_) {
            inject::key(key_.i, false);
            autoHeld_ = false;
        }
    }

private:
    Setting& mode_ = choice("mode", "Mode", {"Toggle with the key", "Automatic while walking forward", "Always sprint"});
    Setting& forward_ = keySetting("forward", "Forward key", 'W');
    Setting& state_ = toggleSetting("state", "Show what you are doing (walking, sprinting ...)", false);
    Setting& source_ = toggleSetting("source", "Show whether sprint is toggled", true);
};

class ToggleSneak : public StickyKey {
public:
    ToggleSneak() : StickyKey("Toggle Sneak", "Sneaking stays on until you press the sneak key again.", VK_LSHIFT, {0.005f, 0.96f}, "[Sneak: on]", "[Sneak: off]") {}

protected:
    int mode() const override { return 0; }
};

class SlotHotkeys : public Module {
public:
    SlotHotkeys(std::string name, std::string desc, bool command)
        : Module(std::move(name), std::move(desc), Category::Comfort, {"chat"}), command_(command) {
        sub("Chat");
        for (int i = 0; i < slots; i++) {
            std::string n = std::to_string(i + 1);
            keys_[i] = &keySetting("key" + n, "Key " + n, 0);
            texts_[i] = &textSetting("text" + n, command ? "Command " + n : "Text " + n, "");
            if (i == 0) continue;
            keys_[i]->visible = texts_[i]->visible = [this, i] { return used(i) || used(i - 1); };
        }
        chatKey_ = &keySetting("chatKey", "Chat key in game", 'T');
    }

    void onEnable() override { publishKeys(); }
    void onDisable() override { pending_ = 0; }

    void onKey(KeyEvent& ev) override {
        if (!ev.down || ev.repeat) return;
        for (int i = 0; i < slots; i++) {
            int key = inputKeys_[i].load(std::memory_order_relaxed);
            if (!key || ev.vk != key) continue;
            ev.cancel = true;
            unsigned queued = pending_.load();
            for (;;) {
                int shift = 0;
                while (shift < 16 && ((queued >> shift) & 15u)) shift += 4;
                if (shift == 16) return;
                unsigned next = queued | (unsigned(i + 1) << shift);
                if (pending_.compare_exchange_weak(queued, next)) return;
            }
        }
    }

    void onFrame() override {
        publishKeys();
        unsigned queued = pending_.exchange(0);
        for (; queued; queued >>= 4) {
            int i = int(queued & 15u) - 1;
            if (i < 0 || i >= slots || texts_[i]->text.empty()) continue;
            std::string text = texts_[i]->text;
            if (command_ && text[0] != '/') text.insert(text.begin(), '/');
            inject::say(text, chatKey_->i);
        }
    }

private:
    void publishKeys() {
        for (int i = 0; i < slots; ++i)
            inputKeys_[i].store(texts_[i]->text.empty() ? 0 : keys_[i]->i, std::memory_order_relaxed);
    }
    bool used(int i) const { return keys_[i]->i || !texts_[i]->text.empty(); }

    static constexpr int slots = 12;
    std::atomic<int> inputKeys_[slots]{};
    std::atomic<unsigned> pending_{0};
    bool command_;
    Setting* keys_[slots]{};
    Setting* texts_[slots]{};
    Setting* chatKey_ = nullptr;
};

class CommandHotkey : public SlotHotkeys {
public:
    CommandHotkey() : SlotHotkeys("Command Hotkey", "Hotkeys for chat commands like /hub. Opens the chat like a player and sends the command.", true) {}
};

class TextHotkey : public SlotHotkeys {
public:
    TextHotkey() : SlotHotkeys("Text Hotkey", "Hotkeys for ready-made chat messages.", false) {}
};

class ProfileHotkeys : public Module {
public:
    ProfileHotkeys()
        : Module("Profile Hotkeys", "Switches between your saved settings profiles on a key.", Category::Comfort, {"cosmetic"}) {
        sub("Profiles");
        for (int i = 0; i < slots; i++) {
            std::string n = std::to_string(i + 1);
            keys_[i] = &keySetting("key" + n, "Key " + n, 0);
            names_[i] = &textSetting("profile" + n, "Profile " + n, "");
        }
    }

    // a profile rewrites the settings of every module, which may only happen on the render thread; the key just asks
    void onKey(KeyEvent& ev) override {
        if (!ev.down || ev.repeat) return;
        for (int i = 0; i < slots; i++)
            if (int key = inputKeys_[i].load(); key && ev.vk == key) wanted_ = i;
    }

    void onEnable() override { publishKeys(); profileAt_ = 0.0; }
    void onDisable() override { wanted_ = -1; }

    void onFrame() override {
        publishKeys();
        int i = wanted_.exchange(-1);
        if (i < 0 || names_[i]->text.empty()) return;
        auto all = config::profiles();
        if (std::find(all.begin(), all.end(), names_[i]->text) == all.end()) {
            notify::push(i18n::tr("Profile not found"), names_[i]->text, notify::Kind::Warn);
            return;
        }
        config::switchProfile(names_[i]->text);
        notify::push(i18n::tr("Profile switched"), names_[i]->text, notify::Kind::Ok);
    }

    void drawSettings() override {
        ImGui::Spacing();
        double now = ui::time();
        if (!profileAt_ || now - profileAt_ >= 1.0) {
            profileAt_ = now;
            profileList_.clear();
            for (auto& p : config::profiles()) profileList_ += (profileList_.empty() ? "" : ", ") + p;
        }
        ImGui::TextDisabled(i18n::tr("Available profiles: %s"), profileList_.c_str());
        ImGui::TextDisabled(i18n::tr("Active: %s"), config::profile().c_str());
    }

private:
    static constexpr int slots = 4;
    Setting* keys_[slots]{};
    Setting* names_[slots]{};
    std::atomic<int> wanted_{-1};
    std::atomic<int> inputKeys_[slots]{};
    std::string profileList_;
    double profileAt_ = 0.0;
    void publishKeys() { for (int i = 0; i < slots; ++i) inputKeys_[i] = keys_[i]->i; }
};
