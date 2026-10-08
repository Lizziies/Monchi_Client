// SPDX-License-Identifier: AGPL-3.0-only
#include "WorldMesh.hpp"

#include "SDK/Client/Core/HashedString.hpp"
#include "SDK/Client/Render/ResourceLocation.hpp"
#include "Utils/Logger/Logger.hpp"
#include "Utils/Memory/Game/SignatureAndOffsetManager.hpp"

#include <windows.h>

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace {

// Everything here is read from the game image of 1.26.52.3 (static). Flarial's own helpers for this do not fit that
// build any more, which is why none of them is used:
//
// renderLevel (0x47f8570) takes (level renderer, screen context, ...). renderer+0x408 is the client instance, whose
//   slot 79 gives the object the renderer hands on as MinecraftGame; renderer+0x468 is the player's renderer with the
//   camera position at +0x654 (the camera target follows at +0x660, the function turns the two into the view
//   direction). The level is drawn relative to that position.
// ScreenRenderer::blit (0x1587c10) is the pattern for a textured quad: tessellator = screen context+0xb8,
//   begin(tess, -, mode 1 = quads, vertex count, false), then per vertex the texture coordinate straight into the
//   tessellator (+0x18c, +0x190, marks at +0x194 and, while byte 0 of the tessellator is clear, +0x15c) and
//   vertex(tess, x, y, z). There is no vertexUV function left; the setter at 0x59ca4a0 writes another channel.
// The material comes from MaterialPtr(out, group, HashedString const&) at 0x52f1e00, called at 0x8db9859 with the ui
//   group; the group's slot 1 now takes (group, out, name), so Flarial's call of it with (group, name) would go wrong.
// renderMeshImmediately2 (0x46b95f0) takes the texture as a variant: its callers build {TexturePtr, tag byte 1 at
//   +0x20} on their stack, having raised both reference counts, and let go of it by the tag afterwards.
//   The fifth argument is a variant as well, tag byte at +0x20, and the callers take apart what the call left in
//   it (blit at 0x1588379, table 0xe7bb87c): tag 1 owns a 0x20 byte object (a control block at +8 loses a weak count,
//   then the object goes through the game's sized delete 0xe021490), tags 2 and 3 hold a shared_ptr whose control
//   block is at +0x18. Handing over scratch memory and never looking at it again kept every drawn mesh alive: the
//   game grew to 17 GB within minutes of third person and aborted.
// TextureGroup::getTexture (0x1d0cce0) is (base, out TexturePtr, ResourceLocation const&, bool, optional<uint>, type)
//   with base = texture group+0x18; MinecraftGame holds the group as a shared_ptr. Where exactly is checked at run
//   time: the pointer only counts when the vtable behind base+0 lists this very function.
constexpr int tessellatorAt = 0xB8, instanceAt = 0x408, playerRendererAt = 0x468, cameraAt = 0x654, gameSlot = 79;
constexpr int uvAt = 0x18C, uvMarkAt = 0x194, formatMarkAt = 0x15C;
constexpr int groupAt = 0x6B0, groupBaseAt = 0x18;

struct Vertex {
    float x, y, z, u, v;
    float color[4];
};

struct Batch {
    std::string texture;
    std::vector<Vertex> vertices;
};

struct TextureRef {
    void *words[4]{};
    bool asked = false;
    bool refreshed = false;
    ULONGLONG at = 0;
};

struct TextureArg {
    void *words[4];
    unsigned char tag;
};

std::mutex lock;
std::shared_ptr<const std::vector<Batch>> pending;
ULONGLONG submitted = 0;
std::atomic<int> state{0};
std::atomic<int> place{1};
void *currentRenderer = nullptr;
bool drawnThisFrame = false, tagsLastFrame = false, tagsThisFrame = false;
bool off = false;

void *material[2]{};
bool materialAsked = false;
std::map<std::string, TextureRef> textures;
void* textureGroup = nullptr;
std::atomic<bool> resetTextures{false};

using Begin = void (*)(void *, int, unsigned char, int, bool);
using Color = void (*)(void *, const float *);
using Point = void (*)(void *, float, float, float);
using Render = void (*)(void *, void *, void *, TextureArg *, void *);
using MakeMaterial = void (*)(void *, void *, const HashedString *);
using GetTexture = void *(*)(void *, void *, const ResourceLocation *, bool, unsigned long long, int);
using Delete = void (*)(void *, size_t);

struct Result {
    void *words[4];
    unsigned char tag;
    unsigned char rest[0x58 - 0x21];
};

std::atomic<int> leftTag{-1};

struct Calls {
    Begin begin = nullptr;
    Color color = nullptr;
    Point vertex = nullptr;
    Render render = nullptr;
    MakeMaterial makeMaterial = nullptr;
    void *group = nullptr;
    uintptr_t getTexture = 0;
    Delete free = nullptr;
    bool complete = false;
};

Calls resolve() {
    Calls c;
    c.begin = reinterpret_cast<Begin>(GET_SIG_ADDRESS("world::begin"));
    c.color = reinterpret_cast<Color>(GET_SIG_ADDRESS("world::color"));
    c.vertex = reinterpret_cast<Point>(GET_SIG_ADDRESS("world::vertex"));
    c.render = reinterpret_cast<Render>(GET_SIG_ADDRESS("world::render"));
    c.getTexture = GET_SIG_ADDRESS("mce::TextureGroup::getTexture");
    // lea rdx, [group]; lea rcx, [out]; lea r8, [name]; call MaterialPtr(out, group, name)
    if (auto site = reinterpret_cast<const unsigned char *>(GET_SIG_ADDRESS("world::materialSite")); site && site[0x12] == 0xE8) {
        c.group = const_cast<unsigned char *>(site + 7 + *reinterpret_cast<const int *>(site + 3));
        c.makeMaterial = reinterpret_cast<MakeMaterial>(site + 0x17 + *reinterpret_cast<const int *>(site + 0x13));
    }
    if (auto assign = reinterpret_cast<const unsigned char *>(GET_SIG_ADDRESS("world::stringAssign")); assign && assign[0x110] == 0xE8)
        c.free = reinterpret_cast<Delete>(const_cast<unsigned char *>(assign + 0x115 + *reinterpret_cast<const int *>(assign + 0x111)));
    c.complete = c.begin && c.color && c.vertex && c.render && c.getTexture && c.group && c.makeMaterial && c.free;
    return c;
}

bool makeMaterialRaw(const Calls &c, const HashedString *name) {
    __try {
        c.makeMaterial(material, c.group, name);
        return material[0] != nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// the texture group out of the game object, accepted only when its vtable holds the lookup this build was read for
void *groupBaseRaw(void *renderer, uintptr_t lookup) {
    __try {
        auto instance = *reinterpret_cast<uintptr_t **>(reinterpret_cast<uintptr_t>(renderer) + instanceAt);
        if (!instance) return nullptr;
        auto game = reinterpret_cast<uintptr_t (*)(void *)>(reinterpret_cast<uintptr_t *>(*instance)[gameSlot])(instance);
        if (!game) return nullptr;
        auto group = *reinterpret_cast<uintptr_t *>(game + groupAt);
        if (!group) return nullptr;
        auto table = *reinterpret_cast<uintptr_t **>(group + groupBaseAt);
        for (int i = 0; i < 48; i++)
            if (table[i] == lookup) return reinterpret_cast<void *>(group + groupBaseAt);
        return nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

bool getTextureRaw(uintptr_t lookup, void *base, TextureRef *out, const ResourceLocation *where, bool force = false) {
    __try {
        reinterpret_cast<GetTexture>(lookup)(base, out->words, where, force, 0, 0);
        return out->words[0] != nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool releaseTextureRaw(TextureRef *texture) {
    __try {
        for (int k = 1; k < 4; k += 2) {
            auto* block = texture->words[k];
            if (!block) continue;
            auto counts = reinterpret_cast<volatile LONG*>(reinterpret_cast<uintptr_t>(block) + 8);
            auto slots = *reinterpret_cast<void (***)(void*)>(block);
            if (InterlockedDecrement(counts) == 0) {
                slots[0](block);
                if (InterlockedDecrement(counts + 1) == 0) slots[1](block);
            }
        }
        *texture = {};
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

bool cameraRaw(void *renderer, float *out) {
    __try {
        auto player = *reinterpret_cast<uintptr_t *>(reinterpret_cast<uintptr_t>(renderer) + playerRendererAt);
        if (!player) return false;
        auto at = reinterpret_cast<const float *>(player + cameraAt);
        out[0] = at[0];
        out[1] = at[1];
        out[2] = at[2];
        return out[0] == out[0] && out[1] == out[1] && out[2] == out[2];
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void reportTint(const float *saved) {
    static bool reported = false;
    if (!reported) {
        reported = true;
        Logger::info("world cosmetics: neutral shader tint, inherited rgba {} {} {} {}", saved[0], saved[1], saved[2], saved[3]);
    }
}

// Adapted from Flarial Utils/Render/DrawUtil3D.cpp: neutralize the shared shader tint for custom geometry.
// ScreenRenderer::blit in 1.26.52.3 reads this pointer at 0x1587c91 from screenContext+0x30.
void renderUntinted(const Calls &c, void *screenContext, void *tess, TextureArg *arg, Result *result) {
    auto *color = *reinterpret_cast<float **>(static_cast<unsigned char *>(screenContext) + 0x30);
    float saved[4] = {color[0], color[1], color[2], color[3]};
    reportTint(saved);
    color[0] = color[1] = color[2] = color[3] = 1.f;
    __try {
        c.render(screenContext, tess, material, arg, result);
    } __finally {
        for (int i = 0; i < 4; ++i) color[i] = saved[i];
    }
}

// the reference counts go up for the copy the game is handed and down again when it has left the copy alone, the way
// the game's own callers hold theirs
bool drawRaw(const Calls &c, void *screenContext, const Vertex *vertices, int count, const float *camera, const TextureRef *texture) {
    __try {
        auto tess = *reinterpret_cast<unsigned char **>(reinterpret_cast<uintptr_t>(screenContext) + tessellatorAt);
        if (!tess) return false;
        c.begin(tess, 0, 1, count, false);
        for (int i = 0; i < count; i++) {
            const Vertex &v = vertices[i];
            c.color(tess, v.color);
            *reinterpret_cast<float *>(tess + uvAt) = v.u;
            if (!tess[uvMarkAt]) tess[uvMarkAt] = 1;
            *reinterpret_cast<float *>(tess + uvAt + 4) = v.v;
            if (!tess[0]) tess[formatMarkAt] = 1;
            c.vertex(tess, v.x - camera[0], v.y - camera[1], v.z - camera[2]);
        }
        TextureArg arg{{texture->words[0], texture->words[1], texture->words[2], texture->words[3]}, 1};
        for (int k = 1; k < 4; k += 2)
            if (arg.words[k]) InterlockedIncrement(reinterpret_cast<volatile LONG *>(reinterpret_cast<uintptr_t>(arg.words[k]) + 8));
        Result result{};
        renderUntinted(c, screenContext, tess, &arg, &result);
        leftTag = result.tag;
        // a kind of result this was never taught to give back: stop here instead of guessing what to free
        if (result.tag != 0 && result.tag != 1 && result.tag != 2 && result.tag != 3) return false;
        // what a shared_ptr's control block does when a count reaches zero: slot 0 destroys the object, slot 1 the block
        auto drop = [](void *block, bool strong) {
            auto counts = reinterpret_cast<volatile LONG *>(reinterpret_cast<uintptr_t>(block) + 8);
            auto slots = *reinterpret_cast<void (***)(void *)>(block);
            if (strong) {
                if (InterlockedDecrement(counts) != 0) return;
                slots[0](block);
            }
            if (InterlockedDecrement(counts + 1) == 0) slots[1](block);
        };
        if (result.tag == 1) {
            if (auto owned = reinterpret_cast<void **>(result.words[0])) {
                if (owned[1]) drop(owned[1], false);
                c.free(owned, 0x20);
            }
        } else if (result.tag == 2 || result.tag == 3) {
            if (result.words[3]) drop(result.words[3], true);
        }
        for (int k = 1; k < 4; k += 2)
            if (arg.words[k] && arg.words[k] == texture->words[k])
                InterlockedDecrement(reinterpret_cast<volatile LONG *>(reinterpret_cast<uintptr_t>(arg.words[k]) + 8));
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void say(int what, const char *text) {
    static int told = 0;
    if (told == what) return;
    told = what;
    Logger::info("world cosmetics: {}", text);
}

}

void drawNow(void *levelRenderer, void *screenContext);

void monchiWorld::draw(void *levelRenderer, void *screenContext, bool after) {
    int where = place.load();
    if (!after) {
        currentRenderer = levelRenderer;
        tagsLastFrame = tagsThisFrame;
        tagsThisFrame = false;
        drawnThisFrame = false;
        // without a name tag on screen there is no later place inside the level; then the start has to do
        if (where == 0 || (where == 1 && !tagsLastFrame)) drawNow(levelRenderer, screenContext);
        return;
    }
    currentRenderer = nullptr;
    if (where == 2) drawNow(levelRenderer, screenContext);
}

void monchiWorld::atNameTags(void *screenContext) {
    tagsThisFrame = true;
    if (place.load() != 1 || drawnThisFrame || !currentRenderer || !tagsLastFrame) return;
    drawNow(currentRenderer, screenContext);
}

void drawNow(void *levelRenderer, void *screenContext) {
    if (off || !levelRenderer || !screenContext || drawnThisFrame) return;
    drawnThisFrame = true;
    std::shared_ptr<const std::vector<Batch>> todo;
    {
        std::scoped_lock guard(lock);
        if (!pending || pending->empty()) return;
        if (GetTickCount64() - submitted > 300) {
            pending.reset();
            return;
        }
        todo = pending;
    }
    static const Calls calls = resolve();
    if (!calls.complete) {
        state = 2;
        static bool listed = false;
        if (!listed) {
            listed = true;
            Logger::warn("world cosmetics: missing on this version: begin {} color {} vertex {} render {} texture {} material {} free {}", calls.begin != nullptr,
                         calls.color != nullptr, calls.vertex != nullptr, calls.render != nullptr, calls.getTexture != 0, calls.makeMaterial != nullptr,
                         calls.free != nullptr);
        }
        return;
    }
    if (!materialAsked) {
        materialAsked = true;
        HashedString name("name_tag_depth_tested");
        if (!makeMaterialRaw(calls, &name)) {
            off = true;
            state = 2;
            return say(2, "the game gave no material, switched off for this session");
        }
    }
    float camera[3];
    if (!cameraRaw(levelRenderer, camera)) return say(3, "the camera position could not be read");

    void* base = groupBaseRaw(levelRenderer, calls.getTexture);
    if (!base) return say(4, "the texture group could not be read");
    bool reset = resetTextures.exchange(false);
    if (textureGroup != base || reset) {
        for (auto& [path, texture] : textures) releaseTextureRaw(&texture);
        textures.clear();
        textureGroup = base;
        Logger::info("world cosmetics: texture cache reset for the current world resource group");
    }
    ULONGLONG now = GetTickCount64();
    bool drew = false;
    for (const auto &batch : *todo) {
        if (batch.vertices.empty()) continue;
        TextureRef &texture = textures[batch.texture];
        if (!texture.asked) {
            texture.asked = true;
            texture.at = now;
            ResourceLocation where(batch.texture, true);
            if (!getTextureRaw(calls.getTexture, base, &texture, &where, true)) Logger::warn("world cosmetics: the game did not take the texture {}", batch.texture);
        }
        // the game reads the file on another thread; until then it would draw its placeholder
        if (now - texture.at < 400) continue;
        if (!texture.refreshed) {
            releaseTextureRaw(&texture);
            ResourceLocation where(batch.texture, true);
            getTextureRaw(calls.getTexture, base, &texture, &where);
            texture.asked = texture.refreshed = true;
        }
        if (!texture.words[0]) continue;
        if (!drawRaw(calls, screenContext, batch.vertices.data(), int(batch.vertices.size()), camera, &texture)) {
            off = true;
            state = 2;
            return say(5, "a call into the game's drawing faulted, switched off for this session");
        }
        drew = true;
    }
    if (drew) {
        state = 1;
        say(6, "the game draws the cosmetics in the world");
        static int toldTag = -2;
        if (int tag = leftTag.load(); tag != toldTag) {
            toldTag = tag;
            Logger::info("world cosmetics: a mesh draw leaves result kind {} behind, released", tag);
        }
    }
}

extern "C" __declspec(dllexport) void monchiFlarialWorldMesh(const MonchiWorldBatch *batches, int count) {
    std::vector<Batch> next;
    for (int i = 0; batches && i < count; i++) {
        const MonchiWorldBatch &in = batches[i];
        if (!in.texture || !in.vertices || in.count < 4) continue;
        Batch batch;
        batch.texture = in.texture;
        batch.vertices.reserve(size_t(in.count));
        for (int k = 0; k + 3 < in.count; k += 4)
            for (int j = 0; j < 4; j++) {
                const MonchiWorldVertex &v = in.vertices[k + j];
                batch.vertices.push_back({v.x, v.y, v.z, v.u, v.v,
                                          {float(v.color & 255) / 255.f, float((v.color >> 8) & 255) / 255.f, float((v.color >> 16) & 255) / 255.f,
                                           float(v.color >> 24) / 255.f}});
            }
        next.push_back(std::move(batch));
    }
    auto snapshot = std::make_shared<const std::vector<Batch>>(std::move(next));
    // Nothing to draw is not a reason to forget the textures: in first person the own cosmetics are not drawn, and
    // every switch back to third person then waited again for the textures to load (seen as cosmetics that take a
    // moment to come back). They are only asked for anew after a long gap, when a pack may have changed.
    static ULONGLONG lastDrawn = 0;
    ULONGLONG tick = GetTickCount64();
    if (!snapshot->empty()) {
        if (lastDrawn && tick - lastDrawn > 120000) resetTextures = true;
        lastDrawn = tick;
    }
    std::scoped_lock guard(lock);
    pending = std::move(snapshot);
    submitted = GetTickCount64();
}

extern "C" __declspec(dllexport) int monchiFlarialWorldMeshState() { return state.load(); }

extern "C" __declspec(dllexport) void monchiFlarialWorldMeshPlace(int where) { place = where; }
