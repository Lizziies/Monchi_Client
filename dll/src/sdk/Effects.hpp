#pragma once

#include "Types.hpp"

#include <initializer_list>
#include <string>
#include <vector>

namespace fx {

bool takeChanged();
bool available(const std::string& signature);

enum class Kind { Value, Flag, Skip, Out, Data, Int, Ghost, Filter, Option };

enum class Id {
    Fov,
    FovEffects,
    Gamma,
    ViewBob,
    HandBob,
    HurtCam,
    BobStrength,
    Perspective,
    SneakCam,
    Sensitivity,
    TimeOfDay,
    Rain,
    Thunder,
    Clouds,
    Sky,
    Particles,
    BlockEntities,
    Shadows,
    Fog,
    Vignette,
    FireHeight,
    Hitbox,
    HitboxColor,
    GlintColor,
    HurtColor,
    FogColor,
    WaterColor,
    ParticleScale,
    GuiScale,
    HideHand,
    HideOffhand,
    HideChat,
    HideScoreboard,
    HideCrosshair,
    HideHud,
    HandMatrix,
    LookTurn,
    LookCamera,
    LookDelta,
    SelfNametag,
    ItemPhysics,
    UseDelay,
    InventoryDelay,
    HurtAnim,
    BlockOutline,
    SwingSpeed,
    CrystalHide,
    CrystalSimple,
    CrystalNoBase,
    GhostRender,
    GhostPick,
    CritParticle,
    HitboxEye,
    HitboxEyeColor,
    HitboxLook,
    HitboxLookColor,
    HitboxLookLength,
    HitboxWidth,
    HitboxSelf,
    HitboxJava,
    HitboxRange,
    Hitbox2D,
    ItemFov,
    HandMatrixThird,
    RenderEntities,
    RenderTerrain,
    ItemPhysicsData,
    NametagText,
    NametagBackground,
    HotbarOffset,
    TitleOffset,
    BossbarOffset,
    HideCoordinates,
    HideDayCounter,
    ScreenAnimations,
    Count
};

struct Info {
    const char* sig;
    const char* label;
    Kind kind;
};

const Info& info(Id id);
std::string sig(Id id);

void begin();
void apply();
void shutdown();

void set(Id id, float v);
void scale(Id id, float m);
void add(Id id, float a);
void force(Id id, bool on);
void setInt(Id id, int v);
void skip(Id id);
void smooth(Id id, float factor);
void out(Id id, std::initializer_list<float> values);
void transform(Id id, game::Vec3 move, game::Vec3 scale, game::Vec3 rotateDeg);

struct Report {
    bool requested = false;
    bool installed = false;
    float value = 0.f;
};
void ghost(uintptr_t actor, float seconds);
bool ghosted(uintptr_t actor);
int ghostCount();

Report report(Id id);
bool available(Id id);
// how often the game has run the hooked function so far
unsigned calls(Id id);

// For the module check: which module asked for an effect, and whether the game ran the code that carries it out.
// `changed` counts the runs in which the game's own result was replaced.
struct Proof {
    const char* sig = "";
    const char* label = "";
    const char* by = nullptr;
    bool found = false;
    bool installed = false;
    unsigned calls = 0;
    unsigned changed = 0;
    // carried out by pointing one of the game's options at Monchi's value: there is no call to count
    bool option = false;
};
void owner(const char* module);

// One of Minecraft's own options, by its number in the game's option list, held at "off" (a switch) or at its
// lowest value (a slider) for as long as it is asked for every frame; the player's saved setting is not touched and
// comes back when the asking stops. Names of the numbers: the offsets "option.<save name>" of the signature file.
enum class Hold { Off, Lowest };
void hold(const char* option, Hold how);
// how many options are held right now, and how many were asked for but are not known on this version
int held();
int heldMissing();
std::vector<Proof> proofs();

}
