#include "Cosmetics.hpp"
#include "GpuPreview.hpp"

#include <imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>

namespace cosmetics {

namespace {

constexpr float pi = 3.14159265f;
constexpr float rad = pi / 180.f;

V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
V3 operator*(V3 a, float k) { return {a.x * k, a.y * k, a.z * k}; }
float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }

V3 norm(V3 a) {
    float l = std::sqrt(dot(a, a));
    return l > 1e-6f ? a * (1.f / l) : V3{0, 1, 0};
}

V3 rotateAxis(V3 p, int axis, float deg) {
    float a = deg * rad, c = std::cos(a), s = std::sin(a);
    if (axis == 0) return {p.x, p.y * c - p.z * s, p.y * s + p.z * c};
    if (axis == 1) return {p.x * c + p.z * s, p.y, -p.x * s + p.z * c};
    return {p.x * c - p.y * s, p.x * s + p.y * c, p.z};
}

V3 rotateXyz(V3 p, const float deg[3]) { return rotateAxis(rotateAxis(rotateAxis(p, 0, deg[0]), 1, deg[1]), 2, deg[2]); }

V3 withAxis(V3 v, int axis, float add) {
    if (axis == 0) v.x += add;
    else if (axis == 1) v.y += add;
    else v.z += add;
    return v;
}

ImTextureData* whiteTex() {
    static ImTextureData* white = [] {
        auto* td = IM_NEW(ImTextureData)();
        td->Create(ImTextureFormat_RGBA32, 1, 1);
        std::memset(td->GetPixels(), 255, 4);
        ImGui::RegisterUserTexture(td);
        return td;
    }();
    return white;
}

struct Chain {
    std::vector<float> th, om, ph, op;
};

struct BoneSim {
    float ang[3]{};
    float vel[3]{};
    float phase = 0.f;
    bool init = false;
    unsigned frame = 0;
    Chain chain;
};

}

struct Rig::Impl {
    std::unordered_map<const Bone*, BoneSim> bones;
    Moving m, prev;
    bool havePrev = false;
    float dt = 0.016f;
    float accF = 0.f, accS = 0.f, accU = 0.f;
    float air = 0.f, sprint = 0.f, sneak = 0.f;
    double clock = 0.0;
    float gait = 0.f;
    unsigned frame = 1;
    unsigned gen = 0;
};

Rig::Rig() : d(std::make_unique<Impl>()) {}
Rig::~Rig() = default;

void Rig::clear() { d = std::make_unique<Impl>(); }

void Rig::step(float dt, const Moving& m) {
    dt = std::clamp(dt, 0.001f, 0.05f);
    auto& r = *d;
    if (r.gen != generation()) {
        clear();
        d->gen = generation();
        return;
    }
    auto follow = [&](float& value, float target, float rate) { value += (target - value) * std::min(1.f, dt * rate); };
    if (r.havePrev) {
        follow(r.accF, (m.fwd - r.prev.fwd) / dt, 12.f);
        follow(r.accS, (m.side - r.prev.side) / dt, 12.f);
        follow(r.accU, (m.up - r.prev.up) / dt, 12.f);
    }
    follow(r.air, m.air ? 1.f : 0.f, 9.f);
    follow(r.sprint, m.sprint ? 1.f : 0.f, 6.f);
    follow(r.sneak, m.sneak ? 1.f : 0.f, 8.f);
    r.prev = m;
    r.m = m;
    r.havePrev = true;
    r.dt = dt;
    r.clock += dt;
    r.gait += dt * (m.sprint ? 11.f : 7.5f) * std::min(1.f, m.fwd / 3.f);
    r.frame++;
}

