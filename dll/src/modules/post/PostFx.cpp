#include "PostFx.hpp"
#include "Backdrop.hpp"
#include "core/Bg.hpp"
#include "core/Guard.hpp"
#include "core/Log.hpp"
#include "ShaderPresets.hpp"
#include "core/Paths.hpp"
#include "render/Ui.hpp"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <imgui_impl_dx11.h>

#include <atomic>
#include <cmath>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>

namespace post {

static const char* shaderSource = R"(
cbuffer P : register(b0)
{
    float4 a;
    float4 b;
    float4 tint;
    float4 c;
    float4 d;
    float4 e;
    float4 night;
    float4 f;
    float4 g;
    float4 h;
};

Texture2D src : register(t0);
Texture2D prev : register(t1);
SamplerState smp : register(s0);

struct VOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };

VOut vs(uint id : SV_VertexID)
{
    VOut o;
    float2 uv = float2((id << 1) & 2, id & 2);
    o.pos = float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
    o.uv = uv;
    return o;
}

float3 tap(float2 uv) { return src.SampleLevel(smp, uv, 0).rgb; }

float luma(float3 v) { return dot(v, float3(0.299, 0.587, 0.114)); }

float3 hueRotate(float3 v, float angle)
{
    float s = sin(angle), k = cos(angle);
    float3x3 m = float3x3(
        0.213 + k * 0.787 - s * 0.213, 0.715 - k * 0.715 - s * 0.715, 0.072 - k * 0.072 + s * 0.928,
        0.213 - k * 0.213 + s * 0.143, 0.715 + k * 0.285 + s * 0.140, 0.072 - k * 0.072 - s * 0.283,
        0.213 - k * 0.213 - s * 0.787, 0.715 - k * 0.715 + s * 0.715, 0.072 + k * 0.928 + s * 0.072);
    return mul(m, v);
}

float hash(float2 p)
{
    return frac(sin(dot(p, float2(12.9898, 78.233))) * 43758.5453);
}

float3 blindMatrix(int mode, float3 v)
{
    int kind = (mode - 1) % 3;
    float3 sim = v;
    if (kind == 0) sim = float3(dot(v, float3(0.567, 0.433, 0.0)), dot(v, float3(0.558, 0.442, 0.0)), dot(v, float3(0.0, 0.242, 0.758)));
    if (kind == 1) sim = float3(dot(v, float3(0.625, 0.375, 0.0)), dot(v, float3(0.7, 0.3, 0.0)), dot(v, float3(0.0, 0.3, 0.7)));
    if (kind == 2) sim = float3(dot(v, float3(0.95, 0.05, 0.0)), dot(v, float3(0.0, 0.433, 0.567)), dot(v, float3(0.0, 0.475, 0.525)));
    if (mode >= 4) return sim;
    float3 err = v - sim;
    if (kind == 2) return v + float3(err.r + 0.7 * err.b, err.g + 0.7 * err.b, 0.0);
    return v + float3(0.0, 0.7 * err.r + err.g, 0.7 * err.r + err.b);
}

cbuffer Bl : register(b1)
{
    float4 rectPx;
    float4 shape;
    float4 tintCol;
    float4 texel2;
};

float4 psBlur(VOut i) : SV_Target
{
    float2 px = i.pos.xy;
    float2 mid = (rectPx.xy + rectPx.zw) * 0.5;
    float2 ext = (rectPx.zw - rectPx.xy) * 0.5;
    // shape.w turns the field around its centre; the mask is taken in the field's own unrotated frame
    float2 d = px - mid;
    float cs = cos(shape.w), sn = sin(shape.w);
    float2 local = float2(d.x * cs + d.y * sn, -d.x * sn + d.y * cs);
    float2 q = abs(local) - (ext - shape.y);
    float dist = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - shape.y;
    float mask = saturate(0.5 - dist);
    if (mask <= 0.0) discard;

    float3 acc = 0;
    float wsum = 0;
    float sigma = max(shape.x * 0.5, 0.5);
    [unroll] for (int k = 0; k < 28; k++)
    {
        float ang = k * 2.399963;
        float rad = sqrt((k + 0.5) / 28.0) * shape.x;
        float w = exp(-(rad * rad) / (2.0 * sigma * sigma));
        acc += src.SampleLevel(smp, (px + float2(cos(ang), sin(ang)) * rad) * texel2.xy, 0).rgb * w;
        wsum += w;
    }
    float3 col = lerp(acc / wsum, tintCol.rgb, tintCol.a);
    return float4(col, mask * shape.z);
}

