#pragma once

#include "I18n.hpp"
#include "Setting.hpp"
#include "core/Events.hpp"
#include "sdk/Effects.hpp"
#include "sdk/Game.hpp"

#include <imgui.h>
#include <json.hpp>

#include <atomic>
#include <deque>
#include <functional>
#include <string>
#include <vector>

enum class Category { Hud, Visual, Pvp, Comfort, Performance, Server, Fun, Client };

const char* categoryName(Category c);

enum class RuleLevel { Allowed, Warn, Block };

class Module {
public:
    Module(std::string name, std::string description, Category category, std::vector<std::string> tags = {},
           std::vector<std::string> sigs = {});
    virtual ~Module() = default;

    Module(const Module&) = delete;
    Module& operator=(const Module&) = delete;

    virtual void onEnable() {}
    virtual void onDisable() {}
    virtual void onFrame() {}
    virtual void onRender(ImDrawList*) {}
    virtual void onKey(KeyEvent&) {}
    virtual void onMouse(MouseEvent&) {}
    virtual void onServer(const ServerEvent&) {}
    // What the settings page shows live while the menu covers the screen: the whole screen with the module's place
    // marked, or the area around it enlarged. min and max are screen coordinates.
    enum class Preview { None, Marked, Zoomed };
    virtual Preview preview(ImVec2& min, ImVec2& max) const { return Preview::None; }
    // a module's own controls: the first above its settings list, for the choice everything else depends on
    virtual void drawSettingsTop() {}
    virtual void drawSettings() {}
    virtual bool isHud() const { return false; }
    // for the module check: what this module has counted of its own work in the game, empty when it counts nothing
    virtual std::string proof() const { return {}; }
    virtual bool alwaysOn() const { return false; }
    // key presses also while a game screen (inventory, chat) has the cursor
    virtual bool screenKeys() const { return false; }
    virtual bool persistent() const { return true; }
    virtual bool defaultEnabled() const { return false; }

    void setEnabled(bool on);
    void toggle() { setEnabled(!enabled_); }
    bool enabled() const { return enabled_; }
    bool userEnabled() const { return wanted_; }

    const std::string& name() const { return name_; }
    // the name before a rename: saved settings and server lists written with it still find the module
    virtual const char* formerName() const { return ""; }
    const std::string& description() const { return description_; }
    Category category() const { return category_; }
    const std::string& sub() const { return sub_; }
    const std::vector<std::string>& tags() const { return tags_; }
    const std::vector<std::string>& sigs() const { return sigs_; }
    bool hasTag(const std::string& t) const;

    bool available() const { return game::demo() || (missing_.empty() && unavailable_.empty() && game::ready(needs_)); }
    const std::vector<std::string>& missingSigs() const { return unavailable_.empty() ? missing_ : unavailable_; }
    void setUnavailable(std::vector<std::string> why) { unavailable_ = std::move(why); }
    void checkSigs();

    RuleLevel rule() const { return rule_; }
    std::string ruleNote() const { return ruleNote_.empty() && risky_ ? std::string(i18n::tr(riskNote_.c_str())) : ruleNote_; }
    void applyRule(RuleLevel level, std::string note);
    bool optionBlocked(const std::string& option) const;
    void setBlockedOptions(std::vector<std::string> opts) { blockedOptions_ = std::move(opts); }

    std::deque<Setting>& settings() { return settings_; }
    Setting& keybind() { return *key_; }
    Setting& hold() { return *hold_; }
    bool holdMode() const { return hold_ && hold_->b; }
    void publishBinding() { inputBinding_ = unsigned(key_->i) | (unsigned(holdMode()) << 16); }
    unsigned inputBinding() const { return inputBinding_.load(); }
    void captureDefaults();
    virtual void resetSettings(const std::function<bool(const Setting&)>& which);

    bool risky() const { return risky_; }
    bool anySigs() const { return anySig_; }
    bool favorite() const { return favorite_; }
    void setFavorite(bool on);
    bool parked() const { return parked_; }
    // Flarial's module of the same feature took over: off for good, hidden, and its name belongs to the stand-in
    bool replaced() const { return replaced_; }
    void replace();
    void setParked(bool on);

    virtual nlohmann::json save() const;
    virtual void load(const nlohmann::json& j);

    float anim = 0.f;
    float hover = 0.f;
    float costMs = 0.f;

protected:
    Setting& toggleSetting(std::string id, std::string label, bool def);
    // a setting that only works through one game hook stays hidden on versions without it
    static Setting& needs(Setting& s, fx::Id id);
    Setting& slider(std::string id, std::string label, float def, float min, float max, const char* fmt = "%.1f");
    Setting& intSlider(std::string id, std::string label, int def, int min, int max);
    Setting& colorSetting(std::string id, std::string label, ImVec4 def);
    Setting& choice(std::string id, std::string label, std::vector<std::string> options, int def = 0);
    Setting& keySetting(std::string id, std::string label, int def = 0);
    Setting& textSetting(std::string id, std::string label, std::string def);

    void markRisky(std::string note = "Not allowed on many servers. Only turn it on where the server rules allow it.") {
        risky_ = true;
        riskNote_ = std::move(note);
    }
    void sub(std::string name) { sub_ = std::move(name); }
    void moveTo(Category c) { category_ = c; }
    void needs(unsigned mask) { needs_ = mask; }
    void wants(unsigned mask) { wants_ = mask; }
    void needs(game::Domain d) { needs_ = unsigned(d); }
    void require(unsigned domains, std::vector<std::string> sigs) {
        needs_ = domains;
        sigs_ = std::move(sigs);
    }
    void requireAny(std::vector<std::string> sigs) {
        sigs_ = std::move(sigs);
        anySig_ = true;
    }

private:
    Setting& add(Setting s);

    std::string name_;
    std::string description_;
    Category category_;
    std::string sub_;
    std::vector<std::string> tags_;
    std::vector<std::string> sigs_;
    std::vector<std::string> missing_;
    std::vector<std::string> unavailable_;
    std::vector<std::string> blockedOptions_;
    std::deque<Setting> settings_;
    Setting* key_ = nullptr;
    Setting* hold_ = nullptr;
    std::vector<nlohmann::json> defaults_;
    unsigned needs_ = 0;
    unsigned wants_ = 0;
    bool anySig_ = false;
    std::atomic<bool> enabled_{false};
    std::atomic<unsigned> inputBinding_{0};
    bool wanted_ = false;
    bool risky_ = false;
    bool favorite_ = false;
    bool parked_ = false;
    bool replaced_ = false;
    RuleLevel rule_ = RuleLevel::Allowed;
    std::string ruleNote_;
    std::string riskNote_;
};
