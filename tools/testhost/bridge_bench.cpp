#include <windows.h>
#include <d3d11.h>
#include <d3d11on12.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <algorithm>
#include <cstdio>
#include <vector>

using Microsoft::WRL::ComPtr;
int main() {
    ComPtr<ID3D12Device> device;
    if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)))) return 1;
    ComPtr<IDXGIFactory4> factory;
    CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    ComPtr<IDXGIAdapter1> adapter;
    factory->EnumAdapterByLuid(device->GetAdapterLuid(), IID_PPV_ARGS(&adapter));
    DXGI_ADAPTER_DESC1 desc{}; adapter->GetDesc1(&desc);
    std::wprintf(L"adapter: %ls\n", desc.Description);
    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    ComPtr<ID3D12CommandQueue> queue;
    device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue));
    ComPtr<ID3D11Device> d11; ComPtr<ID3D11DeviceContext> context;
    IUnknown* queues[] = {queue.Get()};
    if (FAILED(D3D11On12CreateDevice(device.Get(), 0, nullptr, 0, queues, 1, 0, &d11, &context, nullptr))) return 2;
    ComPtr<ID3D11On12Device> bridge; d11.As(&bridge);
    D3D12_HEAP_PROPERTIES heap{}; heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC rd{}; rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    rd.Width = 1920; rd.Height = 1080; rd.DepthOrArraySize = 1; rd.MipLevels = 1;
    rd.Format = DXGI_FORMAT_R8G8B8A8_UNORM; rd.SampleDesc.Count = 1; rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    ComPtr<ID3D12Resource> texture;
    if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&texture)))) return 3;
    ComPtr<ID3D12Fence> fence; device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    HANDLE done = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    UINT64 value = 0;
    auto wait = [&] { queue->Signal(fence.Get(), ++value); if (fence->GetCompletedValue() < value) { fence->SetEventOnCompletion(value, done); WaitForSingleObject(done, 5000); } };
    WNDCLASSW wc{}; wc.lpfnWndProc = DefWindowProcW; wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"BridgeBench";
    RegisterClassW(&wc);
    HWND window = CreateWindowW(wc.lpszClassName, L"BridgeBench", WS_OVERLAPPEDWINDOW, 0, 0, 640, 480, nullptr, nullptr, wc.hInstance, nullptr);
    DXGI_SWAP_CHAIN_DESC1 sd{}; sd.Width = 1920; sd.Height = 1080; sd.Format = rd.Format; sd.SampleDesc.Count = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; sd.BufferCount = 2; sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    ComPtr<IDXGISwapChain1> swapchain;
    if (FAILED(factory->CreateSwapChainForHwnd(queue.Get(), window, &sd, nullptr, nullptr, &swapchain))) return 6;
    texture.Reset(); swapchain->GetBuffer(0, IID_PPV_ARGS(&texture));
    LARGE_INTEGER freq; QueryPerformanceFrequency(&freq);
    for (int pass = 0; pass < 4; ++pass) {
        bool cached = (pass & 1) != 0;
        ComPtr<ID3D11Resource> wrapped; ComPtr<ID3D11RenderTargetView> target;
        std::vector<double> times;
        for (int i = 0; i < 1200; ++i) {
            if (i == 600) {
                target.Reset(); wrapped.Reset(); texture.Reset(); context->ClearState(); context->Flush(); wait();
                if (FAILED(swapchain->ResizeBuffers(2, 1920, 1080, rd.Format, 0))) return 7;
                swapchain->GetBuffer(0, IID_PPV_ARGS(&texture));
            }
            LARGE_INTEGER start, end; QueryPerformanceCounter(&start);
            if (!wrapped) {
                D3D11_RESOURCE_FLAGS flags{D3D11_BIND_RENDER_TARGET};
                if (FAILED(bridge->CreateWrappedResource(texture.Get(), &flags, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT, IID_PPV_ARGS(&wrapped)))) return 4;
                if (FAILED(d11->CreateRenderTargetView(wrapped.Get(), nullptr, &target))) return 5;
            }
            ID3D11Resource* raw = wrapped.Get(); bridge->AcquireWrappedResources(&raw, 1);
            context->OMSetRenderTargets(1, target.GetAddressOf(), nullptr);
            float color[] = {0.2f, 0.3f, 0.4f, 1.f}; context->ClearRenderTargetView(target.Get(), color);
            context->OMSetRenderTargets(0, nullptr, nullptr);
            bridge->ReleaseWrappedResources(&raw, 1); context->ClearState(); context->Flush();
            if (!cached) { target.Reset(); wrapped.Reset(); context->Flush(); }
            QueryPerformanceCounter(&end);
            if (i >= 100) times.push_back(double(end.QuadPart - start.QuadPart) * 1000000.0 / freq.QuadPart);
            wait();
        }
        target.Reset(); wrapped.Reset(); context->ClearState(); context->Flush(); wait();
        std::sort(times.begin(), times.end()); double sum = 0; for (auto t : times) sum += t;
        std::printf("%s: mean %.2f us median %.2f us p99 %.2f us\n", cached ? "cached/one flush" : "recreate/two flushes", sum/times.size(), times[times.size()/2], times[size_t(times.size()*0.99)]);
    }
    DestroyWindow(window);
    CloseHandle(done);
}