float4 ps(VOut i) : SV_Target
{
    float2 uv = i.uv;
    int flip = (int)(b.w + 0.5);
    if (flip == 1 || flip == 3) uv.y = 1.0 - uv.y;
    if (flip >= 2) uv.x = 1.0 - uv.x;
    if (c.y > 1.0) uv = 0.5 + (uv - 0.5) / c.y;
    float2 texel = e.xy;
    float aspect = texel.y / texel.x;

    float3 col = tap(uv);
    if (f.x > 0.5)
    {
        float2 px = e.xy;
        float jitter = frac(52.9829189 * frac(dot(i.pos.xy, float2(0.06711056, 0.00583715))));
        float3 acc = 0;
        [loop] for (int k = 0; k < 40; k++)
        {
            float rad = sqrt((k + 0.5) / 40.0) * f.x;
            float ang = k * 2.399963 + jitter * 6.2831853;
            acc += tap(uv + float2(cos(ang), sin(ang)) * rad * px);
        }
        col = acc / 40.0;
    }

    if (h.z > 0.0)
    {
        float3 m = col;
        [unroll] for (int y = -2; y <= 2; y++)
            [unroll] for (int x = -2; x <= 2; x++)
                m = min(m, tap(uv + float2(x, y) * texel * h.z * 0.25));
        col = m;
    }

    if (b.y > 0.0)
    {
        float3 n = tap(uv + float2(texel.x, 0)) + tap(uv - float2(texel.x, 0)) + tap(uv + float2(0, texel.y)) + tap(uv - float2(0, texel.y));
        col = col + (col - n * 0.25) * b.y;
    }

    float2 dir = d.yz;
    int steps = (int)e.w;
    if (steps > 1 && dot(dir, dir) > 0.0)
    {
        float3 acc = 0;
        [loop] for (int s = 0; s < steps; s++)
            acc += tap(uv + dir * (s / (steps - 1.0) - 0.5));
        col = acc / steps;
    }

    float r = length((uv - 0.5) * float2(aspect, 1.0));
    // g: sharp radius, blur radius, focus height, band shape; h.x blur reach, h.y rings of eight taps
    float rf = g.w > 0.5 ? abs(uv.y - g.z) * 2.0 : length((uv - float2(0.5, g.z)) * float2(aspect, 1.0));
    float blur = d.x * smoothstep(g.x, max(g.y, g.x + 0.01), rf);
    if (blur > 0.0005)
    {
        int rings = clamp((int)h.y, 1, 3);
        float3 acc = col;
        [loop] for (int k = 0; k < rings * 8; k++)
        {
            int q = k >> 3;
            float ang = k * 0.785398 + q * 0.392699;
            float2 o = float2(cos(ang), sin(ang)) * blur * h.x * (1.0 - q / (float)rings);
            acc += tap(uv + o * float2(1.0 / aspect, 1.0));
        }
        col = acc / (rings * 8.0 + 1.0);
    }

    col = col + a.z;
    col = (col - 0.5) * a.w + 0.5;
    col = pow(saturate(col), 1.0 / max(b.x, 0.05));
    float l = luma(col);
    col = lerp(l.xxx, col, a.x);
    if (a.y != 0.0) col = hueRotate(col, a.y);

    if (b.z > 0.0)
    {
        col = (col - 0.5) * (1.0 + b.z * 2.5) + 0.5;
        float ll = luma(col);
        col = lerp(ll.xxx, col, 1.0 + b.z * 3.0);
        float levels = lerp(256.0, 6.0, b.z);
        col = floor(saturate(col) * levels + 0.5) / levels;
        col += (hash(uv * (1.0 / texel) + e.z) - 0.5) * 0.25 * b.z;
    }

    if (c.w >= 1.0) col = blindMatrix((int)c.w, col);

    col *= lerp(float3(1, 1, 1), night.rgb, night.a);

    if (tint.a > 0.0)
    {
        int m = (int)c.x;
        float3 t = tint.rgb;
        float3 res = lerp(col, t, tint.a);
        if (m == 1) res = lerp(col, col * t * 2.0, tint.a);
        if (m == 2) res = col + t * tint.a;
        if (m == 3) res = lerp(col, lerp(2.0 * col * t, 1.0 - 2.0 * (1.0 - col) * (1.0 - t), step(0.5, col)), tint.a);
        col = res;
    }

    if (c.z > 0.0)
    {
        float v = smoothstep(0.35, 1.05, r * 1.35);
        col *= 1.0 - c.z * v;
    }

    col = saturate(col);
    if (d.w > 0.0) col = lerp(col, prev.SampleLevel(smp, i.uv, 0).rgb, d.w);
    return float4(col, 1.0);
}
)";

struct Constants {
    float a[4], b[4], tint[4], c[4], d[4], e[4], night[4], f[4], g[4], h[4];
};

struct BlurJob {
    float rect[4];
    float rounding;
    float radius;
    float tint[4];
    float angle;
};

