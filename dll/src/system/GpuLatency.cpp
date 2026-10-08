#include "GpuLatency.hpp"
#include "PaceGate.hpp"

#include <windows.h>
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include "../../../vendor/nvapi/nvapi.h"
#include "../../../vendor/nvapi/nvapi_interface.h"
#include "../../../vendor/antilag2/ffx_antilag2_dx12.h"

#include <memory>
#include <mutex>
#include <string_view>

namespace gpuLatency {
namespace {
using Microsoft::WRL::ComPtr;
std::mutex lock;
// held for the whole pacing call and by everything that replaces the driver; always taken before lock
std::mutex paceLock;
std::unique_ptr<Driver> driver;
std::unique_ptr<Control> control;
Status last;
bool bound = false;
PaceGate paceGate;

class Nvidia : public Driver {
public:
    explicit Nvidia(IUnknown* device) : device_(device) {
        // An injected client does not own the game's DX12 device. Keep NVAPI alive until process exit.
        static HMODULE processLibrary = LoadLibraryExW(L"nvapi64.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        library_ = processLibrary;
        if (!library_) { error_ = NVAPI_LIBRARY_NOT_FOUND; return; }
        query_ = reinterpret_cast<void* (__cdecl*)(unsigned)>(GetProcAddress(library_, "nvapi_QueryInterface"));
        if (!query_) { error_ = NVAPI_NO_IMPLEMENTATION; return; }
        init_ = get<decltype(&NvAPI_Initialize)>("NvAPI_Initialize");
        set_ = get<decltype(&NvAPI_D3D_SetSleepMode)>("NvAPI_D3D_SetSleepMode");
        sleep_ = get<decltype(&NvAPI_D3D_Sleep)>("NvAPI_D3D_Sleep");
        auto status = get<decltype(&NvAPI_D3D_GetSleepStatus)>("NvAPI_D3D_GetSleepStatus");
        if (!init_ || !set_ || !sleep_ || !status) { error_ = NVAPI_NO_IMPLEMENTATION; return; }
        static const NvAPI_Status processInitialized = init_();
        error_ = processInitialized;
        if (error_ != NVAPI_OK) return;
        NV_GET_SLEEP_STATUS_PARAMS info{};
        info.version = NV_GET_SLEEP_STATUS_PARAMS_VER;
        error_ = status(device_.Get(), &info);
        // Do not replace pacing owned by another client or a frame-generation implementation.
        if (error_ == NVAPI_OK && (info.bLowLatencyMode || info.bUseGameSleep || info.fgMultiplier > 1))
            error_ = NVAPI_DEVICE_BUSY;
        ready_ = error_ == NVAPI_OK;
    }

    bool configure(Mode mode) override {
        if (mode == Mode::Off && !owned_) return true;
        if (!ready_) return false;
        NV_SET_SLEEP_MODE_PARAMS settings{};
        settings.version = NV_SET_SLEEP_MODE_PARAMS_VER;
        settings.bLowLatencyMode = mode != Mode::Off;
        settings.bLowLatencyBoost = mode == Mode::Boost;
        // Bedrock's simulation/render markers have not been verified; never invent them at Present.
        settings.bUseMarkersToOptimize = false;
        error_ = set_(device_.Get(), &settings);
        if (error_ == NVAPI_OK) owned_ = mode != Mode::Off;
        return error_ == NVAPI_OK;
    }

    bool pace() override {
        error_ = sleep_(device_.Get());
        return error_ == NVAPI_OK;
    }

    int error() const override { return error_; }

private:
    template<class T> T get(std::string_view name) {
        for (const auto& entry : nvapi_interface_table)
            if (name == entry.func) return reinterpret_cast<T>(query_(entry.id));
        return nullptr;
    }

    ComPtr<IUnknown> device_;
    HMODULE library_ = nullptr;
    void* (__cdecl* query_)(unsigned) = nullptr;
    decltype(&NvAPI_Initialize) init_ = nullptr;
    decltype(&NvAPI_D3D_SetSleepMode) set_ = nullptr;
    decltype(&NvAPI_D3D_Sleep) sleep_ = nullptr;
    int error_ = NVAPI_OK;
    bool ready_ = false, owned_ = false;
};

class Amd : public Driver {
public:
    explicit Amd(IUnknown* device) {
        error_ = device->QueryInterface(IID_PPV_ARGS(&device_));
        if (error_ == S_OK) error_ = AMD::AntiLag2DX12::Initialize(&context_, device_.Get());
    }
    ~Amd() override { AMD::AntiLag2DX12::DeInitialize(&context_); }

