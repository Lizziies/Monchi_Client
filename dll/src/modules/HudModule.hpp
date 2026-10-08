#pragma once

#include "Module.hpp"

#include <string>
#include <utility>
#include <vector>

namespace hud {
float globalScale();
void setGlobalScale(float s);
}

class HudModule : public Module {
public:
    HudModule(std::string name, std::string description, std::vector<std::string> tags = {"hud-self"},
              ImVec2 defaultPos = {0.01f, 0.1f});

    bool isHud() const override { return true; }
    void onRender(ImDrawList* dl) override;

    ImVec2 position() const;
    void setPosition(ImVec2 screenPos);
    ImVec2 size() const { return lastSize_; }
    Preview preview(ImVec2& min, ImVec2& max) const override {
        min = position();
        max = min + lastSize_;
        return Preview::Marked;
    }
    float scale() const { return scale_.f; }
    void setScale(float s);

protected:
    virtual ImVec2 pivot() const { return {0.f, 0.f}; }
    virtual bool autoPlace() const { return true; }
    virtual bool textLayout() const { return false; }
    // false for displays that draw no text at all: the text style settings are hidden for them
    virtual bool hasText() const { return true; }
    virtual ImVec4 backgroundColor() const { return bgColor_.color; }
    virtual ImVec2 content(ImDrawList* dl, ImVec2 origin, float scale) = 0;

    ImVec2 drawText(ImDrawList* dl, ImVec2 at, float scale, const std::string& text, ImU32 color);
    ImVec2 textSize(float scale, const std::string& text) const;
    ImU32 textColor() const;
    ImU32 accentColor() const;

    float textAlign() const { return float(align_.i) * 0.5f; }
    float minWidth(float scale) const { return minWidth_.f * scale; }

    Setting& background_;
    Setting& bgColor_;
    Setting& textColor_;
    Setting& useAccent_;
    Setting& rounding_;
    Setting& padding_;
    Setting& shadow_;
    Setting& padY_;
    Setting& shadowOffset_;
    Setting& align_;
    Setting& minWidth_;
    Setting& border_;
    Setting& borderColor_;
    Setting& borderWidth_;
    Setting& glow_;
    Setting& glowColor_;
    Setting& glowSize_;
    Setting& dropShadow_;
    Setting& dropShadowColor_;
    Setting& dropShadowSize_;
    Setting& blur_;
    Setting& blurRadius_;
    Setting& rotation_;
    Setting& placed_;

private:
    void makeRoom();

    Setting& x_;
    Setting& y_;
    Setting& scale_;
    ImVec2 defaultPos_;
    ImVec2 lastSize_{0, 0};
    ImVec2 shift_{0, 0};
};

class TextHud : public HudModule {
public:
    TextHud(std::string name, std::string description, std::vector<std::string> tags = {"hud-self"}, ImVec2 defaultPos = {0.01f, 0.1f})
        : HudModule(std::move(name), std::move(description), std::move(tags), defaultPos) {
        showLabel_.style = labelSide_.style = labelGap_.style = true;
        labelText_.visible = [this] { return showLabel_.b; };
        labelSide_.visible = labelGap_.visible = [this] { return showLabel_.b && format_.text.empty(); };
        format_.hint = "A custom format determines the label position and spacing.";
        format_.prefill = "{label} {value}";
    }

protected:
    bool textLayout() const override { return true; }
    virtual std::string label() const { return ""; }
    virtual std::string value() = 0;
    virtual void tokens(std::vector<std::pair<std::string, std::string>>&) {}
    Setting& format_ = textSetting("format", "Format ({label} {value})", "");
    Setting& showLabel_ = toggleSetting("label", "Show label", true);
    Setting& labelText_ = textSetting("labelText", "Custom label (empty uses default)", "");
    Setting& labelSide_ = choice("labelSide", "Label position", {"Before value", "After value"});
    Setting& labelGap_ = slider("labelGap", "Label gap", 4.f, 0.f, 32.f, "%.0f");
    virtual ImU32 valueColor() const { return textColor(); }
    ImVec2 content(ImDrawList* dl, ImVec2 origin, float scale) override;
};