struct Gpu {
    ID3D11Device* dev = nullptr;
    ID3D11VertexShader* vs = nullptr;
    ID3D11PixelShader* ps = nullptr;
    ID3D11PixelShader* psBlur = nullptr;
    ID3D11Buffer* cb = nullptr;
    ID3D11Buffer* cbBlur = nullptr;
    ID3D11BlendState* alpha = nullptr;
    ID3D11RasterizerState* scissor = nullptr;
    ID3D11Buffer* ccb = nullptr;
    std::map<int, ID3D11PixelShader*> custom;
    std::map<int, std::string> customError;
    ID3D11SamplerState* sampler = nullptr;
    ID3D11BlendState* blend = nullptr;
    ID3D11RasterizerState* raster = nullptr;
    ID3D11DepthStencilState* depth = nullptr;
    ID3D11Texture2D* copy = nullptr;
    ID3D11ShaderResourceView* copyView = nullptr;
    // a view handed to the preview can sit in a draw list that is rendered after the texture was rebuilt, so the old
    // pair lives until the next frame begins
    ID3D11Texture2D* retired = nullptr;
    ID3D11ShaderResourceView* retiredView = nullptr;
    ID3D11BlendState* opaque = nullptr;
    bool frameWanted = false;
    ID3D11Texture2D* last = nullptr;
    ID3D11ShaderResourceView* lastView = nullptr;
    UINT width = 0, height = 0;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    bool lastValid = false;
    bool failed = false;
    bool blurCopied = false;
};

static Gpu gpu;
static std::deque<BlurJob> blurJobs;
static Backdrop backdrop;
static Params current;
static Params frameParams;
static float clock = 0.f;

template <class T>
static void release(T*& p) {
    if (p) p->Release();
    p = nullptr;
}

static void dropRetired() {
    release(gpu.retiredView);
    release(gpu.retired);
}

static void dropTextures() {
    dropRetired();
    gpu.retiredView = gpu.copyView;
    gpu.retired = gpu.copy;
    gpu.copyView = nullptr;
    gpu.copy = nullptr;
    release(gpu.lastView);
    release(gpu.last);
    gpu.lastValid = false;
    gpu.width = gpu.height = 0;
}

static void releaseCustomCompiles();

static void dropAll() {
    dropTextures();
    dropRetired();
    release(gpu.opaque);
    release(gpu.vs);
    release(gpu.ps);
    release(gpu.psBlur);
    release(gpu.cb);
    release(gpu.cbBlur);
    release(gpu.alpha);
    release(gpu.scissor);
    release(gpu.ccb);
    for (auto& [k, p] : gpu.custom) release(p);
    gpu.custom.clear();
    gpu.customError.clear();
    release(gpu.sampler);
    release(gpu.blend);
    release(gpu.raster);
    release(gpu.depth);
    gpu.dev = nullptr;
    gpu.failed = false;
}

// Compiling the big shader takes a noticeable moment, so it happens on a background thread right after
// loading instead of on the render thread the first time the menu (blur) or an effect needs it.
enum class Compile { Idle, Running, Ready, Failed };
static std::atomic<Compile> compileState{Compile::Idle};
static ID3DBlob* blobs[3] = {};
// the compiler is not guaranteed to be reentrant (Wine's is not), so compiles never overlap
static std::mutex compileLock;

static void compileShaders() {
    Compile expected = Compile::Idle;
    if (!compileState.compare_exchange_strong(expected, Compile::Running)) return;
    bg::run([] {
        static const char* entries[3][2] = {{"vs", "vs_5_0"}, {"ps", "ps_5_0"}, {"psBlur", "ps_5_0"}};
        std::scoped_lock g(compileLock);
        for (int i = 0; i < 3; i++) {
            ID3DBlob* err = nullptr;
            HRESULT hr = D3DCompile(shaderSource, strlen(shaderSource), "postfx", nullptr, nullptr, entries[i][0], entries[i][1],
                                    D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &blobs[i], &err);
            if (FAILED(hr)) {
                logger::error("postfx shader: {}", err ? (const char*)err->GetBufferPointer() : "compile failed");
                release(err);
                compileState = Compile::Failed;
                return;
            }
            release(err);
        }
        compileState = Compile::Ready;
    });
}

