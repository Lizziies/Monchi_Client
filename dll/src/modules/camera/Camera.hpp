#pragma once

#include "gui/Gui.hpp"
#include "gui/Widgets.hpp"
#include "hook/GameInput.hpp"
#include "hook/Input.hpp"
#include "modules/Manager.hpp"
#include "modules/flarial/FlarialModules.hpp"
#include "modules/Module.hpp"
#include "modules/common/Bars.hpp"
#include "modules/common/Context.hpp"
#include "modules/common/Keys.hpp"
#include "modules/common/Needs.hpp"
#include "modules/post/PostFx.hpp"
#include "render/Draw.hpp"
#include "render/Ui.hpp"
#include "sdk/Effects.hpp"
#include "sdk/Game.hpp"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cmath>

class FovChanger : public Module {
public:
    FovChanger()
        : Module("FOV Changer", "Set your field of view freely. Sprint and potion effects can be turned off.",
                 Category::Visual, {"camera"}) {
        sub("Camera");
        require(0, {fx::sig(fx::Id::Fov)});
        sprintBonus_.visible = [this] { return !noEffects_.b && need::have("MoveState"); };
    }

    void onFrame() override {
        auto& p = game::state().player;
        float target = fov_.f + (p.sprinting && !noEffects_.b ? sprintBonus_.f : 0.f);
        float k = std::min(1.f, ui::dt() * smooth_.f);
        current_ = current_ <= 0.f ? target : current_ + (target - current_) * (smooth_.f >= 30.f ? 1.f : k);
        ctx::fovBase = current_;
        fx::set(fx::Id::Fov, current_);
        if (noEffects_.b) fx::set(fx::Id::FovEffects, 1.f);
        if (hand_.b) fx::set(fx::Id::ItemFov, current_);
    }

    void onDisable() override {
        ctx::fovBase = 0.f;
        current_ = 0.f;
    }

private:
    Setting& fov_ = slider("fov", "Field of view", 90.f, 30.f, 165.f, "%.0f");
    Setting& noEffects_ = needs(toggleSetting("noEffects", "Sprint and potion effects off", true), fx::Id::FovEffects);
    Setting& sprintBonus_ = slider("sprintBonus", "Extra while sprinting", 6.f, 0.f, 30.f, "%.0f");
    Setting& smooth_ = slider("smooth", "Transition speed", 12.f, 1.f, 30.f, "%.0f");
    Setting& hand_ = needs(toggleSetting("hand", "Hand follows the field of view", false), fx::Id::ItemFov);
    float current_ = 0.f;
};

class JavaDynamicFov : public Module {
public:
    JavaDynamicFov()
        : Module("Java Dynamic FOV", "Java-style FOV changes: sprinting, speed and drawing a bow change the view smoothly.", Category::Visual,
                 {"camera"}) {
        sub("Camera");
        require(need::player, {fx::sig(fx::Id::Fov), "LocalPlayer", "MoveState"});
        base_.visible = [] { return !game::has(game::Domain::Player); };
        anim_.visible = [this] { return !instant_.b; };
    }

    void onFrame() override {
        auto& p = game::state().player;
        float base = ctx::fovBase > 0.f ? ctx::fovBase : game::has(game::Domain::Player) ? p.fov : base_.f;
        float mult = 1.f;
        if (p.sprinting) mult += sprint_.f;
        for (auto& e : p.effects) {
            if (e.id == "speed") mult += 0.1f * float(e.amplifier + 1) * speed_.f;
            if (e.id == "slowness") mult -= 0.15f * float(e.amplifier + 1) * speed_.f;
        }
        if (p.flying) mult += fly_.f;
        if (p.usingItem && p.held().name == "bow") {
            float t = std::clamp(p.useProgress, 0.f, 1.f);
            mult *= 1.f - bow_.f * t * t;
        }
        current_ = instant_.b ? mult : current_ + (mult - current_) * std::min(1.f, ui::dt() * anim_.f);
        if (ctx::zooming) return;
        fx::set(fx::Id::Fov, base * current_);
        fx::set(fx::Id::FovEffects, 1.f);
    }

private:
    Setting& base_ = slider("base", "Base field of view", 70.f, 30.f, 120.f, "%.0f");
    Setting& sprint_ = slider("sprint", "Extra while sprinting", 0.15f, 0.f, 0.5f, "%.2f");
    Setting& speed_ = slider("speed", "Potion effect strength", 1.f, 0.f, 2.f, "%.1fx");
    Setting& fly_ = slider("fly", "Extra while flying", 0.1f, 0.f, 0.5f, "%.2f");
    Setting& bow_ = slider("bow", "Zoom when drawing a bow", 0.15f, 0.f, 0.5f, "%.2f");
    Setting& anim_ = slider("anim", "Animation speed", 10.f, 1.f, 30.f, "%.0f");
    Setting& instant_ = toggleSetting("instant", "Instant (no animation)", false);
    float current_ = 1.f;
};

