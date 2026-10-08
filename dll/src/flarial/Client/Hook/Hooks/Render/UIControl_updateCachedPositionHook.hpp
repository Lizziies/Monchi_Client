// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Hook/Hooks/Render/UIControl_updateCachedPositionHook.hpp: picks the hook by which signature
// exists, not by game version.
#pragma once

#include "../Hook.hpp"
#include "../../../../Utils/Memory/Game/SignatureAndOffsetManager.hpp"
#include "../../../../Bridge/Input.hpp"

class UIControl_updateCachedPositionHook : public Hook {
private:
    static bool isLayer(UIControl *c, const char *name, size_t length) {
        __try {
            const std::string &s = c->getLayerName();
            return s.size() == length && memcmp(s.data(), name, length) == 0;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    // Better Chat and Monchi's Scoreboard draw their own; the game's panels move off the screen while those are on. The
    // children follow because they are laid out from the panel's position. Both names were read from the running game
    // (1.26.52.3): a control "chat_panel" and a control "sidebar", positioned at +0x10 like every UIControl.
    struct Layer {
        const char *name;
        size_t length;
        std::atomic<bool> &hide;
        bool applied = false;
        // where the game had put the panel before it was moved away, to put it back exactly there
        Vec2<float> home{};
        bool moved = false;
    };

    static inline Layer layers[2] = {{"chat_panel", 10, monchiInput::hideChat}, {"sidebar", 7, monchiInput::hideScoreboard}};

    static void hideLayers(UIControl *c) {
        for (auto &layer : layers) {
            bool hide = layer.hide.load();
            if ((!hide && !layer.moved) || !isLayer(c, layer.name, layer.length)) continue;
            Vec2<float> away{-5000.f, -5000.f};
            bool isAway = c->parentRelativePosition == away;
            if (hide == isAway) {
                if (!hide) layer.moved = false;
                return;
            }
            if (hide) layer.home = c->parentRelativePosition;
            c->parentRelativePosition = hide ? away : layer.home;
            layer.moved = hide;
            c->forEachChild([](std::shared_ptr<UIControl> &child) { child->updatePosition(); });
            return;
        }
    }

    // a panel only lays itself out again when something changes, so a switch is followed by one forced update
    struct LayerSwitch : Listener {
        ULONGLONG tried = 0;
        const void* root = nullptr;

        void onFrame(SetupAndRenderEvent &) {
            if (!SDK::screenView || !SDK::screenView->VisualTree || !SDK::screenView->VisualTree->root) return;
            const void* current = SDK::screenView->VisualTree->root;
            if (root != current) {
                root = current;
                for (auto& layer : layers) layer.applied = !layer.hide.load();
            }
            bool pending = false;
            for (auto &layer : layers) pending |= layer.hide.load() != layer.applied;
            if (!pending) return;
            ULONGLONG now = GetTickCount64();
            if (now - tried < 250) return;
            tried = now;
            try {
                if (SDK::screenView && SDK::screenView->VisualTree && SDK::screenView->VisualTree->root)
                    SDK::screenView->VisualTree->root->forEachControl([&](std::shared_ptr<UIControl> &c) {
                        bool again = false;
                        for (auto &layer : layers) {
                            if (layer.hide.load() == layer.applied) continue;
                            if (isLayer(c.get(), layer.name, layer.length)) {
                                c->updatePosition();
                                layer.applied = layer.hide.load();
                                Logger::info("flarial core: the game's {} is {}", layer.name, layer.applied ? "hidden" : "back");
                            } else {
                                again = true;
                            }
                        }
                        return !again;
                    });
            } catch (...) {
            }
        }
    };

    static inline LayerSwitch layerSwitch;

    static Vec2<float>* UIControl_getPosition(UIControl *_this) {
        static auto setCachedPosition = reinterpret_cast<decltype(&UIControl_getPosition)>(funcOriginal);

        auto* res = setCachedPosition(_this);

        auto event = nes::make_holder<UIControlGetPositionEvent>(_this, res);
        eventMgr.trigger(event);

        return res;
    }

    static void UIControl_updateCachedPosition21_30(UIControl *_this) {
        static auto setCachedPosition = reinterpret_cast<decltype(&UIControl_updateCachedPosition21_30)>(funcOriginal);

        setCachedPosition(_this);

        auto event = nes::make_holder<UIControlGetPositionEvent>(_this, nullptr);
        eventMgr.trigger(event);
        hideLayers(_this);

        return;
    }

public:
    static inline void* funcOriginal = nullptr;

    static bool hasGetPosition() { return GET_SIG_ADDRESS("UIControl::getPosition") != 0; }

    UIControl_updateCachedPositionHook() : Hook("UIControl_updateCachedPositionHook", hasGetPosition() ? GET_SIG_ADDRESS("UIControl::getPosition") : GET_SIG_ADDRESS("UIControl::_setCachedPosition")) {}

    void enableHook() override {
        if(hasGetPosition()) {
            this->autoHook((void *) UIControl_getPosition, (void **) &funcOriginal);
        } else {
            this->autoHook((void *) UIControl_updateCachedPosition21_30, (void **) &funcOriginal);
        }
        eventMgr.getDispatcher().listen<SetupAndRenderEvent, &LayerSwitch::onFrame, EventOrder::NORMAL>(&layerSwitch);
    }
};