static bool buildShaders(ID3D11Device* dev) {
    bool ok = SUCCEEDED(dev->CreateVertexShader(blobs[0]->GetBufferPointer(), blobs[0]->GetBufferSize(), nullptr, &gpu.vs)) &&
              SUCCEEDED(dev->CreatePixelShader(blobs[1]->GetBufferPointer(), blobs[1]->GetBufferSize(), nullptr, &gpu.ps)) &&
              SUCCEEDED(dev->CreatePixelShader(blobs[2]->GetBufferPointer(), blobs[2]->GetBufferSize(), nullptr, &gpu.psBlur));
    if (!ok) return false;

    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth = sizeof(Constants);
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(dev->CreateBuffer(&bd, nullptr, &gpu.cb))) return false;
    bd.ByteWidth = sizeof(float) * 16;
    if (FAILED(dev->CreateBuffer(&bd, nullptr, &gpu.cbBlur))) return false;
    bd.ByteWidth = 32;
    if (FAILED(dev->CreateBuffer(&bd, nullptr, &gpu.ccb))) return false;

    D3D11_SAMPLER_DESC sd{};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(dev->CreateSamplerState(&sd, &gpu.sampler))) return false;

    D3D11_BLEND_DESC bl{};
    bl.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(dev->CreateBlendState(&bl, &gpu.blend))) return false;

    D3D11_BLEND_DESC ab{};
    ab.RenderTarget[0].BlendEnable = TRUE;
    ab.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    ab.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    ab.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    ab.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    ab.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    ab.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    ab.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(dev->CreateBlendState(&ab, &gpu.alpha))) return false;

    D3D11_RASTERIZER_DESC rd{};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.DepthClipEnable = TRUE;
    if (FAILED(dev->CreateRasterizerState(&rd, &gpu.raster))) return false;
    rd.ScissorEnable = TRUE;
    if (FAILED(dev->CreateRasterizerState(&rd, &gpu.scissor))) return false;

    D3D11_DEPTH_STENCIL_DESC dd{};
    return SUCCEEDED(dev->CreateDepthStencilState(&dd, &gpu.depth));
}

static DXGI_FORMAT viewFormat(DXGI_FORMAT f) {
    switch (f) {
    case DXGI_FORMAT_R8G8B8A8_TYPELESS: return DXGI_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_B8G8R8A8_TYPELESS: return DXGI_FORMAT_B8G8R8A8_UNORM;
    case DXGI_FORMAT_B8G8R8X8_TYPELESS: return DXGI_FORMAT_B8G8R8X8_UNORM;
    case DXGI_FORMAT_R10G10B10A2_TYPELESS: return DXGI_FORMAT_R10G10B10A2_UNORM;
    case DXGI_FORMAT_R16G16B16A16_TYPELESS: return DXGI_FORMAT_R16G16B16A16_FLOAT;
    default: return f;
    }
}

static bool makeTexture(ID3D11Device* dev, const D3D11_TEXTURE2D_DESC& from, ID3D11Texture2D*& tex,
                        ID3D11ShaderResourceView*& view) {
    D3D11_TEXTURE2D_DESC td = from;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.CPUAccessFlags = 0;
    td.MiscFlags = 0;
    td.MipLevels = 1;
    td.ArraySize = 1;
    if (FAILED(dev->CreateTexture2D(&td, nullptr, &tex))) return false;
    D3D11_SHADER_RESOURCE_VIEW_DESC sv{};
    sv.Format = viewFormat(from.Format);
    sv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    sv.Texture2D.MipLevels = 1;
    return SUCCEEDED(dev->CreateShaderResourceView(tex, &sv, &view));
}

static bool ensure(ID3D11Device* dev, const D3D11_TEXTURE2D_DESC& back, bool wantLast) {
    compileShaders();
    if (compileState == Compile::Running) return false;
    if (compileState == Compile::Failed) return false;
    if (gpu.dev != dev) {
        dropAll();
        gpu.dev = dev;
        if (!buildShaders(dev)) {
            gpu.failed = true;
            return false;
        }
    }
    if (gpu.failed) return false;

    if (gpu.width != back.Width || gpu.height != back.Height || gpu.format != back.Format) {
        dropTextures();
        if (!makeTexture(dev, back, gpu.copy, gpu.copyView)) {
            gpu.failed = true;
            return false;
        }
        gpu.width = back.Width;
        gpu.height = back.Height;
        gpu.format = back.Format;
    }
    if (wantLast && !gpu.last) {
        if (!makeTexture(dev, back, gpu.last, gpu.lastView)) {
            release(gpu.lastView);
            release(gpu.last);
            gpu.failed = true;
            return false;
        }
    }
    if (!wantLast && gpu.last) {
        release(gpu.lastView);
        release(gpu.last);
        gpu.lastValid = false;
    }
    return true;
}

static std::vector<ShaderInfo> shaderList;
static bool shadersLoaded = false;

static std::filesystem::path shaderDir() { return paths::root() / L"shaders"; }