class Zoom : public Module {
public:
    Zoom()
        : Module("Zoom", "Zoom on a key with smooth animation, scroll wheel steps and adjusted sensitivity.", Category::Visual, {"camera"}) {
        sub("Camera");
        step_.visible = [this] { return scroll_.b; };
        base_.visible = [] { return !game::has(game::Domain::Player); };
        smooth_.visible = [this] { return !instant_.b; };
        sensAmount_.visible = [this] { return sens_.b; };
        hideHand_.visible = [] { return fx::available(fx::Id::HideHand); };
        cineSmooth_.visible = [this] { return cinematic_.b; };
        barColor_.visible = [this] { return bars_.f > 0.f; };
    }

    void onEnable() override { publishInput(); }

    void onKey(KeyEvent& ev) override {
        unsigned prefs = inputPrefs_.load(std::memory_order_acquire);
        int key = prefs & 0xffff;
        if (!key || ev.vk != key || ev.repeat) return;
        press(ev.down, prefs);
    }

    void onMouse(MouseEvent& ev) override {
        unsigned prefs = inputPrefs_.load(std::memory_order_acquire);
        int key = prefs & 0xffff;
        if (mouseVk(ev.button) == key && key) press(ev.down, prefs);
        if ((prefs & scrollBit) && active_ && ev.wheel != 0) {
            float factor = std::pow(inputStep_.load(std::memory_order_relaxed), ev.wheel > 0 ? 1.f : -1.f);
            float level = level_.load();
            while (!level_.compare_exchange_weak(level, std::clamp(level * factor, 1.2f, 40.f))) {}
            snap_ = !(prefs & animateBit);
            ev.cancel = true;
        }
    }

    void onFrame() override {
        publishInput();
        if (zoom_.f != seenZoom_) {
            seenZoom_ = zoom_.f;
            level_ = zoom_.f;
        }
        // a release can get lost (menu took the keyboard, alt-tab), so hold mode follows the key's real state
        bool playing = input::grabbed() && !gui::wantsInput() && game::state().inWorld;
        if (!playing) active_ = false;
        if (mode_.i == 0) {
            bool held = playing && key_.i && input::down(key_.i);
            if (held && !active_ && !remember_.b) level_ = zoom_.f;
            active_ = held;
        }
        if (!input::focused()) {
            active_ = false;
            current_ = 1.f;
            snap_ = false;
        }
        float target = active_ ? level_.load() : 1.f;
        if ((snap_.exchange(false) || instant_.b) && active_) current_ = target;
        if (instant_.b && !active_) current_ = 1.f;
        current_ = std::fabs(current_ - target) < 0.002f ? target : draw::approach(current_, target, smooth_.f);
        ctx::zooming = current_ > 1.02f;
        ctx::zoomLevel = current_;
        ctx::hideModules = ctx::zooming && hideModules_.b;
        if (active_ && scroll_.b) gameinput::holdWheel();
        bool nativeZoom = flarialModules::zoom(current_);
        if (current_ <= 1.001f) return;
        if (hideHand_.b) fx::skip(fx::Id::HideHand);
        if (cinematic_.b && active_) gameinput::smoothMouse(1.f - cineSmooth_.f);
        if (!nativeZoom) post::params().zoom = std::max(post::params().zoom, current_);
        if (!sens_.b) return;
        float k = 1.f / std::pow(current_, sensAmount_.f);
        if (fx::available(fx::Id::Sensitivity)) fx::scale(fx::Id::Sensitivity, k);
        else gameinput::scaleMouse(k);
    }

