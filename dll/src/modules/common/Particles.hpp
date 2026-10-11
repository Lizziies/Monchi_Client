#pragma once

#include "render/Draw.hpp"
#include "render/Ui.hpp"

#include <imgui.h>

#include <cmath>
#include <algorithm>
#include <random>
#include <vector>

class Particles {
public:
    enum class Shape { Dot, Heart, Spark, Star };

    void burst(ImVec2 origin, int count, Shape shape, ImU32 color, float size, float life, float speed, float gravity = 0.f) {
        count = std::clamp(count, 0, 400);
        if (!count || life <= 0.f) return;
        if (list_.capacity() < 400) list_.reserve(400);
        if (list_.size() + count > 400)
            list_.erase(list_.begin(), list_.begin() + (list_.size() + count - 400));
        std::uniform_real_distribution<float> angle(0.f, 6.2832f), mag(0.4f, 1.f), jitter(0.8f, 1.2f);
        for (int i = 0; i < count; i++) {
            float a = angle(rng_), v = speed * mag(rng_);
            list_.push_back({origin, {std::cos(a) * v, std::sin(a) * v}, life * jitter(rng_), life, size * jitter(rng_), color, shape, gravity});
        }
    }

    void draw(ImDrawList* dl) {
        float dt = ui::dt();
        for (auto& p : list_) {
            p.life -= dt;
            p.vel.y += p.gravity * dt;
            p.pos += p.vel * dt;
        }
        std::erase_if(list_, [](const P& p) { return p.life <= 0.f; });
        for (auto& p : list_) {
            float k = p.life / p.total;
            ImVec4 c = ImGui::ColorConvertU32ToFloat4(p.color);
            c.w *= k;
            ImU32 col = ImGui::ColorConvertFloat4ToU32(c);
            float s = p.size * (0.5f + 0.5f * k);
            switch (p.shape) {
            case Shape::Dot: dl->AddCircleFilled(p.pos, s * 0.5f, col, 12); break;
            case Shape::Heart: draw::heart(dl, p.pos, s, col); break;
            case Shape::Spark: dl->AddLine(p.pos, p.pos - p.vel * 0.04f, col, std::max(1.f, s * 0.2f)); break;
            case Shape::Star: draw::sparkle(dl, p.pos, s * 0.6f, col); break;
            }
        }
    }

    bool empty() const { return list_.empty(); }
    void clear() { list_.clear(); }

private:
    struct P {
        ImVec2 pos;
        ImVec2 vel;
        float life;
        float total;
        float size;
        ImU32 color;
        Shape shape;
        float gravity;
    };
    std::vector<P> list_;
    std::mt19937 rng_{std::random_device{}()};
};
