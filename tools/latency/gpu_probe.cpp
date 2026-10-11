#include "system/GpuLatency.hpp"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <iostream>
#include <string_view>

using Microsoft::WRL::ComPtr;

int main(int argc, char** argv) {
    bool exercise = argc == 2 && std::string_view(argv[1]) == "--exercise-nvidia";
    std::cerr << "Creating a separate DXGI factory\n";
    ComPtr<IDXGIFactory6> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) return 1;
    for (UINT i = 0;; i++) {
        ComPtr<IDXGIAdapter1> gpu;
        if (factory->EnumAdapters1(i, &gpu) == DXGI_ERROR_NOT_FOUND) break;
        DXGI_ADAPTER_DESC1 desc{};
        gpu->GetDesc1(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
        ComPtr<ID3D12Device> device;
        std::wcerr << L"Creating a separate DX12 device: " << desc.Description << L"\n";
        HRESULT hr = D3D12CreateDevice(gpu.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
        std::wcout << desc.Description << L": DX12 result " << hr << L"\n";
        if (FAILED(hr)) continue;
        const auto detected = gpuLatency::adapter(device.Get());
        std::cerr << "Adapter identified\n";
        if (detected.vendorId != desc.VendorId || detected.deviceId != desc.DeviceId) {
            std::cerr << "device adapter mismatch\n";
            return 2;
        }
        gpuLatency::attach(device.Get());
        std::cerr << "Backend support probe completed\n";
        const auto status = gpuLatency::status();
        std::cerr << "vendor " << std::hex << detected.vendorId << std::dec
                  << ", backend stage " << int(status.stage) << ", probe error " << status.error << "\n";
        if (detected.vendor == gpuLatency::Vendor::Amd) {
            if (status.stage == gpuLatency::Stage::Unsupported)
                std::cerr << "AMD Anti-Lag 2 extension is unavailable on this adapter/driver\n";
            else
                std::cerr << "AMD Anti-Lag 2 initialization succeeded; pacing remains disabled\n";
            if (gpuLatency::set(gpuLatency::Mode::On)) {
                std::cerr << "AMD pacing incorrectly enabled without a verified input boundary\n";
                gpuLatency::shutdown();
                return 7;
            }
        }
        if (exercise && detected.vendor == gpuLatency::Vendor::Nvidia) {
            if (!gpuLatency::set(gpuLatency::Mode::On)) {
                std::cerr << "NVIDIA enable failed: " << gpuLatency::status().error << "\n";
                gpuLatency::shutdown();
                return 4;
            }
            if (gpuLatency::status().stage != gpuLatency::Stage::DriverOnly) return 5;
            if (!gpuLatency::set(gpuLatency::Mode::Off)) return 6;
            std::cerr << "NVIDIA enable/disable succeeded on the separate test device\n";
        }
        std::cerr << "Releasing backend\n";
        if (!gpuLatency::shutdown()) return 3;
        std::cerr << "Backend released\n";
    }
    std::cerr << (exercise ? "Driver lifecycle exercised only on separate test devices.\n" : "No GPU latency mode was enabled.\n");
    std::cerr << "No Minecraft process was accessed.\n";
}
