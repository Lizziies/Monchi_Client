#include "cosmetics/GpuPreview.hpp"
#include <cassert>
#include <cstdio>
#include <d3d11.h>
#include <d3d11on12.h>
#include <d3d12.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
int main(int argc, char**) {
    ImGui::CreateContext();
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<ID3D12Device> device12;
    ComPtr<ID3D12CommandQueue> queue;
    if (argc > 1) {
        assert(SUCCEEDED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device12))));
        D3D12_COMMAND_QUEUE_DESC desc{};
        assert(SUCCEEDED(device12->CreateCommandQueue(&desc, IID_PPV_ARGS(&queue))));
        IUnknown* queues[] = {queue.Get()};
        assert(SUCCEEDED(D3D11On12CreateDevice(device12.Get(), 0, nullptr, 0, queues, 1, 0, &device, &context, nullptr)));
    } else {
        assert(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &device, nullptr, &context)));
    }
    cosmetics::gpu::init(device.Get(), context.Get());
    ImTextureData white;
    white.Create(ImTextureFormat_RGBA32, 1, 1);
    for (int i = 0; i < 4; ++i)
        white.Pixels[i] = 255;
    auto quad = [&](unsigned color, float z) {
        cosmetics::gpu::Face f{};
        f.p[0] = {0, 0};
        f.p[1] = {32, 0};
        f.p[2] = {32, 32};
        f.p[3] = {0, 32};
        for (int i = 0; i < 4; ++i) {
            f.uv[i] = {.5f, .5f};
            f.z[i] = z;
        }
        f.color = color;
        f.texture = &white;
        f.nearest = true;
        return f;
    };
    auto read = [&](const cosmetics::gpu::Image &image) {
        auto *srv = reinterpret_cast<ID3D11ShaderResourceView *>(uintptr_t(image.texture));
        ComPtr<ID3D11Resource> resource;
        srv->GetResource(&resource);
        ComPtr<ID3D11Texture2D> tex;
        resource.As(&tex);
        D3D11_TEXTURE2D_DESC desc{};
        tex->GetDesc(&desc);
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        ComPtr<ID3D11Texture2D> staging;
        assert(SUCCEEDED(device->CreateTexture2D(&desc, nullptr, &staging)));
        context->CopyResource(staging.Get(), tex.Get());
        D3D11_MAPPED_SUBRESOURCE m{};
        assert(SUCCEEDED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &m)));
        auto *p = static_cast<unsigned char *>(m.pData) + 16 * m.RowPitch + 16 * 4;
        unsigned result = p[0] | unsigned(p[1]) << 8 | unsigned(p[2]) << 16 | unsigned(p[3]) << 24;
        context->Unmap(staging.Get(), 0);
        return result;
    };
    D3D11_VIEWPORT original{3, 4, 17, 29, 0, 1};
    context->RSSetViewports(1, &original);
    cosmetics::gpu::Face faces[] = {quad(IM_COL32(255, 0, 0, 255), 1), quad(IM_COL32(0, 0, 255, 255), 0)};
    auto image = cosmetics::gpu::paint(faces, {}, 32, 32);
    assert(image.owner);
    assert(read(image) == IM_COL32(255, 0, 0, 255));
    UINT count = 1;
    D3D11_VIEWPORT after{};
    context->RSGetViewports(&count, &after);
    assert(after.TopLeftX == 3 && after.TopLeftY == 4 && after.Width == 17 && after.Height == 29);
    auto translucent = quad(IM_COL32(255, 0, 0, 128), 0);
    auto alpha = cosmetics::gpu::paint(std::span(&translucent, 1), {}, 32, 32);
    assert(alpha.owner && alpha.texture != image.texture);
    unsigned pixel = read(alpha);
    assert((pixel & 255) >= 254 && ((pixel >> 24) & 255) == 128);
    assert(read(image) == IM_COL32(255, 0, 0, 255));
    alpha.owner.reset();
    image.owner.reset();
    cosmetics::gpu::stop();
    ImGui::DestroyContext();
    std::puts("gpu depth, straight alpha and state restore passed");
}