    bool configure(Mode mode) override {
        if (mode == Mode::Off && !context_.m_pAntiLagAPI) return true;
        if (!device_) { error_ = E_NOINTERFACE; return false; }
        if (!context_.m_pAntiLagAPI) {
            error_ = AMD::AntiLag2DX12::Initialize(&context_, device_.Get());
            if (error_ != S_OK) return false;
        }
        // Enabling is deferred to the verified input boundary; never sleep in a settings callback.
        if (mode == Mode::Off) {
            error_ = AMD::AntiLag2DX12::Update(&context_, false, 0);
            if (error_ != S_OK) return false;
        }
        return true;
    }

    bool pace() override {
        error_ = AMD::AntiLag2DX12::Update(&context_, true, 0);
        return error_ == S_OK;
    }

    int error() const override { return error_; }

private:
    ComPtr<ID3D12Device> device_;
    AMD::AntiLag2DX12::Context context_{};
    HRESULT error_ = S_OK;
};

bool stop() {
    paceGate.update(nullptr);
    if (control && !control->set(Mode::Off)) return false;
    control.reset();
    driver.reset();
    last = {};
    return true;
}
}

Adapter adapter(IUnknown* device) {
    Adapter result;
    if (!device) return result;
    ComPtr<IDXGIAdapter> gpu;
    ComPtr<ID3D12Device> d12;
    if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&d12)))) {
        ComPtr<IDXGIFactory4> factory;
        if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
            factory->EnumAdapterByLuid(d12->GetAdapterLuid(), IID_PPV_ARGS(&gpu));
    } else {
        ComPtr<IDXGIDevice> dxgi;
        if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&dxgi)))) dxgi->GetAdapter(&gpu);
    }
    DXGI_ADAPTER_DESC desc{};
    if (!gpu || FAILED(gpu->GetDesc(&desc))) return result;
    result.vendorId = desc.VendorId;
    result.deviceId = desc.DeviceId;
    result.name = desc.Description;
    switch (desc.VendorId) {
    case 0x10de: result.vendor = Vendor::Nvidia; break;
    case 0x1002: result.vendor = Vendor::Amd; break;
    case 0x8086: result.vendor = Vendor::Intel; break;
    }
    return result;
}

void attach(IUnknown* device) {
    std::scoped_lock g(paceLock, lock);
    if (!stop()) return;
    last.vendor = adapter(device).vendor;
    if (last.vendor == Vendor::Nvidia) driver = std::make_unique<Nvidia>(device);
    else if (last.vendor == Vendor::Amd) driver = std::make_unique<Amd>(device);
    if (!driver) return;
    control = std::make_unique<Control>(*driver, last.vendor);
    control->bind(bound);
}

bool set(Mode mode) {
    std::scoped_lock g(lock);
    bool ok = control && control->set(mode);
    paceGate.update(control.get());
    return ok;
}

void bindFrameStart(bool verified) {
    std::scoped_lock g(lock);
    bound = verified;
    if (control) control->bind(verified);
    paceGate.update(control.get());
}

// The driver call sleeps (that is its purpose). The render thread asks for status and mode every frame under lock, so
// the sleep runs outside of it; paceLock keeps the driver alive meanwhile.
bool beforeInput(uint64_t frame) {
    // RTSS leaves this disabled; input then need not contend with render-thread driver updates.
    if (!paceGate.ready()) return false;
    std::scoped_lock pacing(paceLock);
    Driver* active = nullptr;
    {
        std::scoped_lock g(lock);
        if (!control || !driver || !control->wantsPace(frame)) return false;
        active = driver.get();
    }
    bool ok = active->pace();
    std::scoped_lock g(lock);
    if (control) control->paced(ok);
    paceGate.update(control.get());
    return ok;
}

Status status() {
    std::scoped_lock g(lock);
    return control ? control->status() : last;
}

bool shutdown() {
    std::scoped_lock g(paceLock, lock);
    return stop();
}

}
