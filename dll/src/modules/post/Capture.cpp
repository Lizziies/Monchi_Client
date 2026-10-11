#include "Capture.hpp"
#include "core/Guard.hpp"
#include "core/Bg.hpp"
#include "core/Log.hpp"

#include <windows.h>
#include <d3d11.h>
#include <imgui_impl_dx11.h>
#include <wincodec.h>

#include <atomic>
#include <cstring>
#include <mutex>

namespace capture {

static const GUID clsidFactory = {0xcacaf262, 0x9370, 0x4615, {0xa1, 0x3b, 0x9f, 0x55, 0x39, 0xda, 0x4c, 0x0a}};
static const GUID iidFactory = {0xec5ec8a9, 0xc395, 0x4314, {0x9c, 0x77, 0x54, 0xd7, 0xa9, 0x35, 0xff, 0x70}};
static const GUID containerPng = {0x1b7cfaf4, 0x713f, 0x473c, {0xbb, 0xcd, 0x61, 0x37, 0x42, 0x5f, 0xae, 0xaf}};
static const GUID containerJpeg = {0x19e4a5aa, 0x5662, 0x4fc5, {0xa0, 0xc0, 0x17, 0x58, 0x02, 0x8e, 0x10, 0x57}};
static const GUID pixelBgr24 = {0x6fddc324, 0x4e03, 0x4bfe, {0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x0c}};

static bool wanted = false;
static Stage wantedStage = Stage::Game;
static ID3D11Device* stagingDevice = nullptr;
static ID3D11Texture2D* staging = nullptr;
static DXGI_FORMAT stagingFormat = DXGI_FORMAT_UNKNOWN;
static int stagingWidth = 0, stagingHeight = 0;

static std::mutex lock;
static std::filesystem::path savedPath;
static bool savedOk = false;
static bool savedReady = false;

static void dropStaging() {
    if (staging) staging->Release();
    staging = nullptr;
    if (stagingDevice) stagingDevice->Release();
    stagingDevice = nullptr;
}

bool request(Stage stage) {
    if (wanted || staging) return false;
    wanted = true;
    wantedStage = stage;
    return true;
}

bool busy() { return wanted || staging; }

static bool supported(DXGI_FORMAT f) {
    switch (f) {
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT_B8G8R8X8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R10G10B10A2_UNORM:
        return true;
    default:
        return false;
    }
}

static void grab(ID3D11Device* dev, ID3D11DeviceContext* ctx) {
    ID3D11RenderTargetView* rtv = nullptr;
    ctx->OMGetRenderTargets(1, &rtv, nullptr);
    if (!rtv) {
        wanted = false;
        return;
    }
    ID3D11Resource* res = nullptr;
    rtv->GetResource(&res);
    rtv->Release();
    ID3D11Texture2D* back = nullptr;
    if (!res || FAILED(res->QueryInterface(IID_PPV_ARGS(&back)))) {
        if (res) res->Release();
        wanted = false;
        return;
    }
    res->Release();

    D3D11_TEXTURE2D_DESC td{};
    back->GetDesc(&td);
    wanted = false;
    if (td.SampleDesc.Count == 1 && supported(td.Format)) {
        td.Usage = D3D11_USAGE_STAGING;
        td.BindFlags = 0;
        td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        td.MiscFlags = 0;
        td.MipLevels = 1;
        td.ArraySize = 1;
        if (SUCCEEDED(dev->CreateTexture2D(&td, nullptr, &staging))) {
            ctx->CopyResource(staging, back);
            dev->AddRef();
            stagingDevice = dev;
            stagingFormat = td.Format;
            stagingWidth = (int)td.Width;
            stagingHeight = (int)td.Height;
        }
    } else {
        logger::warn("screenshot: unsupported back buffer format {}", (int)td.Format);
    }
    back->Release();
}

static void callback(const ImDrawList*, const ImDrawCmd* cmd) {
    auto* state = static_cast<ImGui_ImplDX11_RenderState*>(ImGui::GetPlatformIO().Renderer_RenderState);
    if (!state || !state->Device || !state->DeviceContext) return;
    Stage stage = cmd->UserCallbackData ? Stage::Overlay : Stage::Game;
    if (!wanted || stage != wantedStage) return;
    guard::call("screenshot", [&] { grab(state->Device, state->DeviceContext); });
}

void grabFinal(ID3D11Device* device, ID3D11DeviceContext* context) {
    if (!wanted || wantedStage != Stage::Final || !device || !context) return;
    grab(device, context);
}

void submit(ImDrawList* dl, Stage stage) {
    if (!wanted || stage != wantedStage) return;
    dl->AddCallback(callback, stage == Stage::Overlay ? reinterpret_cast<void*>(1) : nullptr);
}

bool poll(Image& out) {
    if (!staging || !stagingDevice) return false;
    ID3D11DeviceContext* ctx = nullptr;
    stagingDevice->GetImmediateContext(&ctx);
    if (!ctx) return false;

    D3D11_MAPPED_SUBRESOURCE m{};
    HRESULT hr = ctx->Map(staging, 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &m);
    if (hr == DXGI_ERROR_WAS_STILL_DRAWING) {
        ctx->Release();
        return false;
    }
    if (FAILED(hr)) {
        ctx->Release();
        dropStaging();
        return false;
    }

    out.width = stagingWidth;
    out.height = stagingHeight;
    out.bgra.resize(size_t(stagingWidth) * stagingHeight * 4);
    bool rgba = stagingFormat == DXGI_FORMAT_R8G8B8A8_UNORM || stagingFormat == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB ||
                stagingFormat == DXGI_FORMAT_R8G8B8A8_TYPELESS;
    bool ten = stagingFormat == DXGI_FORMAT_R10G10B10A2_UNORM;
    for (int y = 0; y < stagingHeight; y++) {
        const uint8_t* row = static_cast<const uint8_t*>(m.pData) + size_t(y) * m.RowPitch;
        uint8_t* dst = out.bgra.data() + size_t(y) * stagingWidth * 4;
        for (int x = 0; x < stagingWidth; x++) {
            const uint8_t* px = row + x * 4;
            if (ten) {
                uint32_t v;
                std::memcpy(&v, px, 4);
                dst[x * 4 + 0] = uint8_t(((v >> 20) & 0x3FF) >> 2);
                dst[x * 4 + 1] = uint8_t(((v >> 10) & 0x3FF) >> 2);
                dst[x * 4 + 2] = uint8_t((v & 0x3FF) >> 2);
            } else if (rgba) {
                dst[x * 4 + 0] = px[2];
                dst[x * 4 + 1] = px[1];
                dst[x * 4 + 2] = px[0];
            } else {
                dst[x * 4 + 0] = px[0];
                dst[x * 4 + 1] = px[1];
                dst[x * 4 + 2] = px[2];
            }
            dst[x * 4 + 3] = 255;
        }
    }
    ctx->Unmap(staging, 0);
    ctx->Release();
    dropStaging();
    return true;
}

static bool encode(const Image& img, const std::filesystem::path& path, Format format, int quality) {
    IWICImagingFactory* factory = nullptr;
    if (FAILED(CoCreateInstance(clsidFactory, nullptr, CLSCTX_INPROC_SERVER, iidFactory, reinterpret_cast<void**>(&factory))))
        return false;

    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);