    void drawSettings() override {
        if (fx::available(fx::Id::Fov)) return;
        ImGui::Spacing();
        widgets::hint("Zooms the picture for now. With game data for your version it changes the real field of view, which looks sharper.");
    }

    void onRender(ImDrawList* dl) override {
        if (bars_.f > 0.f && current_ > 1.02f)
            bars::draw(dl, bars_.f * std::clamp((current_ - 1.f) / 1.5f, 0.f, 1.f), ImGui::GetColorU32(barColor_.color));
        if (vignette_.f <= 0.f || current_ <= 1.02f) return;
        float k = std::clamp((current_ - 1.f) / 3.f, 0.f, 1.f) * vignette_.f;
        auto ds = ImGui::GetIO().DisplaySize;
        float e = std::min(ds.x, ds.y) * 0.35f;
        ImU32 solid = IM_COL32(0, 0, 0, int(200 * k)), clear = IM_COL32(0, 0, 0, 0);
        dl->AddRectFilledMultiColor({0, 0}, {ds.x, e}, solid, solid, clear, clear);
        dl->AddRectFilledMultiColor({0, ds.y - e}, {ds.x, ds.y}, clear, clear, solid, solid);
        dl->AddRectFilledMultiColor({0, 0}, {e, ds.y}, solid, clear, clear, solid);
        dl->AddRectFilledMultiColor({ds.x - e, 0}, {ds.x, ds.y}, clear, solid, solid, clear);
    }

    void onDisable() override {
        active_ = false;
        flarialModules::zoom(1.f);
        ctx::zooming = false;
        ctx::hideModules = false;
        current_ = 1.f;
    }

private:
    static constexpr unsigned toggleBit = 1u << 16, scrollBit = 1u << 17, rememberBit = 1u << 18, animateBit = 1u << 19;

    void publishInput() {
        inputStep_.store(step_.f, std::memory_order_relaxed);
        inputZoom_.store(zoom_.f, std::memory_order_relaxed);
        unsigned prefs = unsigned(key_.i) & 0xffff;
        if (mode_.i != 0) prefs |= toggleBit;
        if (scroll_.b) prefs |= scrollBit;
        if (remember_.b) prefs |= rememberBit;
        if (always_.b) prefs |= animateBit;
        inputPrefs_.store(prefs, std::memory_order_release);
    }

    std::atomic<unsigned> inputPrefs_{0};
    std::atomic<float> inputStep_{1.2f}, inputZoom_{4.f};
    void press(bool down, unsigned prefs) {
        if (!(prefs & toggleBit)) {
            active_ = down;
        } else if (down) {
            active_.fetch_xor(1);
        }
        if (active_ && !(prefs & rememberBit)) level_ = inputZoom_.load(std::memory_order_relaxed);
    }

