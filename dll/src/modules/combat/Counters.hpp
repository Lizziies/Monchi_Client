#pragma once

#include "modules/common/Colors.hpp"
#include "modules/common/GameHud.hpp"
#include "modules/common/Needs.hpp"
#include "modules/common/Text.hpp"
#include "render/Ui.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <format>
#include <string>
#include <vector>

class ReachCounter : public GameText {
public:
    ReachCounter()
        : GameText("Reach Counter", "Shows the distance of your last hit, as average or best value.", need::combat,
                   need::sigs({"LocalPlayer"}), {"hud-self"}, {0.135f, 0.05f}) {
        sub("Combat displays");
        window_.visible = [this] { return mode_.i == 1; };
        warn_.visible = [this] { return colored_.b; };
    }

    void onFrame() override {
        auto& c = game::state().combat;
        if (c.reachCount < seen_) {
            seen_ = 0;
            recent_.clear();
            best_ = 0.f;
        }
        if (c.reachCount != seen_) {
            seen_ = c.reachCount;
            recent_.push_back(c.lastReach);
            if (recent_.size() > 10) recent_.erase(recent_.begin());
            best_ = std::max(best_, c.lastReach);
        }
        if (reset_.f > 0.f && !recent_.empty() && ui::time() - c.lastHitAt > reset_.f) {
            recent_.clear();
            best_ = 0.f;
        }
    }

    void onRender(ImDrawList* dl) override {
        auto& c = game::state().combat;
        if (idle_.f > 0.f && ui::time() - c.lastHitAt > idle_.f && !gui::editingHud()) return;
        GameText::onRender(dl);
    }

protected:
    std::string label() const override { return showLabel_.b ? "Reach" : ""; }

    std::string value() override {
        float v = 0.f;
        if (!recent_.empty()) {
            if (mode_.i == 0) {
                v = recent_.back();
            } else if (mode_.i == 1) {
                size_t n = std::min(recent_.size(), size_t(window_.i));
                for (size_t k = recent_.size() - n; k < recent_.size(); k++) v += recent_[k];
                v /= float(n);
            } else {
                v = best_;
            }
        }
        shown_ = v;
        return text::num(v, decimals_.i) + (unit_.b ? i18n::tr(" blocks") : "");
    }

    ImU32 valueColor() const override {
        if (!colored_.b) return textColor();
        return ImGui::GetColorU32(rampColor(shown_, 2.5f, warn_.f, good_.color, mid_.color, bad_.color));
    }

private:
    Setting& mode_ = choice("mode", "Value", {"Last hit", "Average", "Best value"});
    Setting& window_ = intSlider("window", "Hits in the average", 5, 2, 10);
    Setting& decimals_ = intSlider("decimals", "Decimals", 2, 0, 3);
    Setting& unit_ = toggleSetting("unit", "Show unit", false);
    Setting& reset_ = slider("reset", "Reset after (s, 0 = never)", 15.f, 0.f, 60.f, "%.0f s");
    Setting& idle_ = slider("idle", "Hide after (s, 0 = never)", 0.f, 0.f, 30.f, "%.0f s");
    Setting& colored_ = toggleSetting("colored", "Color by value", false);
    Setting& warn_ = slider("warn", "Red from (blocks)", 3.2f, 2.6f, 4.f, "%.1f");
    Setting& good_ = colorSetting("good", "Color low", {0.55f, 0.91f, 0.69f, 1.f});
    Setting& mid_ = colorSetting("mid", "Color medium", {1.f, 0.82f, 0.49f, 1.f});
    Setting& bad_ = colorSetting("bad", "Color high", {1.f, 0.4f, 0.45f, 1.f});
    mutable float shown_ = 0.f;
    std::vector<float> recent_;
    float best_ = 0.f;
    int seen_ = 0;
};

class OpponentReach : public GameText {
public:
    OpponentReach()
        : GameText("Opponent Reach", "Shows the distance from which the opponent last hit you.", need::combat | need::others,
                   need::sigs({"LocalPlayer", "ActorList"}), {"info-others"}, {0.135f, 0.082f}) {
        sub("Combat displays");
    }

