#pragma once

#include "Types.hpp"

#include <imgui.h>

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace game {

class Provider {
public:
    virtual ~Provider() = default;
    virtual unsigned supports() const = 0;
    virtual bool derived() const = 0;
    virtual void update(State& s, std::vector<Event>& events) = 0;
    virtual void use(unsigned) {}
    // our own player actor, 0 when not known
    virtual uintptr_t actor() { return 0; }
};

void init();
void update();
void shutdown();

const State& state();
const std::vector<Event>& events();

bool demo();
void setDemo(bool on);
void setDemoServer(const std::string& name);
const std::string& demoServer();

void filterChat(std::function<bool(const std::string&)> hide);
// Called on the game's thread for every line before the game shows it: may change who it is from and its text
// (color codes, a heart in front of a name). True when something was changed.
void setChatDecor(std::function<bool(std::string& sender, std::string& body)> decor);
bool chatDecor(std::string& sender, std::string& body);
bool chatHidden(const std::string& text);
// Better Chat shows the lines itself: new lines the menu fonts can draw no longer appear in the game's own hud chat
void hideChatHud(bool hide);
// true while something draws the lines only the game's font can show, so they may leave the game's hud as well
void drawNativeChat(bool drawn);
bool nativeChatDrawn();
bool chatHudHidden();

bool ready(unsigned mask);
bool freeCamera(bool on, bool moveHead = false);
void hide(uintptr_t entity, float seconds);
void unhide(uintptr_t entity);
int hidden();
bool has(Domain d);
void lease(unsigned mask, int delta);

void resetCombat();
void resetHitCounts();
uintptr_t selfActor();
// stops the totem activation overlay that is playing right now; false where the field is not known for this version
bool clearTotemAnimation();
std::optional<ImVec2> project(const Vec3& p);
bool projectLine(const Vec3& a, const Vec3& b, ImVec2& out0, ImVec2& out1);

}
