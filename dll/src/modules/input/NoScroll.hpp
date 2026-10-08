#pragma once

#include "gui/Notify.hpp"
#include "hook/GameInput.hpp"
#include "hook/Input.hpp"
#include "modules/Module.hpp"

#include <windows.h>
#include <atomic>

class NoScroll : public Module {
public:
    NoScroll()
        : Module("Disable Mouse Wheel", "Stops you from scrolling through the hotbar by accident.", Category::Comfort,
                 {"input"}) {
        sub("Input");
    }

    void onEnable() override { paused_.store(0); notice_.store(false); publishInput(); }

    void onKey(KeyEvent& ev) override {
        int key = pauseInput_.load();
        if (!ev.down || ev.repeat || !key || ev.vk != key) return;
        paused_.fetch_xor(1);
        notice_.store(true);
    }

    void onMouse(MouseEvent& ev) override {
        if (ev.wheel == 0 || !blocking()) return;
        ev.cancel = true;
    }

    void onFrame() override {
        publishInput();
        if (notice_.exchange(false))
            notify::push(name(), i18n::tr(paused_.load() ? "Scrolling allowed" : "Scrolling blocked"), notify::Kind::Info, 1.5f);
        if (blocking()) gameinput::holdWheel();
    }

private:
    // only while playing: in menus, the inventory and the chat the wheel scrolls lists
    bool blocking() const {
        unsigned prefs = sneakInput_.load();
        return !paused_.load() && input::grabbed() && (!(prefs & (1u << 16)) || input::down(prefs & 0xffffu));
    }
    void publishInput() { pauseInput_ = pauseKey_.i; sneakInput_ = unsigned(sneak_.i) | (unsigned(whileSneaking_.b) << 16); }

    Setting& whileSneaking_ = toggleSetting("sneak", "Only while sneaking", false);
    Setting& sneak_ = keySetting("sneakKey", "Sneak key", VK_LSHIFT);
    Setting& pauseKey_ = keySetting("pauseKey", "Key that switches the block on and off", 0);
    std::atomic<unsigned> paused_{0};
    std::atomic<bool> notice_{false};
    std::atomic<int> pauseInput_{0};
    std::atomic<unsigned> sneakInput_{0};
};
