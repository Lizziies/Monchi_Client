#include "Image.hpp"

#include <windows.h>
#include <wincodec.h>

#include <algorithm>
#include <thread>

namespace img {

namespace {

const GUID clsidFactory = {0xcacaf262, 0x9370, 0x4615, {0xa1, 0x3b, 0x9f, 0x55, 0x39, 0xda, 0x4c, 0x0a}};
const GUID iidFactory = {0xec5ec8a9, 0xc395, 0x4314, {0x9c, 0x77, 0x54, 0xd7, 0xa9, 0x35, 0xff, 0x70}};
const GUID pixelRgba = {0xf5c7ad2d, 0x6a8d, 0x43dd, {0xa7, 0xa8, 0xa2, 0x99, 0x35, 0x26, 0x1a, 0xe9}};

template <class T>
void drop(T*& p) {
    if (p) p->Release();
    p = nullptr;
}

bool decode(const std::filesystem::path& path, std::span<const uint8_t> bytes, int maxSize, Pixels& out) {
    IWICImagingFactory* factory = nullptr;
    if (FAILED(CoCreateInstance(clsidFactory, nullptr, CLSCTX_INPROC_SERVER, iidFactory, reinterpret_cast<void**>(&factory)))) return false;

    IWICStream* stream = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* conv = nullptr;
    IWICBitmapScaler* scaler = nullptr;
    bool ok = false;
    do {
        if (bytes.empty()) {
            if (FAILED(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder))) break;
        } else {
            if (bytes.size() > UINT_MAX || FAILED(factory->CreateStream(&stream))) break;
            if (FAILED(stream->InitializeFromMemory(const_cast<BYTE*>(bytes.data()), UINT(bytes.size())))) break;
            if (FAILED(factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnDemand, &decoder))) break;
        }
        if (FAILED(decoder->GetFrame(0, &frame))) break;
        if (FAILED(factory->CreateFormatConverter(&conv))) break;
        if (FAILED(conv->Initialize(frame, pixelRgba, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) break;
        UINT w = 0, h = 0;
        if (FAILED(conv->GetSize(&w, &h)) || !w || !h) break;
        IWICBitmapSource* source = conv;
        UINT tw = w, th = h;
        if (int(std::max(w, h)) > maxSize) {
            float k = float(maxSize) / float(std::max(w, h));
            tw = std::max(1u, UINT(float(w) * k));
            th = std::max(1u, UINT(float(h) * k));
            if (FAILED(factory->CreateBitmapScaler(&scaler))) break;
            if (FAILED(scaler->Initialize(conv, tw, th, WICBitmapInterpolationModeFant))) break;
            source = scaler;
        }
        out.w = int(tw);
        out.h = int(th);
        out.rgba.assign(size_t(tw) * th, 0);
        ok = SUCCEEDED(source->CopyPixels(nullptr, tw * 4, UINT(out.rgba.size() * 4), reinterpret_cast<BYTE*>(out.rgba.data())));
    } while (false);

    drop(scaler);
    drop(conv);
    drop(frame);
    drop(decoder);
    drop(stream);
    drop(factory);
    return ok;
}

}

bool load(const std::filesystem::path& path, int maxSize, Pixels& out) {
    bool ok = false;
    Pixels result;
    std::thread worker([&] {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        ok = decode(path, {}, maxSize, result);
        CoUninitialize();
    });
    worker.join();
    if (ok) out = std::move(result);
    return ok;
}

bool load(std::span<const uint8_t> bytes, int maxSize, Pixels& out) {
    bool ok = false;
    Pixels result;
    std::thread worker([&] {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        ok = decode({}, bytes, maxSize, result);
        if (SUCCEEDED(hr)) CoUninitialize();
    });
    worker.join();
    if (ok) out = std::move(result);
    return ok;
}

}