    Setting& key_ = keySetting("zoomKey", "Zoom key", 'C');
    Setting& mode_ = choice("mode", "Mode", {"Hold", "Toggle"});
    Setting& zoom_ = slider("zoom", "Zoom level", 4.f, 1.5f, 20.f, "%.1fx");
    Setting& base_ = slider("base", "Base field of view", 70.f, 30.f, 120.f, "%.0f");
    Setting& instant_ = toggleSetting("instant", "Instant (no animation)", false);
    Setting& smooth_ = slider("smooth", "Animation speed", 14.f, 2.f, 40.f, "%.0f");
    Setting& scroll_ = toggleSetting("scroll", "Change level with the mouse wheel", true);
    Setting& step_ = slider("step", "Step size", 1.2f, 1.05f, 1.6f, "%.2fx");
    Setting& remember_ = toggleSetting("remember", "Remember level", true);
    Setting& sens_ = toggleSetting("sens", "Adjust sensitivity while zooming", true);
    Setting& sensAmount_ = slider("sensAmount", "Sensitivity reduction", 0.85f, 0.f, 1.f, "%.2f");
    Setting& vignette_ = slider("vignette", "Dark edge while zooming", 0.f, 0.f, 1.f, "%.2f");
    Setting& bars_ = slider("bars", "Cinematic bars", 0.f, 0.f, 0.2f, "%.2f");
    Setting& barColor_ = colorSetting("barColor", "Bar color", {0.f, 0.f, 0.f, 1.f});
    Setting& cinematic_ = toggleSetting("cinematic", "Cinematic camera while zooming", false);
    Setting& cineSmooth_ = slider("cineSmooth", "Smoothing", 0.7f, 0.f, 0.95f, "%.2f");
    Setting& hideHand_ = toggleSetting("hideHand", "Hide the hand while zooming", false);
    Setting& hideModules_ = toggleSetting("hideModules", "Hide the HUD modules while zooming", false);
    Setting& always_ = toggleSetting("always", "Always animate (also when scrolling)", true);
    std::atomic<bool> snap_{false};
    std::atomic<unsigned> active_{0};
    float seenZoom_ = -1.f;
    std::atomic<float> level_{4.f};
    float current_ = 1.f;
};

class Freelook : public Module {
public:
    Freelook()
        : Module("Freelook", "Turn the camera freely around you while your body and walking direction stay. Banned on some servers.", Category::Visual, {"camera"}) {
        sub("Camera");
        require(need::player, need::sigs({"LocalPlayer", "FreeCamera"}));
        view_.visible = [this] { return thirdPerson_.b; };
    }

    void onEnable() override { publishInput(); }

    void onKey(KeyEvent& ev) override {
        unsigned prefs = inputPrefs_.load(std::memory_order_acquire);
        int key = prefs & 0xffff;
        if (!key || ev.vk != key || ev.repeat) return;
        press(ev.down, prefs);
    }

    void onMouse(MouseEvent& ev) override {
        unsigned prefs = inputPrefs_.load(std::memory_order_acquire);
        int key = prefs & 0xffff;
        if (key && mouseVk(ev.button) == key) press(ev.down, prefs);
    }

    void onFrame() override {
        publishInput();
        if (!input::grabbed() || gui::wantsInput() || !game::state().inWorld) active_ = false;
        else if (mode_.i == 0) active_ = key_.i && input::down(key_.i);
        ctx::freelook = active_ && game::freeCamera(true, moveHead_.b);
        if (!ctx::freelook) {
            game::freeCamera(false);
            return;
        }
        if (thirdPerson_.b) fx::setInt(fx::Id::Perspective, 1 + view_.i);
    }

    void onDisable() override {
        active_ = false;
        ctx::freelook = false;
        game::freeCamera(false);
    }

private:
    void publishInput() { inputPrefs_.store((unsigned(key_.i) & 0xffff) | (mode_.i ? 1u << 16 : 0), std::memory_order_release); }
    std::atomic<unsigned> inputPrefs_{0};
    void press(bool down, unsigned prefs) {
        if (!(prefs & (1u << 16))) active_ = down;
        else if (down) active_.fetch_xor(1);
    }

    Setting& moveHead_ = toggleSetting("moveHead", "Move head with camera", false);
    Setting& key_ = keySetting("freelookKey", "Freelook key", VK_LMENU);
    Setting& mode_ = choice("mode", "Mode", {"Hold", "Toggle"});
    Setting& thirdPerson_ = toggleSetting("thirdPerson", "Switch to third person", true);
    Setting& view_ = choice("view", "View", {"Third person back", "Third person front"});
    std::atomic<unsigned> active_{0};
};

class NoViewBobbing : public Module {
public:
    NoViewBobbing()
        : Module("No View Bobbing", "Turns off camera and hand bobbing while walking.",
                 Category::Visual, {"camera"}) {
        sub("Camera");
        require(0, {fx::sig(fx::Id::ViewBob)});
    }