void reloadShaders() {
    shaderList.clear();
    for (auto& p : presets) shaderList.push_back({p.name, p.body, true});
    std::error_code ec;
    std::filesystem::create_directories(shaderDir(), ec);
    if (!std::filesystem::exists(shaderDir() / L"README.txt", ec)) std::ofstream(shaderDir() / L"README.txt") << userReadme;
    if (!std::filesystem::exists(shaderDir() / L"example.hlsl", ec)) std::ofstream(shaderDir() / L"example.hlsl") << exampleShader;
    for (auto& e : std::filesystem::directory_iterator(shaderDir(), ec)) {
        if (e.path().extension() != L".hlsl") continue;
        std::ifstream in(e.path());
        std::stringstream ss;
        ss << in.rdbuf();
        shaderList.push_back({e.path().stem().string(), ss.str(), false});
    }
    for (auto& [k, p] : gpu.custom) release(p);
    gpu.custom.clear();
    gpu.customError.clear();
    releaseCustomCompiles();
    shadersLoaded = true;
}

const std::vector<ShaderInfo>& shaders() {
    if (!shadersLoaded) reloadShaders();
    return shaderList;
}

std::string shaderError(int index) {
    auto it = gpu.customError.find(index);
    return it == gpu.customError.end() ? std::string() : it->second;
}

// Custom shaders compile on a worker like the built-in one; the frame skips the effect until the bytecode is in.
// A reload bumps the generation so a compile started for the old list is dropped when it finishes.
struct CustomCompile {
    ID3DBlob* blob = nullptr;
    std::string error;
    bool done = false;
};
static std::mutex customLock;
static std::map<int, CustomCompile> customCompiles;
static int customGeneration = 0;

static void releaseCustomCompiles() {
    std::scoped_lock g(customLock);
    for (auto& [k, c] : customCompiles) release(c.blob);
    customCompiles.clear();
    customGeneration++;
}

