// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/SDK/Client/Render/ScreenView/UIControl.hpp: updatePosition falls back to _setCachedPosition when
// getPosition is missing (1.26.52 has only the older recompute function, cached position at +0x10, dirty flag at +0x18).
#pragma once

#include "UIComponent.hpp"
#include <Utils/VersionUtils.hpp>

class UIControl {
public:
    BUILD_ACCESS(this, Vec2<float>, sizeConstrains, GET_OFFSET("UIControl::sizeConstrains"));
    BUILD_ACCESS(this, Vec2<float>, parentRelativePosition, GET_OFFSET("UIControl::parentRelativePosition"));
    BUILD_ACCESS(this, std::vector<std::shared_ptr<UIControl>>, children, GET_OFFSET("UIControl::children"));

    std::string& getLayerName() {
        return hat::member_at<std::string>(this, GET_OFFSET("UIControl::LayerName"));
    }

    std::vector<std::unique_ptr<UIComponent>>& getComponents() {
        return hat::member_at<std::vector<std::unique_ptr<UIComponent>>>(this, GET_OFFSET("UIControl::components"));
    }

    void updatePosition(bool override = false) {
        auto getPosition = reinterpret_cast<Vec2<float>*(__fastcall*)(UIControl*)>(GET_SIG_ADDRESS("UIControl::getPosition"));
        auto setCachedPosition = reinterpret_cast<void(__fastcall*)(UIControl*)>(GET_SIG_ADDRESS("UIControl::_setCachedPosition"));
        if (!getPosition && !setCachedPosition) return;

        hat::member_at<int>(this, 0x18) |= 1; // cachedPositionDirty
        auto keep = parentRelativePosition;
        if (getPosition) {
            auto* pos = getPosition(this);
            if (override && pos) *pos = keep;
        } else {
            setCachedPosition(this);
            if (override) parentRelativePosition = keep;
        }
    }

    void getAllControls(std::vector<std::shared_ptr<UIControl>>& list) {
        for (auto& control : this->children) {
            list.emplace_back(control);
            control->getAllControls(list);
        }
    }

    void forEachControl(std::function<bool(std::shared_ptr<UIControl>&)>&& func) {
        std::vector<std::shared_ptr<UIControl>> writeList;
        this->getAllControls(writeList);

        for (auto& control : writeList) {
            bool res = func(control);
            if(res) return;
        }
    }

    void forEachChild(std::function<void(std::shared_ptr<UIControl>&)> func) {
        for (auto& control : children) {
            func(control);
            control->forEachChild(func);
        }
    }
};