    void onFrame() override {
        auto& p = game::state().player;
        bool on = when_.i == 0 || (when_.i == 1 && p.sprinting) || (when_.i == 2 && p.inWater) || (when_.i == 3 && !p.sprinting);
        if (!on) return;
        fx::skip(fx::Id::ViewBob);
    }

private:
    Setting& when_ = choice("when", "When", {"Always", "Only while sprinting", "Only underwater", "Only while walking"});
};

class NoHurtCam : public Module {
public:
    NoHurtCam()
        : Module("No Hurt Cam", "The camera no longer shakes when you get hit.", Category::Visual, {"camera"}) {
        sub("Camera");
        require(0, {fx::sig(fx::Id::HurtCam)});
        // the game's damage bobbing is a switch (option 39 on 1.26.52), so there is nothing in between to set
        strength_.hidden = true;
    }

    void onFrame() override { fx::skip(fx::Id::HurtCam); }

private:
    Setting& strength_ = slider("strength", "Remaining strength", 0.f, 0.f, 1.f, "%.2f");
};

class AutoPerspective : public Module {
public:
    AutoPerspective()
        : Module("Auto Perspective", "Switches the perspective automatically, for example when gliding or drawing a bow.",
                 Category::Visual, {"camera"}) {
        sub("Camera");
        require(need::player | need::inventory, {fx::sig(fx::Id::Perspective), "LocalPlayer", "MoveState"});
    }

    void onEnable() override { inputManual_.store(keepManual_.b, std::memory_order_relaxed); }

    void onKey(KeyEvent& ev) override {
        if (inputManual_.load(std::memory_order_relaxed) && ctxView_ >= 0 && ev.vk == VK_F5 && ev.down && !ev.repeat) manual_ = true;
    }

    void onDisable() override { ctxView_ = -1; manual_ = false; }

    void onFrame() override {
        inputManual_.store(keepManual_.b, std::memory_order_relaxed);
        auto& p = game::state().player;
        int want = -1;
        if (p.emoting && emote_.i > 0) want = emote_.i - 1;
        else if (p.swimming && swim_.i > 0) want = swim_.i - 1;
        else if (p.gliding && glide_.i > 0) want = glide_.i - 1;
        else if (p.usingItem && p.held().name == "bow" && bow_.i > 0) want = bow_.i - 1;
        if (want >= 0) {
            if (ctxView_ < 0) {
                startView_ = int(p.view);
                manual_ = false;
            }
            ctxView_ = want;
            if (!manual_) fx::setInt(fx::Id::Perspective, want);
        } else if (ctxView_ >= 0) {
            if (restore_.b && !manual_) fx::setInt(fx::Id::Perspective, startView_);
            ctxView_ = -1;
            manual_ = false;
        }
    }

private:
    Setting& swim_ = choice("swim", "While swimming", {"Do not change", "First person", "Third person back", "Third person front"});
    Setting& emote_ = choice("emote", "While emoting", {"Do not change", "First person", "Third person back", "Third person front"}, 2);
    Setting& glide_ = choice("glide", "While gliding", {"Do not change", "First person", "Third person back", "Third person front"}, 2);
    Setting& bow_ = choice("bow", "While drawing a bow", {"Do not change", "First person", "Third person back", "Third person front"}, 1);
    Setting& restore_ = toggleSetting("restore", "Switch back afterwards", true);
    Setting& keepManual_ = toggleSetting("keepManual", "Keep a view you pick yourself (F5)", true);
    std::atomic<int> ctxView_{-1};
    std::atomic<bool> inputManual_{true};
    int startView_ = 0;
    std::atomic<bool> manual_{false};
};

class Fullbright : public Module {
public:
    Fullbright()
        : Module("Fullbright", "Brightens the whole game, with a smooth fade and optionally only at night. The level sets how bright; the Brightness / Contrast filter adds to it.", Category::Visual, {"camera"}) {
        sub("World");
        require(0, {fx::sig(fx::Id::Gamma)});
    }