namespace {

struct Face {
    V3 corner[4];
    V3 normal;
    int region;
};

void addFaces(std::vector<Face>& out, const Cube& c) {
    float x0 = c.origin.x, y0 = c.origin.y, z0 = c.origin.z;
    float x1 = x0 + c.size.x, y1 = y0 + c.size.y, z1 = z0 + c.size.z;
    out.push_back({{{x0, y1, z1}, {x1, y1, z1}, {x1, y0, z1}, {x0, y0, z1}}, {0, 0, 1}, 0});
    out.push_back({{{x1, y1, z0}, {x0, y1, z0}, {x0, y0, z0}, {x1, y0, z0}}, {0, 0, -1}, 1});
    out.push_back({{{x1, y1, z1}, {x1, y1, z0}, {x1, y0, z0}, {x1, y0, z1}}, {1, 0, 0}, 2});
    out.push_back({{{x0, y1, z0}, {x0, y1, z1}, {x0, y0, z1}, {x0, y0, z0}}, {-1, 0, 0}, 3});
    out.push_back({{{x0, y1, z0}, {x1, y1, z0}, {x1, y1, z1}, {x0, y1, z1}}, {0, 1, 0}, 4});
    out.push_back({{{x0, y0, z1}, {x1, y0, z1}, {x1, y0, z0}, {x0, y0, z0}}, {0, -1, 0}, 5});
}

// minecraft box layout: top and bottom on the first row, then right, front, left, back.
// flat cubes use one rectangle for front and back, the edges sample the texel below it
void regionUv(const Cube& c, int region, float& u0, float& v0, float& u1, float& v1) {
    float w = c.uvSize.x, h = c.uvSize.y, d = c.uvSize.z;
    float u = float(c.u), v = float(c.v);
    if (c.flat) {
        if (region > 1) {
            u0 = u1 = u + 0.5f;
            v0 = v1 = v + h + 0.5f;
            return;
        }
        u0 = u; v0 = v; u1 = u + w; v1 = v + h;
        if ((region == 1) != c.mirror) std::swap(u0, u1);
        return;
    }
    switch (region) {
    case 4: u0 = u + d; v0 = v; u1 = u0 + w; v1 = v + d; break;
    case 5: u0 = u + d + w; v0 = v; u1 = u0 + w; v1 = v + d; break;
    case 3: u0 = u; v0 = v + d; u1 = u0 + d; v1 = v0 + h; break;
    case 0: u0 = u + d; v0 = v + d; u1 = u0 + w; v1 = v0 + h; break;
    case 2: u0 = u + d + w; v0 = v + d; u1 = u0 + d; v1 = v0 + h; break;
    default: u0 = u + 2 * d + w; v0 = v + d; u1 = u0 + w; v1 = v0 + h; break;
    }
}

float axisOf(V3 v, int axis) { return axis == 0 ? v.x : axis == 1 ? v.y : v.z; }

struct Ring {
    std::vector<V3> pts;
    V3 center;
    V3 tangent;
    float mix;
};

std::vector<Ring> sweep(const Tube& t, double time) {
    size_t n = t.path.size();
    std::vector<V3> c(n);
    for (size_t i = 0; i < n; i++) {
        c[i] = t.path[i].pos;
        if (t.wave.amplitude != 0.f) {
            float prog = float(i) / float(n - 1);
            c[i] = withAxis(c[i], t.wave.axis, t.wave.amplitude * prog * prog * std::sin(float(time) * t.wave.speed * 2.f * pi - prog * t.wave.freq * pi));
        }
    }
    std::vector<Ring> rings(n);
    V3 u{};
    float inv = 2.f / t.power;
    for (size_t i = 0; i < n; i++) {
        V3 tan = norm(c[std::min(i + 1, n - 1)] - c[i ? i - 1 : 0]);
        if (i == 0) {
            V3 ref = std::fabs(tan.x) > 0.9f ? V3{0, 0, 1} : V3{1, 0, 0};
            u = norm(ref - tan * dot(ref, tan));
        } else {
            u = norm(u - tan * dot(u, tan));
        }
        V3 w = cross(tan, u);
        Ring r;
        r.center = c[i];
        r.tangent = tan;
        r.mix = t.path[i].mix;
        for (int k = 0; k < t.sides; k++) {
            float a = 2.f * pi * float(k) / float(t.sides);
            float cs = std::cos(a), sn = std::sin(a);
            float x = std::copysign(std::pow(std::fabs(cs), inv), cs);
            float z = std::copysign(std::pow(std::fabs(sn), inv), sn);
            r.pts.push_back(c[i] + u * (t.path[i].rx * x) + w * (t.path[i].rz * z));
        }
        rings[i] = std::move(r);
    }
    return rings;
}

void animTarget(const Bone& b, const BoneSim& s, const Rig::Impl& r, float out[3], V3& move) {
    out[0] = b.rotation.x;
    out[1] = b.rotation.y;
    out[2] = b.rotation.z;
    const Anim& a = b.anim;
    float tau = s.phase;
    float speedN = std::clamp(r.m.fwd / 5.6f, 0.f, 1.2f);
    switch (a.kind) {
    case Motion::Flap: out[a.axis] += a.amplitude * (1.f + 1.1f * r.air + 0.15f * speedN) * std::sin(tau + a.phase); break;
    case Motion::Sway:
    case Motion::Wag: out[a.axis] += a.amplitude * (1.f + 0.6f * speedN) * std::sin(tau + a.phase); break;
    case Motion::Twitch: out[a.axis] += a.amplitude * std::pow(std::max(0.f, std::sin(tau * 0.5f + a.phase)), 8.f) * std::max(0.f, std::sin(tau * 3.f)); break;
    case Motion::Spin: out[a.axis] += tau * 57.29578f + a.phase; break;
    case Motion::Bob: move.y = a.amplitude * std::sin(tau + a.phase); break;
    case Motion::Float:
        move.y = a.amplitude * std::sin(tau + a.phase);
        move.x = a.amplitude * 0.3f * std::sin(tau * 0.5f + a.phase);
        break;
    case Motion::Walk: out[a.axis] += a.amplitude * std::sin(r.gait + a.phase) * std::clamp(r.m.fwd / 3.f, 0.f, 1.f) * (1.f - r.air); break;
    case Motion::Hop: move.y = a.amplitude * std::fabs(std::sin(tau + a.phase)); break;
    default: break;
    }
    const Physics& p = b.physics;
    for (int i = 0; i < 3; i++)
        out[i] += r.air * axisOf(p.air, i) + r.sprint * axisOf(p.sprint, i) + r.sneak * axisOf(p.sneak, i) + speedN * axisOf(p.speed, i);
}

void springStep(const Bone& b, BoneSim& s, const Rig::Impl& r, float h) {
    float target[3];
    V3 move{};
    animTarget(b, s, r, target, move);
    float kick[3] = {-8.f * r.accF + 2.f * r.accU, -1.5f * r.m.turn, 8.f * r.accS};
    for (int i = 0; i < 3; i++) {
        float acc = b.physics.stiffness * (target[i] - s.ang[i]) - b.physics.damping * s.vel[i] + b.physics.inertia * kick[i];
        s.vel[i] += acc * h;
        s.ang[i] += s.vel[i] * h;
    }
}

// pendulum chain over the strips from top to bottom, gravity and wind, the body acts as a wall behind the cape
void clothStep(const Bone& b, BoneSim& s, const Rig::Impl& r, const std::vector<const Cube*>& order, float h) {
    size_t n = order.size();
    Chain& c = s.chain;
    const Physics& p = b.physics;
    float windDeg = std::clamp(r.m.fwd * 5.2f, 0.f, 60.f) * p.wind + std::clamp(-r.m.up * 3.f, 0.f, 35.f) + r.sneak * 7.f;
    float gravity = p.stiffness, drag = p.damping, link = 22.f;
    for (size_t i = 0; i < n; i++) {
        float frac = n > 1 ? float(i) / float(n - 1) : 0.f;
        float target = windDeg * rad * (0.3f + 0.7f * frac);
        float gust = 0.35f * std::sin(float(r.clock) * 2.3f + float(i) * 0.8f) * (0.25f + std::min(1.f, r.m.fwd / 4.f));
        float up = i ? c.th[i - 1] : 0.f, down = i + 1 < n ? c.th[i + 1] : c.th[i];
        float acc = -gravity * std::sin(c.th[i] - target) - drag * c.om[i] + link * (up - c.th[i]) + link * 0.6f * (down - c.th[i]);
        acc += p.inertia * (r.accF * 0.09f + r.accU * 0.03f) * (0.4f + frac) + gust;
        c.om[i] += acc * h;
        float upR = i ? c.ph[i - 1] : 0.f, downR = i + 1 < n ? c.ph[i + 1] : c.ph[i];
        float accR = -gravity * 0.8f * std::sin(c.ph[i]) - drag * c.op[i] + link * (upR - c.ph[i]) + link * 0.6f * (downR - c.ph[i]);
        accR += p.inertia * (r.accS * 0.09f - r.m.turn * 0.004f) * (0.4f + frac) + gust * 0.4f;
        c.op[i] += accR * h;
    }
    float zmax = 0.35f, z = 0.f;
    for (size_t i = 0; i < n; i++) {
        c.th[i] += c.om[i] * h;
        c.ph[i] += c.op[i] * h;
        c.th[i] = std::clamp(c.th[i], -1.2f, 1.9f);
        c.ph[i] = std::clamp(c.ph[i], -0.9f, 0.9f);
        float len = order[i]->size.y;
        float nz = z - len * std::sin(c.th[i]);
        if (nz > zmax) {
            c.th[i] = -std::asin(std::clamp((zmax - z) / len, -1.f, 1.f));
            if (c.om[i] > 0.f) c.om[i] *= -0.2f;
            nz = zmax;
        }
        z = nz;
    }
}

void integrate(const Bone& b, BoneSim& s, Rig::Impl& r, const std::vector<const Cube*>& order) {
    if (s.frame == r.frame) return;
    float dt = s.frame ? std::min(r.dt * float(r.frame - s.frame), 0.05f) : 0.f;
    s.frame = r.frame;
    if (!s.init) {
        s.init = true;
        float target[3];
        V3 move{};
        animTarget(b, s, r, target, move);
        for (int i = 0; i < 3; i++) s.ang[i] = target[i];
        s.chain.th.assign(order.size(), 0.f);
        s.chain.om.assign(order.size(), 0.f);
        s.chain.ph.assign(order.size(), 0.f);
        s.chain.op.assign(order.size(), 0.f);
        return;
    }
    int steps = std::max(1, int(std::ceil(dt * 120.f)));
    float h = dt / float(steps);
    float rate = b.anim.speed * (b.anim.kind == Motion::Flap ? 1.f + 2.f * r.air + 0.4f * r.sprint : 1.f);
    for (int k = 0; k < steps; k++) {
        s.phase += h * 2.f * pi * rate;
        if (b.physics.cloth) clothStep(b, s, r, order, h);
        else if (b.physics.spring) springStep(b, s, r, h);
    }
}

struct Draw {
    ImVec2 p[4];
    ImVec2 uv[4];
    float z[4];
    V3 middle;
    float depth;
    ImU32 col;
    const ImTextureData* tex;
    bool nearest;
};

// The picture is put together pixel by pixel with a depth value for each, the way a graphics card does it. Sorting
// whole faces by distance, as this did before, cannot be right where things lie close to each other: a tail showed
// through the trousers and wings through the chest. It is drawn at twice the size and averaged down for soft edges.
struct Canvas {
    ImTextureData* texture = nullptr;
    int w = 0, h = 0;
    int used = -1;
};

std::vector<Canvas> canvases;
std::vector<unsigned char> big;
std::vector<float> deep;

Canvas& canvasFor(int w, int h) {
    int frame = ImGui::GetFrameCount();
    for (auto it = canvases.begin(); it != canvases.end();) {
        if (frame - it->used > 600) {
            retire(it->texture);
            it = canvases.erase(it);
        } else ++it;
    }
    for (auto& c : canvases)
        if (c.w == w && c.h == h && c.used != frame) {
            c.used = frame;
            return c;
        }
    Canvas c;
    c.texture = IM_NEW(ImTextureData)();
    c.texture->Create(ImTextureFormat_RGBA32, w, h);
    std::memset(c.texture->GetPixels(), 0, size_t(w) * size_t(h) * 4);
    ImGui::RegisterUserTexture(c.texture);
    c.w = w;
    c.h = h;
    c.used = frame;
    canvases.push_back(c);
    return canvases.back();
}

void texel(const Draw& d, float u, float v, int out[4]) {
    const ImTextureData* t = d.tex;
    const unsigned char* px = t->Pixels;
    float x = u * float(t->Width) - 0.5f, y = v * float(t->Height) - 0.5f;
    auto at = [&](int ix, int iy) { return px + (size_t(std::clamp(iy, 0, t->Height - 1)) * size_t(t->Width) + size_t(std::clamp(ix, 0, t->Width - 1))) * 4; };
    if (d.nearest) {
        const unsigned char* c = at(int(std::floor(x + 0.5f)), int(std::floor(y + 0.5f)));
        for (int k = 0; k < 4; k++) out[k] = c[k];
        return;
    }
    int x0 = int(std::floor(x)), y0 = int(std::floor(y));
    float fx = x - float(x0), fy = y - float(y0);
    const unsigned char *a = at(x0, y0), *b = at(x0 + 1, y0), *c = at(x0, y0 + 1), *e = at(x0 + 1, y0 + 1);
    // weighted by alpha, or the color of see-through texels would bleed into the edge
    float wa = (1 - fx) * (1 - fy) * a[3], wb = fx * (1 - fy) * b[3], wc = (1 - fx) * fy * c[3], we = fx * fy * e[3];
    float sum = wa + wb + wc + we;
    if (sum <= 0.f) {
        out[0] = out[1] = out[2] = out[3] = 0;
        return;
    }
    for (int k = 0; k < 3; k++) out[k] = int((a[k] * wa + b[k] * wb + c[k] * wc + e[k] * we) / sum);
    out[3] = int(sum);
}

void fill(const Draw& d, int a, int b, int c, ImVec2 origin, float scale, int w, int h) {
    float x[3], y[3];
    const int idx[3] = {a, b, c};
    for (int i = 0; i < 3; i++) {
        x[i] = (d.p[idx[i]].x - origin.x) * scale;
        y[i] = (d.p[idx[i]].y - origin.y) * scale;
    }
    float area = (x[1] - x[0]) * (y[2] - y[0]) - (y[1] - y[0]) * (x[2] - x[0]);
    if (std::fabs(area) < 1e-4f) return;
    int x0 = std::max(0, int(std::floor(std::min({x[0], x[1], x[2]})))), x1 = std::min(w - 1, int(std::ceil(std::max({x[0], x[1], x[2]}))));
    int y0 = std::max(0, int(std::floor(std::min({y[0], y[1], y[2]})))), y1 = std::min(h - 1, int(std::ceil(std::max({y[0], y[1], y[2]}))));
    if (x0 > x1 || y0 > y1) return;
    float inv = 1.f / area;
    int tint[4] = {int(d.col & 255), int((d.col >> 8) & 255), int((d.col >> 16) & 255), int(d.col >> 24)};
    for (int py = y0; py <= y1; py++) {
        float sy = float(py) + 0.5f;
        for (int px = x0; px <= x1; px++) {
            float sx = float(px) + 0.5f;
            float w0 = ((x[1] - sx) * (y[2] - sy) - (y[1] - sy) * (x[2] - sx)) * inv;
            float w1 = ((x[2] - sx) * (y[0] - sy) - (y[2] - sy) * (x[0] - sx)) * inv;
            float w2 = 1.f - w0 - w1;
            if (w0 < 0.f || w1 < 0.f || w2 < 0.f) continue;
            float z = d.z[a] * w0 + d.z[b] * w1 + d.z[c] * w2;
            size_t at = size_t(py) * size_t(w) + size_t(px);
            if (z < deep[at]) continue;
            int t[4];
            texel(d, d.uv[a].x * w0 + d.uv[b].x * w1 + d.uv[c].x * w2, d.uv[a].y * w0 + d.uv[b].y * w1 + d.uv[c].y * w2, t);
            int alpha = t[3] * tint[3] / 255;
            if (alpha < 24) continue;
            unsigned char* out = &big[at * 4];
            if (alpha >= 250 || out[3] == 0) {
                for (int k = 0; k < 3; k++) out[k] = (unsigned char)(t[k] * tint[k] / 255);
                out[3] = (unsigned char)std::max(alpha, int(out[3]));
            } else {
                for (int k = 0; k < 3; k++) out[k] = (unsigned char)((t[k] * tint[k] / 255 * alpha + out[k] * (255 - alpha)) / 255);
                out[3] = (unsigned char)(alpha + out[3] * (255 - alpha) / 255);
            }
            if (alpha >= 128) deep[at] = z;
        }
    }
}

void paint(ImDrawList* dl, std::vector<Draw>& draws, std::vector<PreviewFace>* frame) {
    if (draws.empty()) return;
    ImVec2 lo = dl ? dl->GetClipRectMin() : ImVec2(-FLT_MAX, -FLT_MAX);
    ImVec2 hi = dl ? dl->GetClipRectMax() : ImVec2(FLT_MAX, FLT_MAX);
    ImVec2 from = draws.front().p[0], to = from;
    for (auto& d : draws)
        for (auto& q : d.p) {
            from = {std::min(from.x, q.x), std::min(from.y, q.y)};
            to = {std::max(to.x, q.x), std::max(to.y, q.y)};
        }
    lo = {std::floor(std::max(lo.x, from.x - 1.f)), std::floor(std::max(lo.y, from.y - 1.f))};
    hi = {std::ceil(std::min(hi.x, to.x + 1.f)), std::ceil(std::min(hi.y, to.y + 1.f))};
    int w = int(hi.x - lo.x), h = int(hi.y - lo.y);
    if (w < 2 || h < 2 || w > 4096 || h > 4096) return;
    // small sizes change with every pixel the model moves; rounding up keeps the textures few
    int cw = (w + 31) / 32 * 32, ch = (h + 31) / 32 * 32;
    int scale = size_t(cw) * size_t(ch) <= 400000 ? 2 : 1;
    std::stable_sort(draws.begin(), draws.end(), [](const Draw& a, const Draw& b) { return a.depth < b.depth; });
    std::vector<gpu::Face> faces;
    faces.reserve(draws.size());
    for (auto& d : draws) {
        gpu::Face face{};
        for (int i = 0; i < 4; ++i) { face.p[i] = d.p[i]; face.uv[i] = d.uv[i]; face.z[i] = d.z[i]; }
        face.color = d.col; face.texture = d.tex; face.nearest = d.nearest;
        faces.push_back(face);
    }
    if (auto result = gpu::paint(faces, lo, cw, ch); result.owner) {
        if (frame) {
            PreviewFace image{};
            image.p[0] = lo; image.p[1] = {lo.x + float(cw), lo.y};
            image.p[2] = {lo.x + float(cw), lo.y + float(ch)}; image.p[3] = {lo.x, lo.y + float(ch)};
            image.uv[0] = {0, 0}; image.uv[1] = {1, 0}; image.uv[2] = {1, 1}; image.uv[3] = {0, 1};
            image.col = IM_COL32_WHITE; image.tex = ImTextureRef(result.texture); image.gpuImage = std::move(result.owner);
            frame->assign(1, std::move(image));
        } else dl->AddImage(ImTextureRef(result.texture), lo, {lo.x + float(cw), lo.y + float(ch)}, {0, 0}, {1, 1}, ImGui::GetColorU32({1, 1, 1, 1}));
        return;
    }
    int bw = cw * scale, bh = ch * scale;
    big.assign(size_t(bw) * size_t(bh) * 4, 0);
    deep.assign(size_t(bw) * size_t(bh), -1e9f);
    // far to near, so what is half see-through lies over what is behind it
    std::stable_sort(draws.begin(), draws.end(), [](const Draw& a, const Draw& b) { return a.depth < b.depth; });
    for (auto& d : draws) {
        fill(d, 0, 1, 2, lo, float(scale), bw, bh);
        fill(d, 0, 2, 3, lo, float(scale), bw, bh);
    }
    std::shared_ptr<ImTextureData> owned;
    ImTextureData* texture;
    if (frame) {
        texture = IM_NEW(ImTextureData)();
        texture->Create(ImTextureFormat_RGBA32, cw, ch);
        ImGui::RegisterUserTexture(texture);
        owned = {texture, [](ImTextureData* t) { retire(t); }};
    } else texture = canvasFor(cw, ch).texture;
    auto out = static_cast<unsigned char*>(texture->GetPixels());
    if (scale == 1) std::memcpy(out, big.data(), big.size());
    else
        for (int y = 0; y < ch; y++)
            for (int x = 0; x < cw; x++) {
                int sum[4] = {0, 0, 0, 0};
                for (int k = 0; k < 4; k++) {
                    const unsigned char* q = &big[(size_t(y * 2 + (k >> 1)) * size_t(bw) + size_t(x * 2 + (k & 1))) * 4];
                    sum[0] += q[0] * q[3];
                    sum[1] += q[1] * q[3];
                    sum[2] += q[2] * q[3];
                    sum[3] += q[3];
                }
                unsigned char* o = out + (size_t(y) * size_t(cw) + size_t(x)) * 4;
                for (int k = 0; k < 3; k++) o[k] = (unsigned char)(sum[3] ? sum[k] / sum[3] : 0);
                o[3] = (unsigned char)(sum[3] / 4);
            }
    ImTextureDataQueueUpload(texture, 0, 0, cw, ch);
    if (frame) {
        PreviewFace image{};
        image.p[0] = lo;
        image.p[1] = {lo.x + float(cw), lo.y};
        image.p[2] = {lo.x + float(cw), lo.y + float(ch)};
        image.p[3] = {lo.x, lo.y + float(ch)};
        image.uv[0] = {0, 0}; image.uv[1] = {1, 0}; image.uv[2] = {1, 1}; image.uv[3] = {0, 1};
        image.col = IM_COL32_WHITE;
        image.tex = texture->GetTexRef();
        image.image = std::move(owned);
        frame->assign(1, std::move(image));
    } else dl->AddImage(texture->GetTexRef(), lo, ImVec2(lo.x + float(cw), lo.y + float(ch)), {0, 0}, {1, 1}, ImGui::GetColorU32({1, 1, 1, 1}));
}

}

