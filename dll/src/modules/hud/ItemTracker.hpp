#pragma once

#include "I18n.hpp"
#include "gui/Gui.hpp"
#include "modules/common/GameHud.hpp"
#include "modules/common/Needs.hpp"
#include "modules/common/Text.hpp"
#include "render/Ui.hpp"
#include "sdk/Game.hpp"

#include <algorithm>
#include <format>
#include <map>
#include <string>
#include <vector>

class ItemTracker : public GameList {
public:
    ItemTracker()
        : GameList("Item Tracker", "Shows what you pick up and lose for a few seconds, for example +3 Iron Ingot or -1 Ender Pearl.", need::inventory,
                   need::sigs({"LocalPlayer"}), {"hud-self"}, {0.845f, 0.66f}) {
        sub("Inventory info");
    }

    void onEnable() override {
        counts_.clear();
        changes_.clear();
        primed_ = false;
        slots_.clear();
    }

    void onFrame() override {
        if (!game::state().inWorld) {
            primed_ = false;
            changes_.clear();
            return;
        }
        double t = ui::time();
        std::erase_if(changes_, [&](const Change& c) { return t - c.at > keep_.f; });
        size_t slot = 0;
        bool changed = !primed_;
        visit([&](const game::Item& it) {
            if (slot >= slots_.size()) { slots_.push_back({it.name, it.count}); changed = true; }
            else if (slots_[slot].first != it.name || slots_[slot].second != it.count) {
                slots_[slot] = {it.name, it.count};
                changed = true;
            }
            ++slot;
        });
        if (slot != slots_.size()) { slots_.resize(slot); changed = true; }
        if (!changed) return;
        auto now = totals();
        if (primed_) {
            for (auto& [name, n] : now) note(name, n - (counts_.count(name) ? counts_[name] : 0), t);
            for (auto& [name, n] : counts_)
                if (!now.count(name)) note(name, -n, t);
        }
        counts_ = std::move(now);
        primed_ = true;
    }

    void onRender(ImDrawList* dl) override {
        if (changes_.empty() && !gui::editingHud()) return;
        GameList::onRender(dl);
    }

protected:
    ImVec2 content(ImDrawList* dl, ImVec2 o, float s) override {
        if (changes_.empty()) return drawText(dl, o, s, "+3 " + std::string(i18n::tr("Iron Ingot")), ImGui::GetColorU32(gain_.color));
        double t = ui::time();
        float w = 0.f, y = 0.f;
        for (auto& c : changes_) {
            float fade = std::clamp(float(keep_.f - (t - c.at)) / 0.4f, 0.f, 1.f);
            ImVec4 col = c.delta > 0 ? gain_.color : loss_.color;
            col.w *= fade;
            std::string amount = std::format("{}{}", c.delta > 0 && plus_.b ? "+" : "", c.delta);
            std::string name = showName_.b ? text::pretty(c.name) : "";
            std::string value = name.empty() ? amount : nameSide_.i == 0 ? name + separator_.text + amount : amount + separator_.text + name;
            auto sz = drawText(dl, o + ImVec2(0, y), s, value, ImGui::GetColorU32(col));
            w = std::max(w, sz.x);
            y += sz.y;
        }
        return {w, y};
    }

private:
    struct Change {
        std::string name;
        int delta;
        double at;
    };

    template<class F> static void visit(F&& add) {
        auto& p = game::state().player;
        for (auto& it : p.hotbar) add(it);
        for (auto& it : p.armor) add(it);
        for (auto& it : p.main) add(it);
        add(p.offhand);
    }
    static std::map<std::string, int> totals() {
        std::map<std::string, int> out;
        visit([&](const game::Item& it) { if (!it.empty()) out[it.name] += it.count; });
        return out;
    }

    // picking up a stack arrives over a few ticks, so changes of the same item close together are merged
    void note(const std::string& name, int delta, double t) {
        if (delta == 0) return;
        for (auto it = changes_.rbegin(); it != changes_.rend(); ++it) {
            if (it->name != name || t - it->at > 1.0) continue;
            it->delta += delta;
            it->at = t;
            if (it->delta == 0) changes_.erase(std::next(it).base());
            return;
        }
        changes_.push_back({name, delta, t});
        if (changes_.size() > size_t(lines_.i)) changes_.erase(changes_.begin());
    }

    Setting& keep_ = slider("keep", "Show for (s)", 3.f, 1.f, 10.f, "%.1f s");
    Setting& showName_ = toggleSetting("showName", "Show item name", true);
    Setting& nameSide_ = choice("nameSide", "Label position", {"Before value", "After value"}, 1);
    Setting& separator_ = textSetting("separator", "Separator", " ");
    Setting& plus_ = toggleSetting("plus", "Show plus sign", true);
    Setting& lines_ = intSlider("lines", "Lines at most", 5, 1, 12);
    Setting& gain_ = colorSetting("gain", "Picked up", {0.55f, 0.91f, 0.69f, 1.f});
    Setting& loss_ = colorSetting("loss", "Lost", {1.f, 0.4f, 0.45f, 1.f});
    std::map<std::string, int> counts_;
    std::vector<std::pair<std::string, int>> slots_;
    std::vector<Change> changes_;
    bool primed_ = false;
};
