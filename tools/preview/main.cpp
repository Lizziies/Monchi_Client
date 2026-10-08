#include "Ui.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

struct Image {
    int w, h;
    std::vector<unsigned char> px;
};

static float edge(ImVec2 a, ImVec2 b, ImVec2 p) { return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x); }

static void blend(Image& img, int x, int y, float r, float g, float b, float a) {
    unsigned char* d = &img.px[(y * img.w + x) * 4];
    d[0] = (unsigned char)(r * a * 255.f + d[0] * (1.f - a));
    d[1] = (unsigned char)(g * a * 255.f + d[1] * (1.f - a));
    d[2] = (unsigned char)(b * a * 255.f + d[2] * (1.f - a));
    d[3] = 255;
}

static void raster(Image& img, ImDrawData* data, const unsigned char* atlas, int aw, int ah) {
    for (int n = 0; n < data->CmdListsCount; n++) {
        const ImDrawList* dl = data->CmdLists[n];
        for (const ImDrawCmd& cmd : dl->CmdBuffer) {
            if (cmd.UserCallback) continue;
            int x0 = std::max(0, (int)cmd.ClipRect.x), y0 = std::max(0, (int)cmd.ClipRect.y);
            int x1 = std::min(img.w, (int)std::ceil(cmd.ClipRect.z)), y1 = std::min(img.h, (int)std::ceil(cmd.ClipRect.w));
            for (unsigned i = 0; i + 2 < cmd.ElemCount; i += 3) {
                const ImDrawVert& v0 = dl->VtxBuffer[dl->IdxBuffer[cmd.IdxOffset + i] + cmd.VtxOffset];
                const ImDrawVert& v1 = dl->VtxBuffer[dl->IdxBuffer[cmd.IdxOffset + i + 1] + cmd.VtxOffset];
                const ImDrawVert& v2 = dl->VtxBuffer[dl->IdxBuffer[cmd.IdxOffset + i + 2] + cmd.VtxOffset];
                float area = edge(v0.pos, v1.pos, v2.pos);
                if (std::fabs(area) < 1e-6f) continue;
                int minx = std::max(x0, (int)std::floor(std::min({v0.pos.x, v1.pos.x, v2.pos.x})));
                int maxx = std::min(x1 - 1, (int)std::ceil(std::max({v0.pos.x, v1.pos.x, v2.pos.x})));
                int miny = std::max(y0, (int)std::floor(std::min({v0.pos.y, v1.pos.y, v2.pos.y})));
                int maxy = std::min(y1 - 1, (int)std::ceil(std::max({v0.pos.y, v1.pos.y, v2.pos.y})));
                for (int y = miny; y <= maxy; y++) {
                    for (int x = minx; x <= maxx; x++) {
                        ImVec2 p{x + 0.5f, y + 0.5f};
                        float w0 = edge(v1.pos, v2.pos, p) / area;
                        float w1 = edge(v2.pos, v0.pos, p) / area;
                        float w2 = edge(v0.pos, v1.pos, p) / area;
                        if (w0 < -0.0005f || w1 < -0.0005f || w2 < -0.0005f) continue;
                        float u = w0 * v0.uv.x + w1 * v1.uv.x + w2 * v2.uv.x;
                        float v = w0 * v0.uv.y + w1 * v1.uv.y + w2 * v2.uv.y;
                        int tx = std::clamp((int)(u * aw), 0, aw - 1), ty = std::clamp((int)(v * ah), 0, ah - 1);
                        float t = atlas[ty * aw + tx] / 255.f;
                        auto ch = [&](int s) {
                            float c0 = (v0.col >> s & 255) / 255.f, c1 = (v1.col >> s & 255) / 255.f, c2 = (v2.col >> s & 255) / 255.f;
                            return w0 * c0 + w1 * c1 + w2 * c2;
                        };
                        float a = ch(24) * t;
                        if (a > 0.f) blend(img, x, y, ch(0), ch(8), ch(16), a);
                    }
                }
            }
        }
    }
}

static void writePpm(const Image& img, const char* path) {
    FILE* f = std::fopen(path, "wb");
    std::fprintf(f, "P6\n%d %d\n255\n", img.w, img.h);
    for (int i = 0; i < img.w * img.h; i++) std::fwrite(&img.px[i * 4], 1, 3, f);
    std::fclose(f);
}

static std::string slurp(const char* path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), {});
}