static void compileCustom(int index) {
    std::string code = std::string(prelude) + shaderList[size_t(index)].source, name = shaderList[size_t(index)].name;
    int generation = customGeneration;
    customCompiles[index] = {};
    bg::run([code = std::move(code), name = std::move(name), index, generation] {
        ID3DBlob *blob = nullptr, *err = nullptr;
        HRESULT hr;
        {
            std::scoped_lock g(compileLock);
            hr = D3DCompile(code.c_str(), code.size(), name.c_str(), nullptr, nullptr, "ps", "ps_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &blob, &err);
        }
        std::string msg = FAILED(hr) ? (err ? (const char*)err->GetBufferPointer() : "compile failed") : "";
        release(err);
        if (!msg.empty()) logger::error("shader {}: {}", name, msg);
        std::scoped_lock g(customLock);
        if (generation != customGeneration) {
            release(blob);
            return;
        }
        customCompiles[index] = {blob, msg, true};
    });
}

static ID3D11PixelShader* customShader(ID3D11Device* dev, int index) {
    if (auto it = gpu.custom.find(index); it != gpu.custom.end()) return it->second;
    if (gpu.customError.count(index) || index < 0 || index >= (int)shaderList.size()) return nullptr;
    std::scoped_lock g(customLock);
    auto it = customCompiles.find(index);
    if (it == customCompiles.end()) {
        compileCustom(index);
        return nullptr;
    }
    if (!it->second.done) return nullptr;
    ID3D11PixelShader* ps = nullptr;
    std::string msg = it->second.error;
    if (it->second.blob && FAILED(dev->CreatePixelShader(it->second.blob->GetBufferPointer(), it->second.blob->GetBufferSize(), nullptr, &ps)))
        msg = "the shader compiled but the device refused it";
    release(it->second.blob);
    customCompiles.erase(it);
    if (!msg.empty()) gpu.customError[index] = msg;
    gpu.custom[index] = ps;
    return ps;
}

static void customPass(ID3D11Device* dev, ID3D11DeviceContext* ctx, ID3D11Texture2D* back, const D3D11_TEXTURE2D_DESC& bd) {
    ID3D11PixelShader* ps = customShader(dev, current.shader);
    if (!ps) return;
    ctx->CopyResource(gpu.copy, back);

    D3D11_MAPPED_SUBRESOURCE m{};
    if (FAILED(ctx->Map(gpu.ccb, 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) return;
    float k[8] = {(float)bd.Width, (float)bd.Height, 1.f / bd.Width, 1.f / bd.Height, clock, current.shaderMix, 0.f, 0.f};
    std::memcpy(m.pData, k, sizeof(k));
    ctx->Unmap(gpu.ccb, 0);

    D3D11_VIEWPORT vp{0.f, 0.f, (float)bd.Width, (float)bd.Height, 0.f, 1.f};
    ctx->RSSetViewports(1, &vp);
    ctx->RSSetState(gpu.raster);
    ctx->OMSetBlendState(gpu.blend, nullptr, 0xffffffff);
    ctx->OMSetDepthStencilState(gpu.depth, 0);
    ctx->IASetInputLayout(nullptr);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(gpu.vs, nullptr, 0);
    ctx->GSSetShader(nullptr, nullptr, 0);
    ctx->PSSetShader(ps, nullptr, 0);
    ctx->PSSetConstantBuffers(0, 1, &gpu.ccb);
    ctx->PSSetShaderResources(0, 1, &gpu.copyView);
    ctx->PSSetSamplers(0, 1, &gpu.sampler);
    ctx->Draw(3, 0);
    ID3D11ShaderResourceView* none[1] = {};
    ctx->PSSetShaderResources(0, 1, none);
}

static void basicPass(ID3D11DeviceContext* ctx, ID3D11Texture2D* back, const D3D11_TEXTURE2D_DESC& bd, bool wantLast) {
    const Params& p = current;
    ctx->CopyResource(gpu.copy, back);

    D3D11_MAPPED_SUBRESOURCE m{};
    if (SUCCEEDED(ctx->Map(gpu.cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) {
        Constants k{};
        float a[4] = {p.saturation, p.hue * 0.0174533f, p.brightness, p.contrast};
        float b[4] = {p.gamma, p.sharpen, p.fry, p.flip};
        float c[4] = {(float)p.tintMode, p.zoom, p.vignette, (float)p.colorMode};
        float d[4] = {p.dof, p.dir[0], p.dir[1], gpu.lastValid ? p.blend : 0.f};
        float e[4] = {1.f / bd.Width, 1.f / bd.Height, clock, (float)p.dirSamples};
        std::memcpy(k.a, a, sizeof(a));
        std::memcpy(k.b, b, sizeof(b));
        std::memcpy(k.tint, p.tint, sizeof(k.tint));
        std::memcpy(k.c, c, sizeof(c));
        std::memcpy(k.d, d, sizeof(d));
        std::memcpy(k.e, e, sizeof(e));
        std::memcpy(k.night, p.night, sizeof(k.night));
        k.f[0] = p.blur;
        float g[4] = {p.dofSharp, p.dofEdge, p.dofFocus, p.dofBand ? 1.f : 0.f};
        float h[4] = {p.dofReach, (float)p.dofRings, p.paint, 0.f};
        std::memcpy(k.g, g, sizeof(g));
        std::memcpy(k.h, h, sizeof(h));
        std::memcpy(m.pData, &k, sizeof(k));
        ctx->Unmap(gpu.cb, 0);
    }

    D3D11_VIEWPORT vp{0.f, 0.f, (float)bd.Width, (float)bd.Height, 0.f, 1.f};
    ctx->RSSetViewports(1, &vp);
    ctx->RSSetState(gpu.raster);
    ctx->OMSetBlendState(gpu.blend, nullptr, 0xffffffff);
    ctx->OMSetDepthStencilState(gpu.depth, 0);
    ctx->IASetInputLayout(nullptr);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(gpu.vs, nullptr, 0);
    ctx->GSSetShader(nullptr, nullptr, 0);
    ctx->PSSetShader(gpu.ps, nullptr, 0);
    ctx->PSSetConstantBuffers(0, 1, &gpu.cb);
    ID3D11ShaderResourceView* views[2] = {gpu.copyView, gpu.lastValid ? gpu.lastView : gpu.copyView};
    ctx->PSSetShaderResources(0, 2, views);
    ctx->PSSetSamplers(0, 1, &gpu.sampler);
    ctx->Draw(3, 0);

    ID3D11ShaderResourceView* none[2] = {};
    ctx->PSSetShaderResources(0, 2, none);

    if (wantLast) {
        ctx->CopyResource(gpu.last, back);
        gpu.lastValid = true;
    }
}

static void pass(ID3D11Device* dev, ID3D11DeviceContext* ctx) {
    ID3D11RenderTargetView* rtv = nullptr;
    ctx->OMGetRenderTargets(1, &rtv, nullptr);
    if (!rtv) return;

    ID3D11Resource* res = nullptr;
    rtv->GetResource(&res);
    rtv->Release();
    ID3D11Texture2D* back = nullptr;
    if (!res || FAILED(res->QueryInterface(IID_PPV_ARGS(&back)))) {
        release(res);
        return;
    }
    res->Release();

    D3D11_TEXTURE2D_DESC bd{};
    back->GetDesc(&bd);
    const Params& p = current;
    bool wantLast = p.blend > 0.f && p.basic();

    if (bd.SampleDesc.Count == 1 && ensure(dev, bd, wantLast)) {
        if (p.basic()) basicPass(ctx, back, bd, wantLast);
        if (p.shader >= 0) customPass(dev, ctx, back, bd);
        gpu.blurCopied = false;
    }
    back->Release();
}

static void blurPass(ID3D11Device* dev, ID3D11DeviceContext* ctx, const BlurJob& job) {
    ID3D11RenderTargetView* rtv = nullptr;
    ctx->OMGetRenderTargets(1, &rtv, nullptr);
    if (!rtv) return;
    ID3D11Resource* res = nullptr;
    rtv->GetResource(&res);
    rtv->Release();
    ID3D11Texture2D* back = nullptr;
    if (!res || FAILED(res->QueryInterface(IID_PPV_ARGS(&back)))) {
        release(res);
        return;
    }
    res->Release();

    D3D11_TEXTURE2D_DESC bd{};
    back->GetDesc(&bd);
    if (bd.SampleDesc.Count == 1 && ensure(dev, bd, gpu.last != nullptr)) {
        if (!gpu.blurCopied) {
            back->Release();
            return;
        }
        D3D11_MAPPED_SUBRESOURCE m{};
        if (SUCCEEDED(ctx->Map(gpu.cbBlur, 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) {
            float k[16] = {job.rect[0], job.rect[1], job.rect[2], job.rect[3], job.radius, job.rounding, 1.f, job.angle,
                           job.tint[0], job.tint[1], job.tint[2], job.tint[3], 1.f / bd.Width, 1.f / bd.Height, 0.f, 0.f};
            std::memcpy(m.pData, k, sizeof(k));
            ctx->Unmap(gpu.cbBlur, 0);
        } else {
            back->Release();
            return;
        }
        D3D11_VIEWPORT vp{0.f, 0.f, (float)bd.Width, (float)bd.Height, 0.f, 1.f};
        ctx->RSSetViewports(1, &vp);
        float hx = (job.rect[2] - job.rect[0]) * 0.5f, hy = (job.rect[3] - job.rect[1]) * 0.5f;
        float mx = job.rect[0] + hx, my = job.rect[1] + hy;
        float cs = std::fabs(std::cos(job.angle)), sn = std::fabs(std::sin(job.angle));
        float ex = hx * cs + hy * sn, ey = hx * sn + hy * cs;
        D3D11_RECT sc{LONG(mx - ex) - 1, LONG(my - ey) - 1, LONG(mx + ex) + 2, LONG(my + ey) + 2};
        ctx->RSSetScissorRects(1, &sc);
        ctx->RSSetState(gpu.scissor);
        ctx->OMSetBlendState(gpu.alpha, nullptr, 0xffffffff);
        ctx->OMSetDepthStencilState(gpu.depth, 0);
        ctx->IASetInputLayout(nullptr);
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ctx->VSSetShader(gpu.vs, nullptr, 0);
        ctx->GSSetShader(nullptr, nullptr, 0);
        ctx->PSSetShader(gpu.psBlur, nullptr, 0);
        ID3D11Buffer* bufs[2] = {gpu.cb, gpu.cbBlur};
        ctx->PSSetConstantBuffers(0, 2, bufs);
        ID3D11ShaderResourceView* views[2] = {gpu.copyView, gpu.copyView};
        ctx->PSSetShaderResources(0, 2, views);
        ctx->PSSetSamplers(0, 1, &gpu.sampler);
        ctx->Draw(3, 0);
        ID3D11ShaderResourceView* none[2] = {};
        ctx->PSSetShaderResources(0, 2, none);
    }
    back->Release();
}

static void blurCallback(const ImDrawList*, const ImDrawCmd* cmd) {
    auto* state = static_cast<ImGui_ImplDX11_RenderState*>(ImGui::GetPlatformIO().Renderer_RenderState);
    if (!state || !state->Device || !state->DeviceContext || !cmd->UserCallbackData) return;
    const auto& job = *static_cast<const BlurJob*>(cmd->UserCallbackData);
    guard::call("blur", [&] { blurPass(state->Device, state->DeviceContext, job); });
}

// Capture before any Monchi HUD or menu geometry. All blur rectangles in this frame
// sample the same game image, regardless of draw order or which panel uses blur first.
static void backdropCallback(const ImDrawList*, const ImDrawCmd*) {
    if (blurJobs.empty() && !gpu.frameWanted) return;
    auto* state = static_cast<ImGui_ImplDX11_RenderState*>(ImGui::GetPlatformIO().Renderer_RenderState);
    if (!state || !state->Device || !state->DeviceContext) return;
    guard::call("blur backdrop", [&] {
        ID3D11RenderTargetView* rtv = nullptr;
        state->DeviceContext->OMGetRenderTargets(1, &rtv, nullptr);
        if (!rtv) return;
        ID3D11Resource* res = nullptr;
        rtv->GetResource(&res);
        rtv->Release();
        ID3D11Texture2D* back = nullptr;
        if (res) res->QueryInterface(IID_PPV_ARGS(&back));
        release(res);
        if (!back) return;
        D3D11_TEXTURE2D_DESC bd{};
        back->GetDesc(&bd);
        if (bd.SampleDesc.Count == 1 && ensure(state->Device, bd, gpu.last != nullptr)) {
            state->DeviceContext->CopyResource(gpu.copy, back);
            gpu.blurCopied = true;
        }
        back->Release();
    });
}

static void callback(const ImDrawList*, const ImDrawCmd*) {
    auto* state = static_cast<ImGui_ImplDX11_RenderState*>(ImGui::GetPlatformIO().Renderer_RenderState);
    if (!state || !state->Device || !state->DeviceContext) return;
    guard::call("postfx", [&] { pass(state->Device, state->DeviceContext); });
}

bool Params::active() const { return basic() || shader >= 0; }

bool Params::basic() const {
    return saturation != 1.f || hue != 0.f || brightness != 0.f || contrast != 1.f || gamma != 1.f || sharpen > 0.f ||
           fry > 0.f || paint > 0.f || flip > 0.5f || tint[3] > 0.f
 || night[3] > 0.f || vignette > 0.f || colorMode != 0 ||
           dof > 0.f || blur > 0.5f || (dirSamples > 1 && (dir[0] != 0.f || dir[1] != 0.f)) || blend > 0.f || zoom > 1.001f;
}

Params& params() { return frameParams; }

// the game's back buffer carries whatever alpha the game left there, so the picture is written as it is
static void opaqueCallback(const ImDrawList*, const ImDrawCmd*) {
    auto* state = static_cast<ImGui_ImplDX11_RenderState*>(ImGui::GetPlatformIO().Renderer_RenderState);
    if (!state || !state->Device || !state->DeviceContext) return;
    if (!gpu.opaque) {
        D3D11_BLEND_DESC desc{};
        desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        if (FAILED(state->Device->CreateBlendState(&desc, &gpu.opaque))) return;
    }
    const float factor[4] = {0.f, 0.f, 0.f, 0.f};
    state->DeviceContext->OMSetBlendState(gpu.opaque, factor, 0xffffffff);
}

void frameImage(ImDrawList* dl, ImVec2 min, ImVec2 max, ImVec2 uv0, ImVec2 uv1) {
    gpu.frameWanted = true;
    if (!gpu.copyView) {
        dl->AddRectFilled(min, max, IM_COL32(18, 20, 26, 255));
        return;
    }
    dl->AddCallback(opaqueCallback, nullptr);
    dl->AddImage(ImTextureID(reinterpret_cast<intptr_t>(gpu.copyView)), min, max, uv0, uv1);
    dl->AddCallback(ImGui::GetPlatformIO().DrawCallback_ResetRenderState, nullptr);
}

void blur(ImDrawList* dl, ImVec2 min, ImVec2 max, float rounding, float radius, ImVec4 tint, float angle) {
    if (max.x - min.x < 2.f || max.y - min.y < 2.f) return;
    blurJobs.push_back({{min.x, min.y, max.x, max.y}, rounding, radius, {tint.x, tint.y, tint.z, tint.w}, angle});
    dl->AddCallback(blurCallback, &blurJobs.back());
    dl->AddCallback(ImGui::GetPlatformIO().DrawCallback_ResetRenderState, nullptr);
}

void begin() {
    compileShaders();
    frameParams = Params{};
    blurJobs.clear();
    gpu.blurCopied = false;
    gpu.frameWanted = false;
    backdrop = {};
    dropRetired();
}

void submit(ImDrawList* dl) {
    clock = std::fmod(clock + ui::dt(), 1000.f);
    if (!frameParams.active()) {
        gpu.lastValid = false;
    } else {
        current = frameParams;
        dl->AddCallback(callback, nullptr);
        dl->AddCallback(ImGui::GetPlatformIO().DrawCallback_ResetRenderState, nullptr);
    }
    backdrop.reserve(dl, backdropCallback, ImGui::GetPlatformIO().DrawCallback_ResetRenderState);
}

void finish() {
    // The backdrop is reserved before HUD geometry, but menu blur requests arrive afterwards.
    backdrop.finish(!blurJobs.empty() || gpu.frameWanted);
}

void shutdown() {
    dropAll();
    if (compileState == Compile::Running) return;
    for (auto*& b : blobs) release(b);
    compileState = Compile::Idle;
}

// after the background workers are joined: whatever a compile finished during shutdown is released here
void releaseCompiled() {
    for (auto*& b : blobs) release(b);
    compileState = Compile::Idle;
    releaseCustomCompiles();
}

}