void drawPreview(ImDrawList* dl, ImVec2 center, float unit, float yaw, float pitch, const std::vector<Worn>& worn, ImVec4 body, const Look& look) {
    static Rig fallback;
    Rig::Impl& rig = *(look.rig ? look.rig : &fallback)->d;
    ImTextureData* white = look.mesh ? nullptr : whiteTex();
    std::vector<Draw> draws;

    auto project = [&](V3 p) {
        V3 r = rotateAxis(rotateAxis(p, 1, yaw), 0, pitch);
        return V3{center.x + r.x * unit, center.y - (r.y - look.focus) * unit, r.z};
    };
    auto push = [&](const V3 (&pts)[4], V3 n, bool twoSided, ImVec4 tint, float alpha, ImTextureData* tex, bool nearest, const ImVec2 (&uv)[4]) {
        if (look.mesh) {
            Quad quad{};
            for (int i = 0; i < 4; i++) {
                quad.points[i] = pts[i];
                quad.uv[i] = uv[i];
            }
            quad.color = {tint.x, tint.y, tint.z, alpha};
            quad.texture = tex;
            quad.twoSided = twoSided;
            look.mesh->push_back(quad);
            return;
        }
        V3 vn = rotateAxis(rotateAxis(n, 1, yaw), 0, pitch);
        if (vn.z < -0.02f && !twoSided) return;
        Draw d{};
        float depth = 0.f;
        for (int i = 0; i < 4; i++) {
            V3 sp = project(pts[i]);
            d.p[i] = {sp.x, sp.y};
            d.z[i] = sp.z;
            depth += sp.z * 0.25f;
            d.middle = d.middle + pts[i] * 0.25f;
        }
        float lit = twoSided ? std::fabs(vn.z) : vn.z;
        float shade = 0.62f + 0.38f * std::clamp(vn.y * 0.5f + lit * 0.8f + 0.2f, 0.f, 1.f);
        d.col = ImGui::ColorConvertFloat4ToU32({tint.x * shade, tint.y * shade, tint.z * shade, alpha});
        d.depth = depth;
        for (int i = 0; i < 4; i++) d.uv[i] = uv[i];
        d.tex = tex ? tex : white;
        d.nearest = nearest;
        draws.push_back(d);
    };

    struct Place {
        const Bone* bone = nullptr;
        float deg[3]{};
        V3 move{};
        const BoneSim* sim = nullptr;
        const std::vector<const Cube*>* order = nullptr;
        const Place* parent = nullptr;
        int cube = -1;

        V3 point(V3 p) const {
            if (!bone) return p;
            if (sim && cube >= 0 && bone->physics.cloth && sim->chain.th.size() == order->size()) {
                for (int j = cube; j >= 0; j--) {
                    const Cube& jc = *(*order)[size_t(j)];
                    V3 joint{jc.origin.x + jc.size.x * 0.5f, jc.origin.y + jc.size.y, jc.origin.z + jc.size.z * 0.5f};
                    float prevTh = j ? sim->chain.th[size_t(j - 1)] : 0.f, prevPh = j ? sim->chain.ph[size_t(j - 1)] : 0.f;
                    float rel[3] = {(sim->chain.th[size_t(j)] - prevTh) / rad, 0.f, (sim->chain.ph[size_t(j)] - prevPh) / rad};
                    p = joint + rotateXyz(p - joint, rel);
                }
            }
            V3 pivot = bone->pivot;
            p = pivot + rotateXyz(p - pivot, deg) + move;
            return parent ? parent->point(p) : p;
        }

        V3 normal(V3 n) const {
            if (!bone) return n;
            n = rotateXyz(n, deg);
            return parent ? parent->normal(n) : n;
        }
    };

    ImVec4 skin{0.86f, 0.66f, 0.54f, 1.f};
    ImVec4 legs{body.x * 0.5f, body.y * 0.5f, body.z * 0.5f, 1.f};
    ImVec4 dark{0.2f, 0.14f, 0.16f, 1.f};
    struct Part {
        V3 o, s;
        ImVec4 c;
    };
    float arm = look.slim ? 3.f : 4.f;
    const Part figure[] = {{{-4, 24, -4}, {8, 8, 8}, skin},  {{-4, 12, -2}, {8, 12, 4}, body},       {{-4 - arm, 12, -2}, {arm, 12, 4}, skin},
                           {{4, 12, -2}, {arm, 12, 4}, skin}, {{-4, 0, -2}, {4, 12, 4}, legs},        {{0, 0, -2}, {4, 12, 4}, legs},
                           {{-2.8f, 27.f, 4.f}, {1.6f, 1.8f, 0.05f}, dark}, {{1.2f, 27.f, 4.f}, {1.6f, 1.8f, 0.05f}, dark}};
    // limbs swing with the wearer's steps and the arms go up in the air, as the items were built and checked for
    float stride = (rig.m.sprint ? 48.f : 32.f) * std::min(1.f, rig.m.fwd / 3.f) * (1.f - rig.air) * std::sin(rig.gait);
    Bone joints[4];
    joints[0].pivot = {-4.f - arm * 0.5f, 24.f, 0.f};
    joints[1].pivot = {4.f + arm * 0.5f, 24.f, 0.f};
    joints[2].pivot = {-2.f, 12.f, 0.f};
    joints[3].pivot = {2.f, 12.f, 0.f};
    Place limbs[4];
    for (int i = 0; i < 4; i++) limbs[i].bone = &joints[i];
    limbs[0].deg[0] = stride * 0.9f - rig.air * 126.f;
    limbs[1].deg[0] = -stride * 0.9f - rig.air * 126.f;
    limbs[2].deg[0] = -stride;
    limbs[3].deg[0] = stride;
    if (rig.m.posed) {
        limbs[0].deg[0] = rig.m.armRight;
        limbs[1].deg[0] = rig.m.armLeft;
        limbs[2].deg[0] = rig.m.legRight;
        limbs[3].deg[0] = rig.m.legLeft;
    }

    // head and body for most items; shoes sit on the legs, so they are tested against the legs before either moves
    auto insideBody = [&](V3 p, bool feet) {
        for (int i = feet ? 4 : 0; i < (feet ? 6 : 2); i++) {
            const Part& b = figure[i];
            if (p.x > b.o.x + 0.05f && p.x < b.o.x + b.s.x - 0.05f && p.y > b.o.y + 0.05f && p.y < b.o.y + b.s.y - 0.05f && p.z > b.o.z + 0.05f && p.z < b.o.z + b.s.z - 0.05f) return true;
        }
        return false;
    };
    auto emitCube = [&](const Cube& cube, const Place& place, const Item* item, ImVec4 tint, float alpha) {
        std::vector<Face> faces;
        addFaces(faces, cube);
        bool textured = item && item->texture;
        for (auto& f : faces) {
            // the rim of a card is 0.04 wide and would take its color from the texel under the card
            if (cube.flat && f.region > 1) continue;
            ImVec2 uv[4] = {{0.5f, 0.5f}, {0.5f, 0.5f}, {0.5f, 0.5f}, {0.5f, 0.5f}};
            if (textured) {
                float u0, v0, u1, v1;
                regionUv(cube, f.region, u0, v0, u1, v1);
                if (item->texel > 1.f && std::fabs(u1 - u0) > 1.f && std::fabs(v1 - v0) > 1.f) {
                    float dir = u1 > u0 ? 0.5f : -0.5f;
                    u0 += dir;
                    u1 -= dir;
                    v0 += 0.5f;
                    v1 -= 0.5f;
                }
                float tw = float(item->texW), th = float(item->texH);
                uv[0] = {u0 / tw, v0 / th}; uv[1] = {u1 / tw, v0 / th}; uv[2] = {u1 / tw, v1 / th}; uv[3] = {u0 / tw, v1 / th};
            }
            // the painter's sort works per quad, so big plates are cut into small tiles or they sort wrongly against the body
            auto steps = [](V3 a, V3 b) { return std::clamp(int(std::ceil(std::sqrt(dot(b - a, b - a)) / 2.f)), 1, 10); };
            // cloth bends from strip to strip, so a face needs no further cutting; depth is settled per pixel
            int nu = 1, nv = 1;
            (void)steps;
            V3 eu = f.corner[1] - f.corner[0], ev = f.corner[3] - f.corner[0];
            ImVec2 uu = {uv[1].x - uv[0].x, uv[1].y - uv[0].y}, uvv = {uv[3].x - uv[0].x, uv[3].y - uv[0].y};
            bool feet = item && item->slot == "feet";
            auto at = [&](float a, float b, V3& p, ImVec2& t) {
                p = place.point(f.corner[0] + eu * a + ev * b);
                t = {uv[0].x + uu.x * a + uvv.x * b, uv[0].y + uu.y * a + uvv.y * b};
            };
            for (int j = 0; j < nv; j++)
                for (int i = 0; i < nu; i++) {
                    float a0 = float(i) / float(nu), a1 = float(i + 1) / float(nu), b0 = float(j) / float(nv), b1 = float(j + 1) / float(nv);
                    V3 pts[4];
                    ImVec2 tex[4];
                    at(a0, b0, pts[0], tex[0]);
                    at(a1, b0, pts[1], tex[1]);
                    at(a1, b1, pts[2], tex[2]);
                    at(a0, b1, pts[3], tex[3]);
                    (void)feet;
                    push(pts, place.normal(f.normal), item != nullptr, tint, alpha, textured ? item->texture : nullptr, !item || item->texel <= 1.f, tex);
                }
        }
    };

    auto emitTube = [&](const Tube& tube, const Place& place, ImVec4 a, ImVec4 b) {
        auto rings = sweep(tube, rig.clock);
        const ImVec2 flat[4] = {{0.5f, 0.5f}, {0.5f, 0.5f}, {0.5f, 0.5f}, {0.5f, 0.5f}};
        auto mixed = [&](float m) {
            m = std::clamp(m, 0.f, 1.f);
            return ImVec4{a.x + (b.x - a.x) * m, a.y + (b.y - a.y) * m, a.z + (b.z - a.z) * m, 1.f};
        };
        auto facet = [&](V3 p0, V3 p1, V3 p2, V3 p3, V3 away, float m) {
            V3 n = norm(cross(p2 - p0, p3 - p1));
            V3 centre = (p0 + p1 + p2 + p3) * 0.25f;
            if (dot(n, centre - away) < 0.f) n = n * -1.f;
            const V3 pts[4] = {place.point(p0), place.point(p1), place.point(p2), place.point(p3)};
            push(pts, place.normal(n), tube.twoSided, mixed(m), 1.f, nullptr, false, flat);
        };
        int n = tube.sides;
        for (size_t i = 0; i + 1 < rings.size(); i++) {
            V3 away = (rings[i].center + rings[i + 1].center) * 0.5f;
            float m = (rings[i].mix + rings[i + 1].mix) * 0.5f;
            for (int k = 0; k < n; k++) {
                int k2 = (k + 1) % n;
                facet(rings[i].pts[k], rings[i].pts[k2], rings[i + 1].pts[k2], rings[i + 1].pts[k], away, m);
            }
        }
        if (!tube.capped) return;
        for (int end = 0; end < 2; end++) {
            const Ring& r = end ? rings.back() : rings.front();
            V3 away = r.center + r.tangent * (end ? -1.f : 1.f);
            for (int k = 0; k < n; k++) facet(r.pts[k], r.pts[(k + 1) % n], r.center, r.center, away, r.mix);
        }
    };

    struct Solid {
        float nearest = -1e9f, farthest = 1e9f;
        bool facing[6]{};
    };
    Solid solids[6];
    bool skinned = look.skin && look.skin->texture;
    for (int i = 0; i < (look.mesh ? 0 : skinned ? 6 : 8); i++) {
        const Part& p = figure[i];
        Place still;
        const Place& place = i >= 2 && i < 6 ? limbs[i - 2] : still;
        size_t from = draws.size();
        if (skinned) {
            const int uv[][2] = {{0, 0}, {16, 16}, {40, 16}, {32, 48}, {0, 16}, {16, 48}};
            Cube cube{p.o, p.s, p.s, uv[i][0], uv[i][1], -1};
            if (look.skin->texH * 2 == look.skin->texW && (i == 3 || i == 5)) {
                cube.u = i == 3 ? 40 : 0;
                cube.v = 16;
                cube.mirror = true;
            }
            float ratio = float(look.skin->texW) / 64.f;
            cube.u = int(cube.u * ratio);
            cube.v = int(cube.v * ratio);
            cube.uvSize = cube.uvSize * ratio;
            emitCube(cube, place, look.skin, {1, 1, 1, 1}, 1.f);
            if (i == 0 || look.skin->texH == look.skin->texW) {
                const int overlay[][2] = {{32, 0}, {16, 32}, {40, 32}, {48, 48}, {0, 32}, {0, 48}};
                cube.u = int(overlay[i][0] * ratio);
                cube.v = int(overlay[i][1] * ratio);
                float inflate = i == 0 ? 0.5f : 0.25f;
                cube.origin = p.o - V3{inflate, inflate, inflate};
                cube.size = p.s + V3{inflate * 2, inflate * 2, inflate * 2};
                emitCube(cube, place, look.skin, {1, 1, 1, 1}, 1.f);
            }
        } else {
            emitCube({p.o, p.s, {}, 0, 0, -1}, place, nullptr, p.c, 1.f);
        }
        if (i >= 6) continue;
        for (size_t k = from; k < draws.size(); k++) {
            solids[i].nearest = std::max(solids[i].nearest, draws[k].depth);
            solids[i].farthest = std::min(solids[i].farthest, draws[k].depth);
        }
        const V3 normals[6] = {{0, 0, 1}, {0, 0, -1}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
        for (int f = 0; f < 6; f++) solids[i].facing[f] = rotateAxis(rotateAxis(place.normal(normals[f]), 1, yaw), 0, pitch).z > 0.02f;
    }
    size_t figureEnd = draws.size();

    for (auto& w : worn) {
        const Item* item = w.item;
        if (!item) continue;
        auto tintOf = [&](int i, ImVec4 fallback) {
            if (i < 0 || i >= (int)item->tints.size()) return fallback;
            size_t k = size_t(i);
            return k < w.tints.size() ? w.tints[k] : item->tints[k].color;
        };
        std::vector<Place> placed(item->bones.size());
        for (size_t b = 0; b < item->bones.size(); b++) {
            const Bone& bone = item->bones[b];
            std::vector<const Cube*> order;
            for (auto& c : bone.cubes) order.push_back(&c);
            if (bone.physics.cloth)
                std::stable_sort(order.begin(), order.end(), [](const Cube* a, const Cube* b) { return a->origin.y + a->size.y > b->origin.y + b->size.y; });

            BoneSim& sim = rig.bones[&bone];
            integrate(bone, sim, rig, order);

            Place& place = placed[b];
            place.bone = &bone;
            place.sim = &sim;
            place.order = &order;
            place.parent = bone.parent >= 0 && size_t(bone.parent) < b ? &placed[size_t(bone.parent)] : nullptr;
            if (!place.parent && item->slot == "feet") place.parent = &limbs[bone.pivot.x >= 0.f ? 3 : 2];
            float target[3];
            animTarget(bone, sim, rig, target, place.move);
            const float* pose = bone.physics.spring ? sim.ang : target;
            for (int i = 0; i < 3; i++) place.deg[i] = pose[i];

            size_t tubeFrom = draws.size();
            for (auto& tube : bone.tubes) {
                ImVec4 a = tintOf(tube.tint, {1, 1, 1, 1});
                emitTube(tube, place, a, tintOf(tube.tint2, a));
            }
            float tubeNear = -1e9f, tubeFar = 1e9f;
            for (size_t i = tubeFrom; i < draws.size(); i++) {
                tubeNear = std::max(tubeNear, draws[i].depth);
                tubeFar = std::min(tubeFar, draws[i].depth);
            }
            bool hasTube = draws.size() > tubeFrom;
            if (!item->texture) {
                place.order = nullptr;
                continue;
            }
            for (size_t i = 0; i < order.size(); i++) {
                const Cube& cube = *order[i];
                ImVec4 tint = tintOf(cube.tint, {1, 1, 1, 1});
                if (cube.tint2 >= 0) {
                    ImVec4 other = tintOf(cube.tint2, tint);
                    tint = {tint.x + (other.x - tint.x) * cube.mix, tint.y + (other.y - tint.y) * cube.mix, tint.z + (other.z - tint.z) * cube.mix, 1.f};
                }
                float alpha = bone.anim.kind == Motion::Sparkle ? 0.55f + 0.45f * std::sin(sim.phase * 2.f + bone.anim.phase + float(i) * 1.7f) : 1.f;
                Place cp = place;
                cp.cube = int(i);
                size_t cardFrom = draws.size();
                emitCube(cube, cp, item, tint, alpha);
                if (!cube.flat || !hasTube) continue;
                // A card painted onto a tube (inner ear, fur edge) lies a hair above its surface. Sorting quad
                // against quad by their middles cannot keep that apart, so the card goes as a whole in front of
                // the bone's tubes when it is on the viewer's side of them and behind them otherwise.
                V3 middle{cube.origin.x + cube.size.x * 0.5f, cube.origin.y + cube.size.y * 0.5f, cube.origin.z + cube.size.z * 0.5f};
                V3 out{};
                float best = 1e9f;
                for (auto& tube : bone.tubes)
                    for (auto& pt : tube.path) {
                        V3 d = middle - pt.pos;
                        if (float len = dot(d, d); len < best) {
                            best = len;
                            out = d;
                        }
                    }
                // only what is across the card counts, along it the nearest ring is an accident of the path
                int thin = cube.size.x <= cube.size.y && cube.size.x <= cube.size.z ? 0 : cube.size.y <= cube.size.z ? 1 : 2;
                V3 across = withAxis({}, thin, axisOf(out, thin));
                V3 seen = rotateAxis(rotateAxis(place.normal(across), 1, yaw), 0, pitch);
                bool front = seen.z > 0.f;
                for (size_t k = cardFrom; k < draws.size(); k++)
                    draws[k].depth = front ? std::max(draws[k].depth, tubeNear + 0.01f) : std::min(draws[k].depth, tubeFar - 0.01f);
            }
            place.order = nullptr;
        }
    }

    if (look.mesh) return;

    // Something worn close to a body part (ears on the side of the head, a headband, a shoe) is a hair in front of
    // one of its faces, closer than the sort by quad middles can tell. A box cannot hide what lies outside one of the
    // faces it turns to the viewer, so such a quad is drawn after the whole part, keeping its order among its like.
    for (size_t k = figureEnd; k < draws.size(); k++) {
        for (int i = 0; i < 6; i++) {
            const Part& b = figure[i];
            const Solid& solid = solids[i];
            V3 p = draws[k].middle;
            if (i >= 2) {
                const Place& limb = limbs[i - 2];
                const float back[3] = {-limb.deg[0], 0.f, 0.f};
                p = limb.bone->pivot + rotateXyz(p - limb.bone->pivot, back);
            }
            float over[6] = {p.z - (b.o.z + b.s.z), b.o.z - p.z, p.x - (b.o.x + b.s.x), b.o.x - p.x, p.y - (b.o.y + b.s.y), b.o.y - p.y};
            float apart = 0.f;
            bool outside = false;
            for (int f = 0; f < 6; f++) {
                if (over[f] <= 0.f) continue;
                apart += over[f] * over[f];
                outside |= solid.facing[f];
            }
            if (!outside || apart > 1.5f * 1.5f || draws[k].depth > solid.nearest) continue;
            draws[k].depth = solid.nearest + 0.01f + (draws[k].depth - solid.farthest) * 0.01f;
        }
    }

    if (look.fitSize.x > 0.f && look.fitSize.y > 0.f && !draws.empty()) {
        ImVec2 lo = draws.front().p[0], hi = lo;
        for (const auto& draw : draws)
            for (auto p : draw.p) {
                lo.x = std::min(lo.x, p.x); lo.y = std::min(lo.y, p.y);
                hi.x = std::max(hi.x, p.x); hi.y = std::max(hi.y, p.y);
            }
        float fit = std::min({1.f, look.fitSize.x / std::max(1.f, hi.x - lo.x), look.fitSize.y / std::max(1.f, hi.y - lo.y)});
        ImVec2 middle{(lo.x + hi.x) * 0.5f, (lo.y + hi.y) * 0.5f};
        for (auto& draw : draws)
            for (auto& p : draw.p) p = {center.x + (p.x - middle.x) * fit, center.y + (p.y - middle.y) * fit};
    }
    paint(dl, draws, look.frame);
}

void drawFrame(ImDrawList* dl, const std::vector<PreviewFace>& draws, ImVec2 center, float scale) {
    auto& pio = ImGui::GetPlatformIO();
    bool canSwitch = pio.DrawCallback_SetSamplerNearest && pio.DrawCallback_SetSamplerLinear;
    bool nearestOn = false;
    for (auto& d : draws) {
        if (canSwitch && d.nearest != nearestOn) {
            dl->AddCallback(d.nearest ? pio.DrawCallback_SetSamplerNearest : pio.DrawCallback_SetSamplerLinear, nullptr);
            nearestOn = d.nearest;
        }
        ImVec2 p[4];
        for (int i = 0; i < 4; ++i) p[i] = {center.x + d.p[i].x * scale, center.y + d.p[i].y * scale};
        dl->AddImageQuad(d.tex, p[0], p[1], p[2], p[3], d.uv[0], d.uv[1], d.uv[2], d.uv[3], ImGui::GetColorU32(ImGui::ColorConvertU32ToFloat4(d.col)));
    }
    if (canSwitch && nearestOn) dl->AddCallback(pio.DrawCallback_SetSamplerLinear, nullptr);

}

std::vector<Quad> mesh(const std::vector<Worn>& worn, Rig& rig, bool slim) {
    std::vector<Quad> out;
    Look look;
    look.slim = slim;
    look.rig = &rig;
    look.mesh = &out;
    drawPreview(nullptr, {}, 1.f, 0.f, 0.f, worn, {1, 1, 1, 1}, look);
    return out;
}

}
