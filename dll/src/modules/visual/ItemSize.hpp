#pragma once

#include "hook/ItemIcons.hpp"
#include "modules/Module.hpp"

class ItemSize : public Module {
public:
    ItemSize()
        : Module("Item Size", "Makes the item icons in your hotbar and inventories smaller or bigger, like GUI Scale just for items. Display only.",
                 Category::Visual, {"cosmetic"}) {
        sub("HUD parts");
        requireAny({"ItemIconRender"});
    }

    void onFrame() override { itemIcons::scale(icons_.f); }
    void onDisable() override { itemIcons::scale(1.f); }

private:
    Setting& icons_ = slider("icons", "Item icon size", 0.8f, 0.3f, 1.5f, "%.2fx");
};
