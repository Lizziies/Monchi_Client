#pragma once

#include "modules/Module.hpp"
#include "modules/common/Nick.hpp"

class Nick : public Module {
public:
    Nick()
        : Module("Nick", "Shows your own name as a nick in Monchi's chat, tab list and stats. Only you see it, other players never do.", Category::Comfort,
                 {"cosmetic"}) {
        sub("Chat");
    }

    void onFrame() override {
        nick::on = true;
        nick::name = name_.text;
        nick::color = colorOn_.b ? color_.i : -1;
        nick::bold = bold_.b;
        nick::obfuscated = obfuscated_.b;
    }

    void onDisable() override { nick::on = false; }

private:
    Setting& name_ = textSetting("name", "Nick", "Monchi Player");
    Setting& colorOn_ = toggleSetting("colorOn", "Colored name", true);
    Setting& color_ = choice("color", "Color", {"White", "Gray", "Black", "Red", "Dark red", "Orange", "Gold", "Yellow", "Lime", "Green", "Dark green", "Aqua",
                                                "Cyan", "Light blue", "Blue", "Dark blue", "Purple", "Violet", "Magenta", "Pink", "Hot pink", "Brown", "Salmon",
                                                "Mint", "Silver"}, 19);
    Setting& bold_ = toggleSetting("bold", "Bold", false);
    Setting& obfuscated_ = toggleSetting("obfuscated", "Obfuscated (random letters)", false);
};
