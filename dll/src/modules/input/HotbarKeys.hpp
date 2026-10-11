#pragma once

#include "hook/GameInput.hpp"
#include "modules/Module.hpp"
#include "sdk/Inject.hpp"

#include <windows.h>

class HotbarKeys : public Module {
public:
    HotbarKeys()
        : Module("Hotbar Keys", "Assigns your own keys to the hotbar slots, for example mouse buttons or keys next to WASD.", Category::Comfort, {"input"}) {
        sub("Movement");
        for (int i = 0; i < 9; i++) keys_[i] = &keySetting("slot" + std::to_string(i + 1), "Slot " + std::to_string(i + 1), 0);
    }

    // the key now stands for a hotbar slot, so what it does in the game by itself is taken away
    void onFrame() override {
        publishKeys();
        for (auto& k : inputKeys_)
            if (int key = k.load(); key > VK_XBUTTON2) gameinput::drop(key);
    }
    void onEnable() override { publishKeys(); }

    void onKey(KeyEvent& ev) override {
        for (int i = 0; i < 9; i++) {
            int key = inputKeys_[i].load();
            if (!key || ev.vk != key) continue;
            ev.cancel = true;
            if (ev.down && !ev.repeat) inject::tapLater('1' + i);
            return;
        }
    }

    void onMouse(MouseEvent& ev) override {
        int vk = ev.button == MouseButton::Middle ? VK_MBUTTON : ev.button == MouseButton::X1 ? VK_XBUTTON1 : ev.button == MouseButton::X2 ? VK_XBUTTON2 : 0;
        if (!vk) return;
        for (int i = 0; i < 9; i++) {
            if (inputKeys_[i].load() != vk) continue;
            ev.cancel = true;
            if (ev.down) inject::tapLater('1' + i);
            return;
        }
    }

private:
    Setting* keys_[9]{};
    std::atomic<int> inputKeys_[9]{};
    void publishKeys() { for (int i = 0; i < 9; ++i) inputKeys_[i] = keys_[i]->i; }
};