int main(int argc, char** argv) {
    if (argc < 5) {
        std::printf("usage: preview <assets/fonts> <out.ppm> <page> <mouseX,mouseY|-> [phase]\n");
        return 1;
    }
    std::string fonts = argv[1];
    std::string regularData = slurp((fonts + "/Poppins-Regular.ttf").c_str());
    std::string boldData = slurp((fonts + "/Poppins-Medium.ttf").c_str());

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = {ui::width, ui::height};
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

    ImFontConfig cfg;
    cfg.FontDataOwnedByAtlas = false;
    ImFont* regular = io.Fonts->AddFontFromMemoryTTF(regularData.data(), (int)regularData.size(), 17.f, &cfg);
    ImFont* bold = io.Fonts->AddFontFromMemoryTTF(boldData.data(), (int)boldData.size(), 17.f, &cfg);
    ui::setFonts(regular, bold);

    ui::State state;
    state.clientVersion = "0.1.0";
    state.latestVersion = "0.2.0";
    state.updateAvailable = std::getenv("UPDATE") != nullptr;
    state.gameVersion = "1.26.31";
    state.managerInstalled = std::getenv("MANAGER") != nullptr;
    state.changelog = "Neues Menü mit weichem Scrollen\nLatenz-Overlay und WLAN-Modul\nNo View Bobbing\nServer-Regeln für Hive und Lifeboat\nBessere Standardwerte für FPS";
    state.versions = {{"1.26.31", false, true, true, false, true, {}},
                      {"1.26.20", false, true, true, true, false, "C:\\Users\\<user>\\AppData\\Local\\Programs\\LeviLauncher\\versions\\1.26.20\\Minecraft.Windows.exe"},
                      {"1.26.0", false, true, true, false, false, "D:\\Games\\Minecraft\\1.26.0\\Minecraft.Windows.exe"},
                      {"1.26.40", true, true, false, false, false, "C:\\Users\\<user>\\AppData\\Local\\Programs\\LeviLauncher\\versions\\1.26.40 Preview\\Minecraft.Windows.exe"}};
    std::string page = argv[3];
    state.page = page == "versions" ? ui::Page::Versions : page == "settings" ? ui::Page::Settings : page == "about" ? ui::Page::About : ui::Page::Start;
    if (argc > 5) {
        std::string ph = argv[5];
        if (ph == "starting") { state.phase = ui::Phase::Starting; state.status = "Minecraft wird gestartet"; state.progress = 0.f; }
        if (ph == "updating") { state.phase = ui::Phase::Updating; state.status = "Lade Monchi 0.2.0 herunter"; state.progress = 0.62f; }
        if (ph == "done") { state.phase = ui::Phase::Done; state.status = "Monchi ist verbunden. Viel Spaß!"; }
    }

    float mx = -1, my = -1;
    if (std::strcmp(argv[4], "-") != 0) std::sscanf(argv[4], "%f,%f", &mx, &my);

    Image img{(int)ui::width, (int)ui::height, std::vector<unsigned char>((int)(ui::width * ui::height * 4), 0)};
    for (int i = 0; i < 90; i++) {
        io.DeltaTime = 1.f / 60.f;
        io.AddMousePosEvent(mx, my);
        ImGui::NewFrame();
        ui::Events ev;
        ui::draw(state, ev);
        ImGui::Render();
        if (i == 89) {
            for (ImTextureData* tex : ImGui::GetPlatformIO().Textures) {
                if (tex->Status == ImTextureStatus_WantCreate || tex->Status == ImTextureStatus_WantUpdates) tex->SetStatus(ImTextureStatus_OK);
            }
            ImTextureData* tex = io.Fonts->TexData;
            std::fill(img.px.begin(), img.px.end(), 0);
            for (int p = 0; p < img.w * img.h; p++) { img.px[p * 4] = 26; img.px[p * 4 + 1] = 15; img.px[p * 4 + 2] = 30; img.px[p * 4 + 3] = 255; }
            std::vector<unsigned char> alpha(tex->Width * tex->Height);
            for (int p = 0; p < tex->Width * tex->Height; p++) alpha[p] = tex->Format == ImTextureFormat_Alpha8 ? tex->Pixels[p] : tex->Pixels[p * 4 + 3];
            raster(img, ImGui::GetDrawData(), alpha.data(), tex->Width, tex->Height);
        } else {
            for (ImTextureData* tex : ImGui::GetPlatformIO().Textures)
                if (tex->Status == ImTextureStatus_WantCreate || tex->Status == ImTextureStatus_WantUpdates) tex->SetStatus(ImTextureStatus_OK);
        }
    }
    writePpm(img, argv[2]);
    return 0;
}
