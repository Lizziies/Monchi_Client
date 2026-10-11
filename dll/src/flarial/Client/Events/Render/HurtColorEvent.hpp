// SPDX-License-Identifier: AGPL-3.0-only
// Adapted from Flarial src/Client/Events/Render/HurtColorEvent.hpp: carry actor identity when the shader source provides it.
#pragma once

#include "../Event.hpp"
#include "../Cancellable.hpp"
#include "../../GUI/D2D.hpp"

class HurtColorEvent : public Event, Cancellable {
private:
    MCCColor* hurtColor;
    bool known = false;
    bool self = false;
public:
    [[nodiscard]] MCCColor* getHurtColor() const {
        return this->hurtColor;
    }

    void setHurtColor(MCCColor* e) {
        this->hurtColor = e;
    }

    void setHurtColorFromD2DColor(D2D1_COLOR_F& e, float alpha) {
        this->hurtColor->r = e.r;
        this->hurtColor->g = e.g;
        this->hurtColor->b = e.b;
        this->hurtColor->a = alpha;
    }

    explicit HurtColorEvent(MCCColor* hurtColor, bool known = false, bool self = false) : hurtColor(hurtColor), known(known), self(self) {}
    bool hasActor() const { return known; }
    bool isSelf() const { return self; }
};

