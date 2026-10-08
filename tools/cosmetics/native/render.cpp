// Renders cosmetics with the client's own loader and preview (dll/src/cosmetics) on Linux, so a change to
// the C++ renderer can be checked without Windows. Writes one PPM per frame.
// usage: MOCHI_ROOT=<dir with cosmetics/> render <out prefix> <yaw> <motion idle|walk|sprint|jump> <frames> <id>...
#include "cosmetics/Cosmetics.hpp"
#include "core/Log.hpp"
#include "core/Paths.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace logger {
void write(std::string_view level, std::string_view msg) { std::fprintf(stderr, "[%.*s] %.*s\n", int(level.size()), level.data(), int(msg.size()), msg.data()); }
}

namespace paths {
const fs::path& root() {
    static fs::path p = std::getenv("MOCHI_ROOT") ? std::getenv("MOCHI_ROOT") : ".";
    return p;
}
}

namespace {

constexpr int W = 480, H = 480;

struct Px {
    float r, g, b;
};

void raster(const ImDrawList* dl, std::vector<Px>& img) {
    for (const ImDrawCmd& cmd : dl->CmdBuffer) {
        if (cmd.UserCallback) continue;
        ImTextureData* tex = cmd.TexRef._TexData;
        for (unsigned i = 0; i + 2 < cmd.ElemCount; i += 3) {
            const ImDrawVert* v[3];
            for (int k = 0; k < 3; k++) v[k] = &dl->VtxBuffer[cmd.VtxOffset + dl->IdxBuffer[cmd.IdxOffset + i + k]];
            float x0 = std::min({v[0]->pos.x, v[1]->pos.x, v[2]->pos.x}), x1 = std::max({v[0]->pos.x, v[1]->pos.x, v[2]->pos.x});
            float y0 = std::min({v[0]->pos.y, v[1]->pos.y, v[2]->pos.y}), y1 = std::max({v[0]->pos.y, v[1]->pos.y, v[2]->pos.y});
            float area = (v[1]->pos.x - v[0]->pos.x) * (v[2]->pos.y - v[0]->pos.y) - (v[2]->pos.x - v[0]->pos.x) * (v[1]->pos.y - v[0]->pos.y);
            if (std::fabs(area) < 1e-6f) continue;
            for (int y = std::max(0, int(y0)); y <= std::min(H - 1, int(y1)); y++)
                for (int x = std::max(0, int(x0)); x <= std::min(W - 1, int(x1)); x++) {
                    float px = x + 0.5f, py = y + 0.5f;
                    float w0 = ((v[1]->pos.x - px) * (v[2]->pos.y - py) - (v[2]->pos.x - px) * (v[1]->pos.y - py)) / area;
                    float w1 = ((v[2]->pos.x - px) * (v[0]->pos.y - py) - (v[0]->pos.x - px) * (v[2]->pos.y - py)) / area;
                    float w2 = 1.f - w0 - w1;
                    if (w0 < 0 || w1 < 0 || w2 < 0) continue;
                    float u = w0 * v[0]->uv.x + w1 * v[1]->uv.x + w2 * v[2]->uv.x, t = w0 * v[0]->uv.y + w1 * v[1]->uv.y + w2 * v[2]->uv.y;
                    ImVec4 c = ImGui::ColorConvertU32ToFloat4(v[0]->col);
                    float tr = 1, tg = 1, tb = 1, ta = 1;
                    if (tex && tex->Pixels) {
                        int tx = std::clamp(int(u * tex->Width), 0, tex->Width - 1), ty = std::clamp(int(t * tex->Height), 0, tex->Height - 1);
                        const unsigned char* p = (const unsigned char*)tex->GetPixelsAt(tx, ty);
                        tr = p[0] / 255.f; tg = p[1] / 255.f; tb = p[2] / 255.f; ta = p[3] / 255.f;
                    }
                    float a = c.w * ta;
                    if (a < 0.01f) continue;
                    Px& d = img[size_t(y) * W + size_t(x)];
                    d = {d.r + (c.x * tr - d.r) * a, d.g + (c.y * tg - d.g) * a, d.b + (c.z * tb - d.b) * a};
                }
        }
    }
}

}

int main(int argc, char** argv) {
    if (argc < 6) return 1;
    std::string out = argv[1];
    float yaw = std::strtof(argv[2], nullptr);
    std::string motion = argv[3];
    int frames = std::atoi(argv[4]);

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = {float(W), float(H)};
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
    io.IniFilename = nullptr;
    cosmetics::reload();

    std::vector<cosmetics::Worn> worn;
    for (int i = 5; i < argc; i++)
        if (auto* it = cosmetics::find(argv[i])) worn.push_back({it, {}});
        else std::fprintf(stderr, "missing %s\n", argv[i]);

    cosmetics::Rig rig;
    cosmetics::Moving m;
    if (motion == "walk") m.fwd = 4.3f;
    if (motion == "sprint") { m.fwd = 5.6f; m.sprint = true; }
    float unit = std::getenv("UNIT") ? std::strtof(std::getenv("UNIT"), nullptr) : 8.f;
    float focus = std::getenv("FOCUS") ? std::strtof(std::getenv("FOCUS"), nullptr) : 18.f;
    int every = std::getenv("EVERY") ? std::atoi(std::getenv("EVERY")) : frames;
    for (int f = 1; f <= frames; f++) {
        if (motion == "jump") {
            float k = std::fmod(f / 60.f, 1.4f);
            m.fwd = 1.5f;
            m.air = k < 0.8f;
            m.up = m.air ? 7.f * (1.f - k / 0.4f) : 0.f;
        }
        rig.step(1.f / 60.f, m);
        io.DeltaTime = 1.f / 60.f;
        for (ImTextureData* td : ImGui::GetPlatformIO().Textures)
            if (td->Status == ImTextureStatus_WantCreate || td->Status == ImTextureStatus_WantUpdates) td->SetStatus(ImTextureStatus_OK);
        ImGui::NewFrame();
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        cosmetics::drawPreview(dl, {W * 0.5f, H * 0.62f}, unit, yaw, 8.f, worn, {0.93f, 0.45f, 0.65f, 1.f}, {false, focus, &rig});
        ImGui::Render();
        if (f % every) continue;
        std::vector<Px> img(size_t(W) * H, {0.98f, 0.89f, 0.93f});
        raster(dl, img);
        std::string name = out + "_" + std::to_string(f / every) + ".ppm";
        FILE* fp = std::fopen(name.c_str(), "wb");
        std::fprintf(fp, "P6 %d %d 255\n", W, H);
        for (auto& p : img) {
            unsigned char rgb[3] = {(unsigned char)std::clamp(p.r * 255.f, 0.f, 255.f), (unsigned char)std::clamp(p.g * 255.f, 0.f, 255.f), (unsigned char)std::clamp(p.b * 255.f, 0.f, 255.f)};
            std::fwrite(rgb, 1, 3, fp);
        }
        std::fclose(fp);
    }
    return 0;
}