    void onFrame() override {
        auto& st = game::state();
        for (auto& e : game::events()) {
            if (e.kind != game::EventKind::Hurt) continue;
            float r = attackerReach(st);
            if (r <= 0.f) continue;
            value_ = r;
            at_ = st.time;
        }
        if (reset_.f > 0.f && value_ > 0.f && st.time - at_ > reset_.f) value_ = 0.f;
    }

protected:
    std::string label() const override { return showLabel_.b ? i18n::tr("Opponent reach") : ""; }

    std::string value() override { return text::num(value_, decimals_.i) + (unit_.b ? i18n::tr(" blocks") : ""); }

private:
    float attackerReach(const game::State& st) const {
        const game::Other* from = nullptr;
        float nearest = 10.f;
        for (auto& o : st.others) {
            if (excludeTeam_.b && st.player.team && o.team == st.player.team) continue;
            float d = game::distance(o.pos, st.player.pos);
            if (d >= nearest) continue;
            nearest = d;
            from = &o;
        }
        if (!from) return 0.f;
        const auto& me = st.player.pos;
        float ex = from->pos.x, ey = from->pos.y + 1.62f, ez = from->pos.z;
        float cx = std::clamp(ex, me.x - 0.3f, me.x + 0.3f);
        float cy = std::clamp(ey, me.y, me.y + 1.8f);
        float cz = std::clamp(ez, me.z - 0.3f, me.z + 0.3f);
        float reach = std::sqrt((ex - cx) * (ex - cx) + (ey - cy) * (ey - cy) + (ez - cz) * (ez - cz));
        return reach <= 5.5f ? reach : 0.f;
    }

    Setting& decimals_ = intSlider("decimals", "Decimals", 2, 0, 3);
    Setting& unit_ = toggleSetting("unit", "Show unit", false);
    Setting& excludeTeam_ = toggleSetting("excludeTeam", "Try to exclude team", true);
    Setting& reset_ = slider("reset", "Reset after (s, 0 = never)", 15.f, 0.f, 60.f, "%.0f s");
    float value_ = 0.f;
    double at_ = 0.0;
};

class ComboCounter : public GameText {
public:
    ComboCounter()
        : GameText("Combo Counter", "Counts your hits in a row until the opponent hits you.", need::combat,
                   need::sigs({"LocalPlayer", "HurtEvents"}), {"hud-self"}, {0.135f, 0.114f}) {
        sub("Combat displays");
        timeout_.visible = [this] { return expire_.b; };
    }

    void onFrame() override {
        double now = game::state().time;
        for (auto& e : game::events()) {
            if (e.kind == game::EventKind::Hit) {
                if (e.crystal) continue;
                if (e.time - lastHit_ < gap_.f / 1000.f && e.text == target_) continue;
                lastHit_ = e.time;
                target_ = e.text;
                count_ = count_ < 0 ? 1 : count_ + 1;
                bestCount_ = std::max(bestCount_, count_);
                stamp_ = e.time;
            } else if (e.kind == game::EventKind::Hurt) {
                count_ = negatives_.b ? std::min(count_, 0) - 1 : 0;
                stamp_ = e.time;
            } else if (e.kind == game::EventKind::Death) {
                count_ = 0;
            }
        }
        if (expire_.b && count_ != 0 && now - stamp_ > timeout_.f) count_ = 0;
        if (count_ > shownLast_) pulse_ = 1.f;
        shownLast_ = count_;
        pulse_ = std::max(0.f, pulse_ - ui::dt() * 4.f);
    }

protected:
    std::string label() const override { return showLabel_.b ? "Combo" : ""; }

    std::string value() override {
        std::string v = std::to_string(count_);
        if (showBest_.b) v += i18n::fmt("  ·  Best {}", bestCount_);
        return v;
    }

