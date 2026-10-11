#include "cosmetics/GpuPreview.hpp"
#include "cosmetics/RetiredTextures.hpp"
#include <d3d11.h>
#include <wrl/client.h>
#include "cosmetics/Cosmetics.hpp"
#include "core/Paths.hpp"
#include "modules/post/Backdrop.hpp"
#include <imgui_internal.h>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>

namespace logger {
void write(std::string_view, std::string_view) {}
std::string narrow(std::wstring_view text) {
    const auto utf8 = std::filesystem::path(text).u8string();
    return {utf8.begin(), utf8.end()};
}
}
namespace paths {
fs::path catalogRoot;
const fs::path& root() { return catalogRoot; }
}

int main(int argc, char** argv) {
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.DisplaySize = {1024, 768};
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
    io.IniFilename = nullptr;
    {
        ImDrawList draw(ImGui::GetDrawListSharedData());
        auto callback = +[](const ImDrawList*, const ImDrawCmd*) {};
        for (bool needed : {false, true, false}) {
            draw._ResetForNewFrame();
            draw.PushClipRect({-4096, -4096}, {4096, 4096});
            draw.AddRectFilled({0, 0}, {8, 8}, IM_COL32_WHITE);
            post::Backdrop backdrop;
            backdrop.reserve(&draw, callback, ImDrawCallback_ResetRenderState);
            int first = backdrop.first;
            draw.AddRectFilled({16, 0}, {24, 8}, IM_COL32_WHITE);
            int vertices = draw.VtxBuffer.Size, indices = draw.IdxBuffer.Size;
            int commands = draw.CmdBuffer.Size;
            backdrop.finish(needed);
            assert(draw.VtxBuffer.Size == vertices && draw.IdxBuffer.Size == indices);
            assert(draw.CmdBuffer.Size == commands - (needed ? 0 : 2));
            assert(draw.CmdBuffer[first].UserCallback == (needed ? callback : nullptr));
            assert(draw.CmdBuffer.back().ElemCount == 6);
            assert(draw.CmdBuffer.back().IdxOffset == 6);
            backdrop.finish(false);
            assert(draw.CmdBuffer.Size == commands - (needed ? 0 : 2));
        }
        draw._ResetForNewFrame();
        post::Backdrop backdrop;
        backdrop.reserve(&draw, callback, ImDrawCallback_ResetRenderState);
        backdrop.finish(false);
        draw._PopUnusedDrawCmd();
        assert(draw.CmdBuffer.empty());
    }
    {
        int registered = GImGui->UserTextures.Size;
        auto* texture = IM_NEW(ImTextureData)();
        texture->Create(ImTextureFormat_RGBA32, 64, 64);
        ImGui::RegisterUserTexture(texture);
        texture->SetStatus(ImTextureStatus_OK);
        cosmetics::textures::retire(texture);
        cosmetics::textures::retire(texture);
        cosmetics::textures::collect();
        assert(GImGui->UserTextures.Size == registered + 1);
        assert(cosmetics::textures::retired.size() == 1);
        texture->SetStatus(ImTextureStatus_Destroyed);
        ImGui::GetPlatformIO().Textures.push_back(texture);
        cosmetics::textures::collect();
        assert(GImGui->UserTextures.Size == registered);
        assert(!ImGui::GetPlatformIO().Textures.contains(texture));
        assert(cosmetics::textures::retired.empty());
    }

    {
    ImDrawList direct(ImGui::GetDrawListSharedData()), cached(ImGui::GetDrawListSharedData());
    auto reset = [](ImDrawList& dl) { dl._ResetForNewFrame(); dl.PushClipRect({-4096, -4096}, {4096, 4096}); };
    cosmetics::Item sample;
    sample.id = "sample";
    cosmetics::Bone bone;
    bone.cubes.push_back({{10, 10, 10}, {8, 12, 4}, {2, 2, 2}, 0, 0, 0});
    sample.bones.push_back(bone);
    std::vector<cosmetics::Worn> worn{{&sample, {}}};
    std::vector<cosmetics::PreviewFace> frame;
    cosmetics::Look look;
    look.slim = true;
    look.frame = &frame;
    cosmetics::drawPreview(nullptr, {}, 2.5f, 25, 10, worn, {0, 1, 0, 1}, look);
    look.frame = nullptr;
    auto opaquePixels = std::vector<unsigned char>(frame.front().image->Pixels, frame.front().image->Pixels + frame.front().image->Width * frame.front().image->Height * 4);
    ImGui::GetStyle().Alpha = 0.15f;
    look.frame = &frame;
    cosmetics::drawPreview(nullptr, {}, 2.5f, 25, 10, worn, {0, 1, 0, 1}, look);
    assert(std::equal(opaquePixels.begin(), opaquePixels.end(), frame.front().image->Pixels));
    ImGui::GetStyle().Alpha = 1.f;
    look.frame = nullptr;
    for (float scale : {0.75f, 1.f, 1.5f}) {
        look.frame = &frame;
        cosmetics::drawPreview(nullptr, {}, 2.5f * scale, 25, 10, worn, {0, 1, 0, 1}, look);
        look.frame = nullptr;
        reset(direct); reset(cached);
        cosmetics::drawPreview(&direct, {120, 240}, 2.5f * scale, 25, 10, worn, {0, 1, 0, 1}, look);
        cosmetics::drawFrame(&cached, frame, {120, 240}, 1.f);
        assert(direct.VtxBuffer.Size == cached.VtxBuffer.Size);
        assert(direct.IdxBuffer.Size == cached.IdxBuffer.Size);
        for (int i = 0; i < direct.VtxBuffer.Size; ++i) {
            const auto& a = direct.VtxBuffer[i]; const auto& b = cached.VtxBuffer[i];
            assert(std::fabs(a.pos.x - b.pos.x) < 0.001f && std::fabs(a.pos.y - b.pos.y) < 0.001f);
            assert(a.col == b.col && a.uv.x == b.uv.x && a.uv.y == b.uv.y);
        }
        assert(frame.size() == 1 && frame.front().image);
        auto* raster = direct.CmdBuffer.front().TexRef._TexData;
        auto* saved = frame.front().image.get();
        assert(raster->Width == saved->Width && raster->Height == saved->Height);
        const size_t bytes = size_t(raster->Width) * raster->Height * 4;
        double difference = 0;
        for (size_t i = 0; i < bytes; ++i) difference += std::abs(int(raster->Pixels[i]) - int(saved->Pixels[i]));
        assert(difference / bytes < 2.0);
    }
    auto* heldImage = frame.front().image.get();
    std::weak_ptr<ImTextureData> held = frame.front().image;
    frame.clear();
    assert(held.expired() && heldImage->WantDestroyNextFrame);
    ImGui::NewFrame();
    ImGui::SetNextWindowSize({600, 600});
    ImGui::Begin("grid");
    ImGuiListClipper clipper;
    clipper.Begin(500, 190.f);
    int visited = 0;
    while (clipper.Step())
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
            visited++;
            ImGui::Dummy({400, 176});
        }
    assert(visited > 0 && visited < 8);
    ImGui::End();
    ImGui::Render();
    if (argc > 1) {
        paths::catalogRoot = argv[1];
        cosmetics::reload();
        const auto& items = cosmetics::items();
        std::vector<std::vector<cosmetics::PreviewFace>> thumbnails(std::min<size_t>(8, items.size()));
        for (size_t i = 0; i < thumbnails.size(); ++i) {
            look.frame = &thumbnails[i];
            cosmetics::drawPreview(nullptr, {}, 2.5f, 155, 10, {{&items[i], {}}}, {0, 1, 0, 1}, look);
        }
        look.frame = nullptr;
        auto measure = [&](bool cache) {
            auto start = std::chrono::steady_clock::now();
            for (int pass = 0; pass < 20; ++pass) {
                ImGui::NewFrame();
                reset(direct);
                if (cache) for (const auto& thumb : thumbnails) cosmetics::drawFrame(&direct, thumb, {}, 1);
                else for (const auto& item : items) cosmetics::drawPreview(&direct, {}, 2.5f, 155, 10, {{&item, {}}}, {0, 1, 0, 1}, look);
                ImGui::Render();
            }
            return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / 20;
        };
        std::printf("catalog %zu, visible %zu, all uncached %.3f ms/frame, visible cached %.3f ms/frame\n",
                    items.size(), thumbnails.size(), measure(false), measure(true));
        std::vector<cosmetics::Worn> outfit;
        for (const char* id : {"angel_wings", "pet_dragon", "hare_ears", "dog_shoes"})
            if (auto* item = cosmetics::find(id)) outfit.push_back({item, {}});
        if (outfit.empty() && !items.empty()) outfit.push_back({&items.front(), {}});
        look.fitSize = {320, 480};
        auto stage = [&] {
            auto start = std::chrono::steady_clock::now();
            for (int i = 0; i < 5; ++i) {
                ImGui::NewFrame(); reset(direct);
                cosmetics::drawPreview(&direct, {200, 260}, 12.f, 25, 12, outfit, {1, 1, 1, 1}, look);
                ImGui::Render();
            }
            return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now()-start).count()/5;
        };
        double cpu = stage();
        Microsoft::WRL::ComPtr<ID3D11Device> device;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
        if (SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &device, nullptr, &context))) {
            cosmetics::gpu::init(device.Get(), context.Get());
            stage();
            std::printf("main preview CPU raster %.3f ms/frame, GPU submission %.3f ms/frame, outfit %zu\n", cpu, stage(), outfit.size());
            cosmetics::gpu::stop();
        }
    }
    }
    ImGui::DestroyContext();
}
