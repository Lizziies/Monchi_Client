#include "Notify.hpp"
#include "Theme.hpp"
#include "render/Draw.hpp"
#include "render/Fonts.hpp"
#include "render/Ui.hpp"

#include <imgui.h>

#include <algorithm>
#include <deque>
#include <mutex>

namespace notify {

struct Toast {
    std::string title;
    std::string body;
    Kind kind;
    float life;
    float age = 0.f;
    float slide = 0.f;
    float y = -1.f;
};

static std::mutex lock;
static std::deque<Toast> toasts;
static bool muted = false;

void setMuted(bool on) { muted = on; }

void push(std::string title, std::string body, Kind kind, float seconds) {
    if (muted && kind != Kind::Error) return;
    std::scoped_lock g(lock);
    toasts.push_back({std::move(title), std::move(body), kind, seconds});
    while (toasts.size() > 5) toasts.pop_front();
}

static ImVec4 tint(Kind k) {
    auto& t = theme::current();
    switch (k) {
    case Kind::Ok: return t.ok;
    case Kind::Warn: return t.warn;
    case Kind::Error: return {1.f, 0.42f, 0.48f, 1.f};
    default: return t.accent;
    }
}

void draw() {
    std::scoped_lock g(lock);
    if (toasts.empty()) return;

    auto* dl = ImGui::GetForegroundDrawList();
    auto ds = ImGui::GetIO().DisplaySize;
    float s = ui::scale();
    float w = 300.f * s, pad = 12.f * s, gap = 8.f * s;
    float titleSize = 15.f * s, bodySize = 13.f * s;
    auto& t = theme::current();

    // newest at the bottom right, older ones stack upwards
    float bottom = ds.y - 16.f * s;
    for (auto it = toasts.rbegin(); it != toasts.rend(); ++it) {
        auto& toast = *it;
        toast.age += ui::dt();
        bool leaving = toast.age > toast.life;
        toast.slide = draw::approach(toast.slide, leaving ? 0.f : 1.f, 14.f);

        ImVec2 bodySz = fonts::regular()->CalcTextSizeA(bodySize, FLT_MAX, w - pad * 2 - 6 * s, toast.body.c_str());
        float h = pad * 2 + titleSize + (toast.body.empty() ? 0 : bodySz.y + 3 * s);
        float y = bottom - h;
        if (toast.y < 0) toast.y = y;
        toast.y = draw::approach(toast.y, y, 16.f);
        float x = ds.x - 16.f * s - w + (w + 16.f * s) * (1.f - draw::easeOutCubic(toast.slide));
        ImVec2 min{x, toast.y}, max{x + w, toast.y + h};
        float a = toast.slide;
        float r = t.rounding * s;

        dl->AddRectFilled(min, max, theme::col(t.bg, std::max(t.opacity, 0.9f) * a), r);
        dl->AddRect(min, max, theme::col(theme::border(), 0.85f * a), r, 0, 1.f);
        ImVec4 c = tint(toast.kind);
        dl->AddRectFilled({min.x + 6 * s, min.y + pad}, {min.x + 8.5f * s, max.y - pad}, theme::col(c, a), 2 * s);

        float progress = std::clamp(1.f - toast.age / toast.life, 0.f, 1.f);
        dl->AddRectFilled({min.x + r, max.y - 2 * s}, {min.x + r + (w - 2 * r) * progress, max.y - 1 * s}, theme::col(c, 0.5f * a));

        float tx = min.x + pad + 4 * s;
        dl->AddText(fonts::bold(), titleSize, {tx, min.y + pad}, theme::col(t.text, a), toast.title.c_str());
        if (!toast.body.empty())
            dl->AddText(fonts::regular(), bodySize, {tx, min.y + pad + titleSize + 3 * s}, theme::col(t.textDim, a), toast.body.c_str(), nullptr,
                        w - pad * 2 - 6 * s);

        bottom = y - gap;
    }

    while (!toasts.empty() && toasts.front().age > toasts.front().life && toasts.front().slide <= 0.01f)
        toasts.pop_front();
}

}