    ImU32 valueColor() const override {
        if (!flash_.b) return textColor();
        return ImGui::GetColorU32(theme::mix(textColor_.color, flashColor_.color, pulse_));
    }

private:
    Setting& showBest_ = toggleSetting("best", "Show record", true);
    Setting& flash_ = toggleSetting("flash", "Flash on a new hit", true);
    Setting& flashColor_ = colorSetting("flashColor", "Flash color", {0.23f, 0.65f, 0.93f, 1.f});
    Setting& gap_ = slider("gap", "Minimum time between hits (ms)", 480.f, 100.f, 1000.f, "%.0f ms");
    Setting& negatives_ = toggleSetting("negatives", "Count to negatives", false);
    Setting& expire_ = toggleSetting("expire", "Reset after a pause", true);
    Setting& timeout_ = slider("timeout", "Reset after (s)", 15.f, 1.f, 60.f, "%.0f s");
    int count_ = 0;
    int bestCount_ = 0;
    int shownLast_ = 0;
    float pulse_ = 0.f;
    double lastHit_ = -100.0;
    double stamp_ = 0.0;
    std::string target_;
};

class HitCounter : public GameText {
public:
    HitCounter()
        : GameText("Hit Counter", "Hits, swings and hit rate of the current round.", need::combat,
                   need::sigs({"LocalPlayer"}), {"hud-self"}, {0.135f, 0.146f}) {
        sub("Combat displays");
        crits_.visible = [] { return need::have("MoveState"); };
    }

    void onKey(KeyEvent& ev) override {
        if (ev.down && !ev.repeat && ev.vk == reset_.i && reset_.i) resetPending_ = true;
    }

    void onFrame() override {
        if (resetPending_.exchange(false)) game::resetHitCounts();
        const auto& state = game::state();
        if (idleReset_.f > 0.f && state.combat.hits > 0 &&
            state.time - state.combat.lastHitAt >= idleReset_.f) game::resetHitCounts();
    }

    void onDisable() override { resetPending_ = false; }

protected:
    std::string label() const override { return i18n::tr("Hits"); }

    std::string value() override {
        auto& c = game::state().combat;
        std::string out = std::to_string(c.hits);
        if (swings_.b) out += std::format(" / {}", c.swings);
        if (accuracy_.b && c.swings > 0) out += std::format("  ·  {:.0f}%", 100.f * c.hits / c.swings);
        if (crits_.b && c.hits > 0) out += std::format("  ·  Crit {:.0f}%", 100.f * c.crits / c.hits);
        return out;
    }

private:
    std::atomic<bool> resetPending_{false};
    Setting& swings_ = toggleSetting("swings", "Show swings", true);
    Setting& accuracy_ = toggleSetting("accuracy", "Hit rate", true);
    Setting& crits_ = toggleSetting("crits", "Crit rate", false);
    Setting& reset_ = keySetting("reset", "Reset", 0);
    Setting& idleReset_ = slider("idleReset", "Reset after no hits (s, 0 = never)", 60.f, 0.f, 300.f, "%.0f s");
};

class HitPing : public GameText {
public:
    HitPing()
        : GameText("Hit Ping", "Estimated time from your click until the target loses health. Other damage can affect this measurement.", need::combat,
                   need::sigs({"LocalPlayer", "HitConfirm"}), {"info-others"}, {0.135f, 0.178f}) {
        sub("Combat displays");
    }

    void onFrame() override {
        double now = game::state().time;
        for (auto& e : game::events()) {
            if (e.kind != game::EventKind::Confirm) continue;
            if (e.time - at_ < gap_.f / 1000.f && e.text == target_) continue;
            ping_ = e.value;
            lastReach_ = e.reach;
            target_ = e.text;
            at_ = e.time;
        }
        if (reset_.f > 0.f && ping_ > 0.f && now - at_ > reset_.f) ping_ = 0.f;
    }

protected:
    std::string label() const override { return showLabel_.b ? i18n::tr("Hit ping") : ""; }

    std::string value() override {
        std::string out = std::format("{:.0f} ms", ping_);
        if (reach_.b && ping_ > 0.f) out += std::format("  ·  {}", text::num(lastReach_, 2));
        return out;
    }

private:
    Setting& reach_ = toggleSetting("reach", "Show reach", false);
    Setting& gap_ = slider("gap", "Ignore repeats within (ms)", 480.f, 100.f, 1000.f, "%.0f ms");
    Setting& reset_ = slider("reset", "Reset after (s, 0 = never)", 15.f, 0.f, 60.f, "%.0f s");
    float ping_ = 0.f;
    float lastReach_ = 0.f;
    double at_ = -100.0;
    std::string target_;
};

