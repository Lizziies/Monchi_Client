// A bare D3D12 window that loads Monchi.dll, the way the game does on DX12: Monchi then draws through D3D11On12.
// Prints the frame time with and without the dll, so the cost of that path is a number.
// usage: testhost12 <Monchi.dll | none> [seconds]
#include <windows.h>
#include <mmsystem.h>
#include <d3d12.h>
#include <dxgi1_4.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

static bool running = true;

static LRESULT CALLBACK proc(HWND w, UINT m, WPARAM wp, LPARAM lp) {
    if (m == WM_DESTROY || m == WM_CLOSE) {
        running = false;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(w, m, wp, lp);
}

int wmain(int argc, wchar_t** argv) {
    if (argc < 2) {
        std::printf("usage: testhost12 <Monchi.dll | none> [seconds]\n");
        return 1;
    }
    int seconds = argc > 2 ? _wtoi(argv[2]) : 15;
    bool testResize = argc > 3 && std::wstring(argv[3]) == L"resize";
    unsigned resized = 0;
    int width = 1920, height = 1080;
    timeBeginPeriod(1);

    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"TestHost12";
    RegisterClassExW(&wc);
    RECT r{0, 0, width, height};
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, wc.lpszClassName, L"TestHost12", WS_OVERLAPPEDWINDOW, 0, 0, r.right - r.left,
                                r.bottom - r.top, nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);

    ID3D12Device* device = nullptr;
    if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)))) {
        std::printf("no d3d12 device\n");
        return 2;
    }
    D3D12_COMMAND_QUEUE_DESC qd{};
    ID3D12CommandQueue* queue = nullptr;
    device->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue));

    IDXGIFactory4* factory = nullptr;
    CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    DXGI_SWAP_CHAIN_DESC1 sd{};
    sd.Width = width;
    sd.Height = height;
    sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.SampleDesc.Count = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 3;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    IDXGISwapChain1* sc1 = nullptr;
    if (FAILED(factory->CreateSwapChainForHwnd(queue, hwnd, &sd, nullptr, nullptr, &sc1))) {
        std::printf("no swapchain\n");
        return 3;
    }
    IDXGISwapChain3* sc = nullptr;
    sc1->QueryInterface(IID_PPV_ARGS(&sc));

    D3D12_DESCRIPTOR_HEAP_DESC hd{D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 3};
    ID3D12DescriptorHeap* heap = nullptr;
    device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&heap));
    UINT step = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    ID3D12Resource* buffers[3]{};
    D3D12_CPU_DESCRIPTOR_HANDLE rtv[3]{};
    for (UINT i = 0; i < 3; i++) {
        sc->GetBuffer(i, IID_PPV_ARGS(&buffers[i]));
        rtv[i] = heap->GetCPUDescriptorHandleForHeapStart();
        rtv[i].ptr += SIZE_T(step) * i;
        device->CreateRenderTargetView(buffers[i], nullptr, rtv[i]);
    }
    ID3D12CommandAllocator* alloc = nullptr;
    device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc));
    ID3D12GraphicsCommandList* list = nullptr;
    device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc, nullptr, IID_PPV_ARGS(&list));
    list->Close();
    ID3D12Fence* fence = nullptr;
    device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    UINT64 fenceValue = 0;

    bool none = std::wstring(argv[1]) == L"none";
    HMODULE monchi = none ? nullptr : LoadLibraryW(argv[1]);
    std::printf("dll %s\n", none ? "not loaded" : monchi ? "loaded" : "FAILED to load");

    LARGE_INTEGER qf, last, now;
    QueryPerformanceFrequency(&qf);
    QueryPerformanceCounter(&last);
    std::vector<double> intervals;
    DWORD start = GetTickCount();
    while (running) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        double t = (GetTickCount() - start) / 1000.0;
        if (testResize && resized < 10 && t > 5.0 + resized * 0.4) {
            alloc->Reset();
            list->Reset(alloc, nullptr);
            list->Close();
            for (auto*& buffer : buffers) { buffer->Release(); buffer = nullptr; }
            UINT side = resized % 2 ? 640 : 800;
            UINT masks[] = {1, 1, 1};
            IUnknown* queues[] = {queue, queue, queue};
            HRESULT hr = resized % 2
                ? sc->ResizeBuffers(3, side, 480, sd.Format, sd.Flags)
                : sc->ResizeBuffers1(3, side, 480, sd.Format, sd.Flags, masks, queues);
            if (FAILED(hr)) {
                std::printf("resize %u failed 0x%08X\n", resized, unsigned(hr));
                return 4;
            }
            for (UINT i = 0; i < 3; ++i) {
                if (FAILED(sc->GetBuffer(i, IID_PPV_ARGS(&buffers[i])))) return 5;
                device->CreateRenderTargetView(buffers[i], nullptr, rtv[i]);
            }
            ++resized;
        }
        UINT index = sc->GetCurrentBackBufferIndex();
        alloc->Reset();
        list->Reset(alloc, nullptr);
        D3D12_RESOURCE_BARRIER b{};
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Transition.pResource = buffers[index];
        b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        b.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        b.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        list->ResourceBarrier(1, &b);
        float color[4] = {0.35f, 0.55f, 0.9f, 1.f};
        list->ClearRenderTargetView(rtv[index], color, 0, nullptr);
        std::swap(b.Transition.StateBefore, b.Transition.StateAfter);
        list->ResourceBarrier(1, &b);
        list->Close();
        ID3D12CommandList* lists[] = {list};
        queue->ExecuteCommandLists(1, lists);
        sc->Present(0, DXGI_PRESENT_ALLOW_TEARING);
        queue->Signal(fence, ++fenceValue);
        if (fence->GetCompletedValue() < fenceValue) {
            fence->SetEventOnCompletion(fenceValue, event);
            WaitForSingleObject(event, 1000);
        }
        QueryPerformanceCounter(&now);
        // the first seconds are start-up (the dll finds the swapchain, builds its fonts)
        if (t > 5.0) intervals.push_back(double(now.QuadPart - last.QuadPart) * 1000.0 / double(qf.QuadPart));
        last = now;
        if (t > seconds) break;
    }
    if (!intervals.empty()) {
        std::vector<double> sorted = intervals;
        std::sort(sorted.begin(), sorted.end());
        double sum = 0;
        for (double v : sorted) sum += v;
        std::printf("frame ms: mean %.3f median %.3f p99 %.3f max %.2f over %zu frames (%.0f fps)\n", sum / sorted.size(), sorted[sorted.size() / 2],
                    sorted[size_t(sorted.size() * 0.99)], sorted.back(), sorted.size(), 1000.0 * sorted.size() / sum);
    }
    if (testResize) {
        std::printf("resize transitions: %u/10\n", resized);
        if (resized != 10) return 6;
    }
    return 0;
}
