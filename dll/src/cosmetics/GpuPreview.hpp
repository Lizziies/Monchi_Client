#pragma once
#include <imgui.h>
#include <memory>
#include <span>
struct ID3D11Device;
struct ID3D11DeviceContext;
namespace cosmetics::gpu {
struct Face {
    ImVec2 p[4], uv[4];
    float z[4];
    ImU32 color;
    const ImTextureData *texture;
    bool nearest;
};
struct Image {
    ImTextureID texture = 0;
    std::shared_ptr<void> owner;
};
void init(ID3D11Device *device, ID3D11DeviceContext *context);
void stop();
void forget(const ImTextureData *texture);
Image paint(std::span<const Face> faces, ImVec2 origin, int width, int height);
} // namespace cosmetics::gpu
