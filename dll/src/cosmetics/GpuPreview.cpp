#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "GpuPreview.hpp"
#include <algorithm>
#include <cfloat>
#include <cstring>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <unordered_map>
#include <vector>
#include <wrl/client.h>

namespace cosmetics::gpu {
namespace {
using Microsoft::WRL::ComPtr;
struct Vertex {
    float x, y, z, u, v;
    unsigned color;
};
struct Target {
    ComPtr<ID3D11Texture2D> color, output, depth;
    ComPtr<ID3D11RenderTargetView> colorView, outputView;
    ComPtr<ID3D11ShaderResourceView> colorRead, outputRead;
    ComPtr<ID3D11DepthStencilView> depthView;
    int w = 0, h = 0, used = -1;
};
ComPtr<ID3D11Device> device;
ComPtr<ID3D11DeviceContext> immediate, deferred;
ComPtr<ID3D11VertexShader> vs;
ComPtr<ID3D11PixelShader> ps, resolve;
ComPtr<ID3D11InputLayout> layout;
ComPtr<ID3D11Buffer> vertices;
ComPtr<ID3D11BlendState> blend;
ComPtr<ID3D11DepthStencilState> depth;
ComPtr<ID3D11RasterizerState> raster;
ComPtr<ID3D11SamplerState> point, linear;
std::unordered_map<const ImTextureData *, ComPtr<ID3D11ShaderResourceView>> sources;
std::vector<std::shared_ptr<Target>> targets;
size_t capacity = 0;
bool failed = false;
constexpr char shader[] = R"(
Texture2D image : register(t0); SamplerState texSampler : register(s0);
struct Input { float3 p:POSITION; float2 uv:TEXCOORD; float4 color:COLOR; };
struct Pixel { float4 p:SV_POSITION; float2 uv:TEXCOORD; float4 color:COLOR; };
Pixel vertex(Input i) { Pixel o; o.p=float4(i.p,1); o.uv=i.uv; o.color=i.color; return o; }
float4 pixel(Pixel i):SV_TARGET { float4 c=image.Sample(texSampler,i.uv)*i.color; clip(c.a-0.01); return c; }
float4 straight(Pixel i):SV_TARGET { float4 c=image.Sample(texSampler,i.uv); if(c.a>0) c.rgb/=c.a; return c; }
)";
bool programs() {
    if (vs)
        return true;
    ComPtr<ID3DBlob> v, p, r;
    if (FAILED(D3DCompile(shader, sizeof(shader), nullptr, nullptr, nullptr, "vertex", "vs_4_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &v,
                          nullptr)) ||
        FAILED(D3DCompile(shader, sizeof(shader), nullptr, nullptr, nullptr, "pixel", "ps_4_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &p,
                          nullptr)) ||
        FAILED(D3DCompile(shader, sizeof(shader), nullptr, nullptr, nullptr, "straight", "ps_4_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &r,
                          nullptr)))
        return false;
    D3D11_INPUT_ELEMENT_DESC elements[] = {{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
                                           {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
                                           {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0}};
    if (FAILED(device->CreateVertexShader(v->GetBufferPointer(), v->GetBufferSize(), nullptr, &vs)) ||
        FAILED(device->CreatePixelShader(p->GetBufferPointer(), p->GetBufferSize(), nullptr, &ps)) ||
        FAILED(device->CreatePixelShader(r->GetBufferPointer(), r->GetBufferSize(), nullptr, &resolve)) ||
        FAILED(device->CreateInputLayout(elements, 3, v->GetBufferPointer(), v->GetBufferSize(), &layout)))
        return false;
    D3D11_BLEND_DESC b{};
    b.RenderTarget[0].BlendEnable = TRUE;
    auto &rt = b.RenderTarget[0];
    rt.SrcBlend = D3D11_BLEND_SRC_ALPHA;
    rt.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    rt.BlendOp = D3D11_BLEND_OP_ADD;
    rt.SrcBlendAlpha = D3D11_BLEND_ONE;
    rt.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
    rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    D3D11_DEPTH_STENCIL_DESC d{};
    d.DepthEnable = TRUE;
    d.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    d.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    D3D11_RASTERIZER_DESC ras{};
    ras.FillMode = D3D11_FILL_SOLID;
    ras.CullMode = D3D11_CULL_NONE;
    ras.DepthClipEnable = TRUE;
    D3D11_SAMPLER_DESC sampler{};
    sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(device->CreateBlendState(&b, &blend)) || FAILED(device->CreateDepthStencilState(&d, &depth)) ||
        FAILED(device->CreateRasterizerState(&ras, &raster)) || FAILED(device->CreateSamplerState(&sampler, &point)))
        return false;
    sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    return SUCCEEDED(device->CreateSamplerState(&sampler, &linear));
}
std::shared_ptr<Target> target(int w, int h) {
    int frame = ImGui::GetFrameCount();
    std::erase_if(targets, [&](const auto &t) { return t.use_count() == 1 && frame - t->used > 2; });
    for (auto &t : targets)
        if (t.use_count() == 1 && t->used != frame && t->w == w && t->h == h) {
            t->used = frame;
            return t;
        }
    auto t = std::make_shared<Target>();
    t->w = w;
    t->h = h;
    t->used = frame;
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = w;
    desc.Height = h;
    desc.MipLevels = desc.ArraySize = 1;
    desc.SampleDesc.Count = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(device->CreateTexture2D(&desc, nullptr, &t->color)) || FAILED(device->CreateTexture2D(&desc, nullptr, &t->output)) ||
        FAILED(device->CreateRenderTargetView(t->color.Get(), nullptr, &t->colorView)) ||
        FAILED(device->CreateRenderTargetView(t->output.Get(), nullptr, &t->outputView)) ||
        FAILED(device->CreateShaderResourceView(t->color.Get(), nullptr, &t->colorRead)) ||
        FAILED(device->CreateShaderResourceView(t->output.Get(), nullptr, &t->outputRead)))
        return {};
    desc.Format = DXGI_FORMAT_D32_FLOAT;
    desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    if (FAILED(device->CreateTexture2D(&desc, nullptr, &t->depth)) ||
        FAILED(device->CreateDepthStencilView(t->depth.Get(), nullptr, &t->depthView)))
        return {};
    targets.push_back(t);
    return t;
}
ID3D11ShaderResourceView *source(const ImTextureData *data) {
    if (!data || !data->Pixels || data->Width <= 0 || data->Height <= 0)
        return nullptr;
    if (auto it = sources.find(data); it != sources.end())
        return it->second.Get();
    D3D11_TEXTURE2D_DESC d{};
    d.Width = data->Width;
    d.Height = data->Height;
    d.MipLevels = d.ArraySize = 1;
    d.SampleDesc.Count = 1;
    d.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    d.Usage = D3D11_USAGE_IMMUTABLE;
    d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA initial{data->Pixels, UINT(data->GetPitch()), 0};
    ComPtr<ID3D11Texture2D> t;
    ComPtr<ID3D11ShaderResourceView> view;
    if (FAILED(device->CreateTexture2D(&d, &initial, &t)) || FAILED(device->CreateShaderResourceView(t.Get(), nullptr, &view)))
        return nullptr;
    sources.emplace(data, view);
    return view.Get();
}
} // namespace
void forget(const ImTextureData *data) {
    sources.erase(data);
}
void stop() {
    targets.clear();
    sources.clear();
    vertices.Reset();
    layout.Reset();
    vs.Reset();
    ps.Reset();
    resolve.Reset();
    blend.Reset();
    depth.Reset();
    raster.Reset();
    point.Reset();
    linear.Reset();
    deferred.Reset();
    immediate.Reset();
    device.Reset();
    capacity = 0;
    failed = false;
}
void init(ID3D11Device *d, ID3D11DeviceContext *c) {
    stop();
    device = d;
    immediate = c;
    if (d && (FAILED(d->CreateDeferredContext(0, &deferred)) || !programs()))
        failed = true;
}
Image paint(std::span<const Face> faces, ImVec2 origin, int width, int height) {
    if (!device || !deferred || failed || faces.empty())
        return {};
    if (!programs()) {
        failed = true;
        return {};
    }
    auto out = target(width, height);
    if (!out)
        return {};
    float lo = FLT_MAX, hi = -FLT_MAX;
    for (auto &f : faces)
        for (float z : f.z) {
            lo = std::min(lo, z);
            hi = std::max(hi, z);
        }
    std::vector<Vertex> data;
    data.reserve(faces.size() * 6 + 3);
    constexpr int indices[] = {0, 1, 2, 0, 2, 3};
    for (auto &f : faces)
        for (int i : indices)
            data.push_back({2 * (f.p[i].x - origin.x) / width - 1, 1 - 2 * (f.p[i].y - origin.y) / height,
                            0.01f + 0.98f * (hi - f.z[i]) / std::max(0.001f, hi - lo), f.uv[i].x, f.uv[i].y, f.color});
    data.insert(data.end(), {{-1, -1, 0, 0, 1, IM_COL32_WHITE}, {-1, 3, 0, 0, -1, IM_COL32_WHITE}, {3, -1, 0, 2, 1, IM_COL32_WHITE}});
    if (data.size() > capacity) {
        vertices.Reset();
        capacity = data.size() + 1024;
        D3D11_BUFFER_DESC d{};
        d.ByteWidth = UINT(capacity * sizeof(Vertex));
        d.Usage = D3D11_USAGE_DYNAMIC;
        d.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        d.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(device->CreateBuffer(&d, nullptr, &vertices))) {
            capacity = 0;
            return {};
        }
    }
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(deferred->Map(vertices.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        return {};
    std::memcpy(mapped.pData, data.data(), data.size() * sizeof(Vertex));
    deferred->Unmap(vertices.Get(), 0);
    float clear[] = {0, 0, 0, 0};
    deferred->ClearRenderTargetView(out->colorView.Get(), clear);
    deferred->ClearDepthStencilView(out->depthView.Get(), D3D11_CLEAR_DEPTH, 1, 0);
    auto rt = out->colorView.Get();
    deferred->OMSetRenderTargets(1, &rt, out->depthView.Get());
    deferred->OMSetBlendState(blend.Get(), nullptr, ~0u);
    deferred->OMSetDepthStencilState(depth.Get(), 0);
    D3D11_VIEWPORT viewport{0, 0, float(width), float(height), 0, 1};
    deferred->RSSetViewports(1, &viewport);
    deferred->RSSetState(raster.Get());
    deferred->IASetInputLayout(layout.Get());
    deferred->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    UINT stride = sizeof(Vertex), offset = 0;
    auto vb = vertices.Get();
    deferred->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    deferred->VSSetShader(vs.Get(), nullptr, 0);
    deferred->PSSetShader(ps.Get(), nullptr, 0);
    for (size_t i = 0; i < faces.size();) {
        auto &f = faces[i];
        auto tex = source(f.texture);
        if (!tex) {
            ComPtr<ID3D11CommandList> discarded;
            deferred->FinishCommandList(FALSE, &discarded);
            failed = true;
            return {};
        }
        size_t end = i + 1;
        while (end < faces.size() && faces[end].texture == f.texture && faces[end].nearest == f.nearest)
            ++end;
        auto sampler = f.nearest ? point.Get() : linear.Get();
        deferred->PSSetSamplers(0, 1, &sampler);
        deferred->PSSetShaderResources(0, 1, &tex);
        deferred->Draw(UINT((end - i) * 6), UINT(i * 6));
        i = end;
    }
    ID3D11ShaderResourceView *empty = nullptr;
    deferred->PSSetShaderResources(0, 1, &empty);
    rt = out->outputView.Get();
    deferred->OMSetRenderTargets(1, &rt, nullptr);
    deferred->OMSetBlendState(nullptr, nullptr, ~0u);
    deferred->OMSetDepthStencilState(nullptr, 0);
    deferred->PSSetShader(resolve.Get(), nullptr, 0);
    auto tex = out->colorRead.Get();
    auto sampler = point.Get();
    deferred->PSSetShaderResources(0, 1, &tex);
    deferred->PSSetSamplers(0, 1, &sampler);
    deferred->Draw(3, UINT(faces.size() * 6));
    deferred->PSSetShaderResources(0, 1, &empty);
    ComPtr<ID3D11CommandList> commands;
    if (FAILED(deferred->FinishCommandList(FALSE, &commands)))
        return {};
    immediate->ExecuteCommandList(commands.Get(), TRUE);
    return {ImTextureID(reinterpret_cast<uintptr_t>(out->outputRead.Get())), out};
}
} // namespace cosmetics::gpu
