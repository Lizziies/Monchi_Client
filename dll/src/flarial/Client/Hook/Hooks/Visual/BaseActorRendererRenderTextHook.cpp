// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Hook/Hooks/Visual/BaseActorRendererRenderTextHook.cpp: the address is the callee of the
// call at BaseActorRenderer::renderText inside 0x46ba370; the game passes (context, view data, tag object, font) in
// registers and one more value on the stack, all five are handed on.
#include "BaseActorRendererRenderTextHook.hpp"

#include "Bridge/NameStyles.hpp"
#include "Bridge/WorldMesh.hpp"
#include "Client.hpp"
#include "Events/Render/DrawNameTagEvent.hpp"

#include <cstring>

namespace {

// A Monchi user's tag is drawn as a second name tag one line above the game's own, which stays untouched. The second
// one is a byte copy of the game's tag object with our text lent to it (the string objects are swapped byte for byte,
// nothing of ours is ever freed by the game), without the background the game built for the name's width.
struct Extra {
    unsigned char bytes[sizeof(NameTagRenderObject)];
    std::string text;

    NameTagRenderObject *make(const NameTagRenderObject &from, const monchiNames::Look &look) {
        text = look.prefix;
        // The game gets a byte copy of this string. Up to fifteen bytes a std::string holds its text inside itself,
        // so the copy owns nothing; a longer one would point into this module's heap, and if the game ever moved
        // from it, it would free that with its own allocator. Longer tags are cut at a character boundary.
        if (text.size() > 15) {
            size_t cut = 15;
            while (cut > 0 && (static_cast<unsigned char>(text[cut]) & 0xC0) == 0x80) cut--;
            std::string(text, 0, cut).swap(text);
        }
        std::memcpy(bytes, &from, sizeof(bytes));
        auto tag = reinterpret_cast<NameTagRenderObject *>(bytes);
        std::memcpy(static_cast<void *>(&tag->nameTag), &text, sizeof(std::string));
        std::memset(static_cast<void *>(&tag->mesh), 0, sizeof(tag->mesh));
        tag->textColor.r = look.color[0];
        tag->textColor.g = look.color[1];
        tag->textColor.b = look.color[2];
        // a line of name tag text is ten units of 0.0267 blocks
        tag->pos.y += 0.27f * (tag->scale > 0.1f && tag->scale < 8.f ? tag->scale : 1.f);
        return tag;
    }
};

}

void BaseActorRendererRenderTextHook::callback(ScreenContext* screenContext, ViewRenderData* viewData,
    NameTagRenderObject* tagData, Font* font, void* mesh)
{
    monchiWorld::atNameTags(screenContext);
    const auto background = tagData->tagColor;
    const auto text = tagData->textColor;
    auto event = nes::make_holder<DrawNameTagEvent>(tagData);
    eventMgr.trigger(event);

    funcOriginal(screenContext, viewData, tagData, font, mesh);
    tagData->tagColor = background;
    tagData->textColor = text;

    if (monchiNames::Look look; monchiNames::find(tagData->nameTag, look) && !look.prefix.empty()) {
        Extra extra;
        funcOriginal(screenContext, viewData, extra.make(*tagData, look), font, mesh);
        static bool told = false;
        if (!told) {
            told = true;
            Logger::info("name tags: a Monchi user's tag is drawn above their name");
        }
    }
}

BaseActorRendererRenderTextHook::BaseActorRendererRenderTextHook(): Hook("BaseActorRenderer renderText Hook", GET_SIG_ADDRESS("BaseActorRenderer::renderText"))
{}

void BaseActorRendererRenderTextHook::enableHook()
{
    if (!address || *reinterpret_cast<const uint8_t*>(address) != 0xE8) {
        Logger::warn("BaseActorRenderer::renderText is not a call site, DrawNameTagEvent stays off");
        return;
    }
    this->manualHook(reinterpret_cast<void*>(Memory::offsetFromSig(address, 1)), reinterpret_cast<void*>(callback), reinterpret_cast<void**>(&funcOriginal));
}