class SessionStats : public GameText {
public:
    SessionStats()
        : GameText("Session Stats", "Kills, deaths, K/D and kill streak of this session.", need::combat,
                   need::sigs({"LocalPlayer", "ChatEvents"}), {"hud-self"}, {0.135f, 0.21f}) {
        sub("Combat displays");
    }

    void onKey(KeyEvent& ev) override {
        if (ev.down && !ev.repeat && ev.vk == reset_.i && reset_.i) resetPending_ = true;
    }

    void onFrame() override { if (resetPending_.exchange(false)) game::resetCombat(); }
    void onDisable() override { resetPending_ = false; }

protected:
    std::string value() override {
        auto& c = game::state().combat;
        std::string out;
        auto metric = [&](const std::string& label, std::string value) {
            if (!out.empty()) out += separator_.text;
            if (!showLabel_.b || label.empty()) out += value;
            else if (labelSide_.i == 0) out += label + " " + value;
            else out += value + " " + label;
        };
        if (kills_.b) metric(killLabel_.text, std::to_string(c.kills));
        if (deaths_.b) metric(deathLabel_.text, std::to_string(c.deaths));
        if (kd_.b) metric(ratioLabel_.text, text::num(c.deaths ? float(c.kills) / c.deaths : float(c.kills), precision_.i));
        if (streak_.b) metric(i18n::tr("Streak"), showLabel_.b ? i18n::fmt("{} (best {})", c.streak, c.bestStreak) : std::format("{} ({})", c.streak, c.bestStreak));
        return out.empty() ? "–" : out;
    }

private:
    std::atomic<bool> resetPending_{false};
    Setting& killLabel_ = textSetting("killLabel", "Kill label", "K");
    Setting& deathLabel_ = textSetting("deathLabel", "Death label", "D");
    Setting& ratioLabel_ = textSetting("ratioLabel", "Ratio label", "K/D");
    Setting& separator_ = textSetting("separator", "Metric separator", "  ");
    Setting& precision_ = intSlider("precision", "Ratio decimals", 2, 0, 3);
    Setting& kills_ = toggleSetting("kills", "Kills", true);
    Setting& deaths_ = toggleSetting("deaths", "Deaths", true);
    Setting& kd_ = toggleSetting("kd", "K/D", true);
    Setting& streak_ = toggleSetting("streak", "Streak", true);
    Setting& reset_ = keySetting("reset", "Reset", 0);
};

class HitInfo : public GameText {
public:
    HitInfo()
        : GameText("Hit Info", "Shows whether your last hit was a critical hit, and the crit rate.", need::combat,
                   need::sigs({"LocalPlayer", "MoveState", "HurtEvents"}), {"hud-self"}, {0.135f, 0.242f}) {
        sub("Combat displays");
    }

protected:
    std::string value() override {
        auto& c = game::state().combat;
        if (!c.hits) return "–";
        std::string out = i18n::tr(c.lastCrit ? "Critical!" : "Normal");
        if (rate_.b) out += i18n::fmt("  ·  {:.0f}% crit", 100.f * c.crits / c.hits);
        if (damage_.b) out += i18n::fmt("  ·  {:.0f} damage", c.damageDealt);
        return out;
    }

    ImU32 valueColor() const override {
        return ImGui::GetColorU32(game::state().combat.lastCrit ? critColor_.color : textColor_.color);
    }

private:
    Setting& rate_ = toggleSetting("rate", "Crit rate", true);
    Setting& damage_ = toggleSetting("damage", "Total damage", false);
    Setting& critColor_ = colorSetting("critColor", "Color on crit", {0.23f, 0.65f, 0.93f, 1.f});
};

class EntityCounter : public GameText {
public:
    EntityCounter()
        : GameText("Entity Counter", "Counts entities and players around you.", need::world, need::sigs({"EntityList"}),
                   {"info-others"}, {0.135f, 0.274f}) {
        sub("Combat displays");
    }

protected:
    std::string label() const override { return "Entities"; }

    std::string value() override {
        auto& w = game::state().world;
        return players_.b ? i18n::fmt("{}  ·  {} players", w.entities, w.players) : std::to_string(w.entities);
    }

private:
    Setting& players_ = toggleSetting("players", "Also show players", true);
};
