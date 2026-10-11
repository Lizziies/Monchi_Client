#pragma once
#include "modules/Module.hpp"
#include "modules/flarial/FlarialModules.hpp"

// The scale is what GuiData::calculateGuiScale returns (fx.guiScale). The game only asks for it when the window or its
// own option changes, so every change here is followed by a request to compute it again. Right after the start the
// request can go nowhere (the core that carries it out is loaded later, the game has no screen yet), so it is asked
// again until the game has really run the function.
class McGuiScale : public Module {
public:
    McGuiScale() : Module("MC GUI Scale", "Adjust the Minecraft interface scale.", Category::Visual, {"size"}, {"fx.guiScale"}) {}
    void onEnable() override { last_ = 0.f; }
    void onDisable() override { flarialModules::refreshScreen(); }
    void onFrame() override {
        fx::set(fx::Id::GuiScale, scale_.f);
        unsigned calls = fx::calls(fx::Id::GuiScale);
        if (last_ != scale_.f) {
            last_ = scale_.f;
            pending_ = true;
            asked_ = 0.0;
        }
        if (!pending_) return;
        double now = ImGui::GetTime();
        if (asked_ > 0.0 && calls != seen_) {
            pending_ = false;
            if (!told_) logger::info("gui scale: applied at {:.2f} without switching the module", scale_.f);
            told_ = true;
            return;
        }
        if (now - asked_ < 0.5) return;
        if (!flarialModules::refreshScreen()) return;
        seen_ = calls;
        asked_ = now;
    }
private:
    Setting& scale_ = slider("guiscale", "UI Scale", 2.f, 1.f, 4.f, "%.2f");
    float last_ = 0.f;
    bool pending_ = false, told_ = false;
    unsigned seen_ = 0;
    double asked_ = 0.0;
};