    IWICStream* stream = nullptr;
    IWICBitmapEncoder* encoder = nullptr;
    IWICBitmapFrameEncode* frame = nullptr;
    IPropertyBag2* props = nullptr;
    bool ok = false;

    std::vector<uint8_t> rgb(size_t(img.width) * img.height * 3);
    for (size_t i = 0, n = size_t(img.width) * img.height; i < n; i++) {
        rgb[i * 3 + 0] = img.bgra[i * 4 + 0];
        rgb[i * 3 + 1] = img.bgra[i * 4 + 1];
        rgb[i * 3 + 2] = img.bgra[i * 4 + 2];
    }

    do {
        if (FAILED(factory->CreateStream(&stream))) break;
        if (FAILED(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE))) break;
        if (FAILED(factory->CreateEncoder(format == Format::Png ? containerPng : containerJpeg, nullptr, &encoder))) break;
        if (FAILED(encoder->Initialize(stream, WICBitmapEncoderNoCache))) break;
        if (FAILED(encoder->CreateNewFrame(&frame, &props))) break;

        if (format == Format::Jpeg && props) {
            PROPBAG2 opt{};
            wchar_t name[] = L"ImageQuality";
            opt.pstrName = name;
            VARIANT v;
            VariantInit(&v);
            v.vt = VT_R4;
            v.fltVal = quality / 100.f;
            props->Write(1, &opt, &v);
        }
        if (FAILED(frame->Initialize(props))) break;
        if (FAILED(frame->SetSize((UINT)img.width, (UINT)img.height))) break;
        WICPixelFormatGUID pf = pixelBgr24;
        if (FAILED(frame->SetPixelFormat(&pf))) break;
        if (FAILED(frame->WritePixels((UINT)img.height, (UINT)img.width * 3, (UINT)rgb.size(), rgb.data()))) break;
        if (FAILED(frame->Commit())) break;
        ok = SUCCEEDED(encoder->Commit());
    } while (false);

    if (props) props->Release();
    if (frame) frame->Release();
    if (encoder) encoder->Release();
    if (stream) stream->Release();
    factory->Release();
    return ok;
}

void save(Image image, std::filesystem::path path, Format format, int quality) {
    bg::run([img = std::move(image), path = std::move(path), format, quality]() mutable {
        HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        bool ok = false;
        guard::call("screenshot encode", [&] { ok = encode(img, path, format, quality); });
        if (SUCCEEDED(com)) CoUninitialize();
        std::scoped_lock g(lock);
        savedPath = std::move(path);
        savedOk = ok;
        savedReady = true;
    });
}

bool takeSaved(std::filesystem::path& path, bool& ok) {
    std::scoped_lock g(lock);
    if (!savedReady) return false;
    savedReady = false;
    path = savedPath;
    ok = savedOk;
    return true;
}

bool copyToClipboard(const Image& image, void* window) {
    size_t bytes = size_t(image.width) * image.height * 4;
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, sizeof(BITMAPINFOHEADER) + bytes);
    if (!mem) return false;
    auto* base = static_cast<uint8_t*>(GlobalLock(mem));
    if (!base) {
        GlobalFree(mem);
        return false;
    }
    BITMAPINFOHEADER bi{};
    bi.biSize = sizeof(bi);
    bi.biWidth = image.width;
    bi.biHeight = image.height;
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;
    bi.biSizeImage = (DWORD)bytes;
    std::memcpy(base, &bi, sizeof(bi));
    for (int y = 0; y < image.height; y++)
        std::memcpy(base + sizeof(bi) + size_t(y) * image.width * 4, image.bgra.data() + size_t(image.height - 1 - y) * image.width * 4,
                    size_t(image.width) * 4);
    GlobalUnlock(mem);

    bool ok = false;
    if (OpenClipboard(static_cast<HWND>(window))) {
        EmptyClipboard();
        ok = SetClipboardData(CF_DIB, mem) != nullptr;
        CloseClipboard();
    }
    if (!ok) GlobalFree(mem);
    return ok;
}

void shutdown() {
    wanted = false;
    dropStaging();
}

}