    void onFrame() override {
        auto& w = game::state().world;
        bool dark = !onlyNight_.b || !game::has(game::Domain::World) || w.time >= 12500 || w.time < 500;
        float target = dark ? level_.f : 0.f;
        current_ += (target - current_) * std::min(1.f, ui::dt() * (fade_.b ? 4.f : 60.f));
        if (current_ > 0.05f) fx::set(fx::Id::Gamma, std::max(current_, 1.f));
    }

    void onDisable() override { current_ = 0.f; }

private:
    Setting& level_ = slider("level", "Brightness level", 12.f, 1.f, 25.f, "%.0f");
    Setting& fade_ = toggleSetting("fade", "Smooth fade", true);
    Setting& onlyNight_ = toggleSetting("onlyNight", "Only at night", false);
    float current_ = 0.f;
};

class CinematicCamera : public Module {
public:
    CinematicCamera()
        : Module("Cinematic Camera", "Soft, gliding camera movement like in film shots, with black bars. Optionally only while zooming.", Category::Visual, {"camera"}) {
        sub("Camera");
        key_.visible = [this] { return activation_.i != 0; };
        barColor_.visible = [this] { return bars_.f > 0.f; };
    }

    void onEnable() override { publishInput(); }

    void onKey(KeyEvent& ev) override {
        unsigned prefs = inputPrefs_.load(std::memory_order_acquire);
        int key = prefs & 0xffff;
        if ((prefs >> 16) == 1 && key && ev.vk == key && ev.down && !ev.repeat) toggled_.fetch_xor(1);
    }

    void onMouse(MouseEvent& ev) override {
        unsigned prefs = inputPrefs_.load(std::memory_order_acquire);
        int key = prefs & 0xffff;
        if ((prefs >> 16) == 1 && key && mouseVk(ev.button) == key && ev.down) toggled_.fetch_xor(1);
    }

    void onFrame() override {
        publishInput();
        bool held = key_.i && input::down(key_.i) && !gui::wantsInput();
        bool keyed = activation_.i == 0 || (activation_.i == 1 ? bool(toggled_.load()) : held);
        bool on = keyed && (!onlyZoom_.b || ctx::zooming) && game::state().inWorld && input::grabbed() && !gui::wantsInput();
        ctx::cinematicHide = on && hideHud_.b;
        flarialModules::hideHud(ctx::cinematicHide);
        shown_ += ((on ? 1.f : 0.f) - shown_) * std::min(1.f, ui::dt() * 6.f);
        if (on) gameinput::smoothMouse(1.f - smoothing_.f);
    }

    void onRender(ImDrawList* dl) override { bars::draw(dl, bars_.f * shown_, ImGui::GetColorU32(barColor_.color)); }

    void onDisable() override {
        ctx::cinematicHide = false;
        flarialModules::hideHud(false);
        shown_ = 0.f;
        toggled_ = false;
    }

private:
    void publishInput() { inputPrefs_.store((unsigned(key_.i) & 0xffff) | (unsigned(activation_.i) << 16), std::memory_order_release); }
    std::atomic<unsigned> inputPrefs_{0};
    Setting& hideHud_ = toggleSetting("hideHud", "Hide HUD and hotbar", true);
    Setting& smoothing_ = slider("smoothing", "Smoothing", 0.7f, 0.f, 0.95f, "%.2f");
    Setting& onlyZoom_ = toggleSetting("onlyZoom", "Only while zooming", false);
    Setting& activation_ = choice("activation", "Active", {"Always", "Toggle with a key", "While a key is held"});
    Setting& key_ = keySetting("cineKey", "Key", 0);
    Setting& bars_ = slider("bars", "Cinematic bars", 0.08f, 0.f, 0.2f, "%.2f");
    Setting& barColor_ = colorSetting("barColor", "Bar color", {0.f, 0.f, 0.f, 1.f});
    float shown_ = 0.f;
    std::atomic<unsigned> toggled_{0};

};
