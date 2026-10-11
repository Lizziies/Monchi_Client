#include "Dx.hpp"
#include "Input.hpp"
#include "FrameWait.hpp"
#include "Hook.hpp"
#include "core/Guard.hpp"
#include "core/Finally.hpp"
#include "core/Log.hpp"
#include "modules/post/Capture.hpp"
#include "render/Ui.hpp"
#include "system/GpuLatency.hpp"

#include <d3d11.h>
#include <d3d11on12.h>
#include <d3d12.h>
#include <dxgi1_5.h>

#include <atomic>
#include <mutex>
#include <vector>

namespace dx {

using PresentFn = HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT);
using Present1Fn = HRESULT(WINAPI*)(IDXGISwapChain1*, UINT, UINT, const DXGI_PRESENT_PARAMETERS*);
using ResizeFn = HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
using Resize1Fn = HRESULT(WINAPI*)(IDXGISwapChain3*, UINT, UINT, UINT, DXGI_FORMAT, UINT, const UINT*, IUnknown* const*);
using ExecuteFn = void(WINAPI*)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);

static PresentFn oPresent = nullptr;
static Present1Fn oPresent1 = nullptr;
static ResizeFn oResize = nullptr;
static Resize1Fn oResize1 = nullptr;
static ExecuteFn oExecute = nullptr;

static Api current = Api::None;
static HWND hwnd = nullptr;
static Tuning tune;
static FrameInfo info;
static thread_local bool inPresent = false;
static std::atomic<int> presenting{0};
static std::atomic<bool> dead{false};

static ID3D11Device* d11 = nullptr;
static ID3D11DeviceContext* ctx = nullptr;
static ID3D11On12Device* on12 = nullptr;
static ID3D12CommandQueue* queue = nullptr;
static IDXGISwapChain* chain = nullptr;
static ID3D12Device* device12 = nullptr;
static bool uiReady = false;
static bool latencyApplied = false;
static int setupCooldown = 0;
static int setupFailures = 0;

struct Buffer {
    ID3D11Resource* wrapped = nullptr;
    ID3D11RenderTargetView* rtv = nullptr;
    bool acquired = false;
};
static ID3D11RenderTargetView* rtv11 = nullptr;
static std::vector<Buffer> cachedBuffers;
static void releaseBuffer(Buffer& b);

static LARGE_INTEGER qpf{};
static int64_t lastPresent = 0;
static HANDLE limiterTimer = nullptr;

template <class T>
static void release(T*& p) {
    if (p) p->Release();
    p = nullptr;
}

static int64_t now() {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}

static ID3D12Fence* fence = nullptr;
static HANDLE fenceEvent = nullptr;
static UINT64 fenceValue = 0;

// 11on12 keeps its references to the swapchain buffers until the queue has finished the work that used them,
// and ResizeBuffers fails (the game then aborts) while any reference is left
static void waitGpu() {
    if (!queue) return;
    if (!fence) {
        ID3D12Device* device = nullptr;
        if (FAILED(queue->GetDevice(IID_PPV_ARGS(&device)))) return;
        HRESULT hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
        device->Release();
        if (FAILED(hr)) return;
        fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    }
    if (!fenceEvent) return;
    UINT64 value = ++fenceValue;
    if (FAILED(queue->Signal(fence, value)) || fence->GetCompletedValue() >= value) return;
    if (SUCCEEDED(fence->SetEventOnCompletion(value, fenceEvent))) {
        ULONGLONG started = GetTickCount64();
        DWORD result = WaitForSingleObject(fenceEvent, 2000);
        ULONGLONG elapsed = GetTickCount64() - started;
        if (elapsed >= 250 || result != WAIT_OBJECT_0)
            logger::warn("renderer: resize GPU wait {} ms, result {}, completed {}, target {}", elapsed, result, fence->GetCompletedValue(), value);
    }
}

static void dropTargets() {
    release(rtv11);
    for (auto& b : cachedBuffers) releaseBuffer(b);
    cachedBuffers.clear();
    if (ctx) {
        ctx->ClearState();
        ctx->Flush();
    }
    if (current == Api::Dx12) waitGpu();
}

static void releaseBuffer(Buffer& b) {
    release(b.rtv);
    release(b.wrapped);
}

static bool wrapBuffer(IDXGISwapChain* sc, UINT index, Buffer& out) {
    DXGI_SWAP_CHAIN_DESC desc{};
    sc->GetDesc(&desc);
    info.bufferCount = (int)desc.BufferCount;

    ID3D12Resource* res = nullptr;
    if (FAILED(sc->GetBuffer(index, IID_PPV_ARGS(&res)))) return false;

    D3D11_RESOURCE_FLAGS flags{D3D11_BIND_RENDER_TARGET};
    HRESULT hr = on12->CreateWrappedResource(res, &flags, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PRESENT,
                                             IID_PPV_ARGS(&out.wrapped));
    res->Release();
    if (FAILED(hr)) return false;
    return SUCCEEDED(d11->CreateRenderTargetView(out.wrapped, nullptr, &out.rtv));
}

static bool buildTargets11(IDXGISwapChain* sc) {
    ID3D11Texture2D* back = nullptr;
    if (FAILED(sc->GetBuffer(0, IID_PPV_ARGS(&back)))) return false;
    HRESULT hr = d11->CreateRenderTargetView(back, nullptr, &rtv11);
    back->Release();
    DXGI_SWAP_CHAIN_DESC desc{};
    sc->GetDesc(&desc);
    info.bufferCount = (int)desc.BufferCount;
    return SUCCEEDED(hr);
}

static HWND findWindow(IDXGISwapChain* sc) {
    DXGI_SWAP_CHAIN_DESC desc{};
    if (SUCCEEDED(sc->GetDesc(&desc)) && desc.OutputWindow) return desc.OutputWindow;

    IDXGISwapChain1* sc1 = nullptr;
    HWND out = nullptr;
    if (SUCCEEDED(sc->QueryInterface(IID_PPV_ARGS(&sc1)))) {
        sc1->GetHwnd(&out);
        sc1->Release();
    }
    if (out) return out;

    struct Search {
        DWORD pid;
        HWND best;
        long area;
    } s{GetCurrentProcessId(), nullptr, 0};

    EnumWindows([](HWND w, LPARAM lp) -> BOOL {
        auto* s = reinterpret_cast<Search*>(lp);
        DWORD pid = 0;
        GetWindowThreadProcessId(w, &pid);
        if (pid != s->pid || !IsWindowVisible(w)) return TRUE;
        RECT r{};
        GetClientRect(w, &r);
        long area = (r.right - r.left) * (r.bottom - r.top);
        if (area > s->area) {
            s->area = area;
            s->best = w;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&s));
    return s.best;
}

static bool setup(IDXGISwapChain* sc) {
    ID3D12Device* d12 = nullptr;
    if (SUCCEEDED(sc->GetDevice(IID_PPV_ARGS(&d12)))) {
        ID3D12CommandQueue* own = nullptr;
        if (SUCCEEDED(sc->GetDevice(IID_PPV_ARGS(&own)))) {
            release(queue);
            queue = own;
        }
        if (!queue) {
            d12->Release();
            return false;
        }

        device12 = d12;
        IUnknown* queues[] = {queue};
        HRESULT hr = D3D11On12CreateDevice(d12, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, queues, 1, 0, &d11,
                                           &ctx, nullptr);
        d12->Release();
        if (FAILED(hr)) {
            logger::error("D3D11On12CreateDevice failed 0x{:08X}", (unsigned)hr);
            return false;
        }
        d11->QueryInterface(IID_PPV_ARGS(&on12));
        current = Api::Dx12;
    } else if (SUCCEEDED(sc->GetDevice(IID_PPV_ARGS(&d11)))) {
        d11->GetImmediateContext(&ctx);
        current = Api::Dx11;
    } else {
        return false;
    }

    hwnd = findWindow(sc);
    chain = sc;
    logger::info("renderer: {} hwnd={}", current == Api::Dx12 ? "dx12 (11on12)" : "dx11", (void*)hwnd);

    DXGI_SWAP_CHAIN_DESC desc{};
    sc->GetDesc(&desc);
    info.tearingSupported = (desc.Flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING) != 0;

    uiReady = ui::init(hwnd, d11, ctx);
    if (uiReady) {
        gpuLatency::attach(current == Api::Dx12 ? static_cast<IUnknown*>(device12) : static_cast<IUnknown*>(d11));
        auto gpu = gpuLatency::adapter(current == Api::Dx12 ? static_cast<IUnknown*>(device12) : static_cast<IUnknown*>(d11));
        logger::info("latency adapter: vendor=0x{:X}, device=0x{:X}", gpu.vendorId, gpu.deviceId);
    }
    return uiReady;
}

static IDXGISwapChain2* latencyChain = nullptr;
static IDXGIDevice1* latencyDevice = nullptr;
static UINT savedLatency = 0;

static bool restoreLatency() {
    HRESULT hr = S_OK;
    if (latencyChain) hr = latencyChain->SetMaximumFrameLatency(savedLatency);
    if (latencyDevice) hr = latencyDevice->SetMaximumFrameLatency(savedLatency);
    if (FAILED(hr)) return false;
    release(latencyChain);
    release(latencyDevice);
    savedLatency = 0;
    info.lowLatencyActive = false;
    return true;
}

static void applyLatency(IDXGISwapChain* sc) {
    if (latencyApplied == tune.lowLatency) return;
    if (!tune.lowLatency) {
        if (restoreLatency()) latencyApplied = false;
        return;
    }
    latencyApplied = true;
    if (latencyChain || latencyDevice) {
        HRESULT hr = latencyChain ? latencyChain->SetMaximumFrameLatency(1) : latencyDevice->SetMaximumFrameLatency(1);
        info.lowLatencyActive = SUCCEEDED(hr);
        return;
    }
    DXGI_SWAP_CHAIN_DESC desc{};
    sc->GetDesc(&desc);
    if (desc.Flags & DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT) {
        if (SUCCEEDED(sc->QueryInterface(IID_PPV_ARGS(&latencyChain)))) {
            if (FAILED(latencyChain->GetMaximumFrameLatency(&savedLatency))) {
                release(latencyChain);
                return;
            }
            info.lowLatencyActive = SUCCEEDED(latencyChain->SetMaximumFrameLatency(1));
        }
    } else if (current == Api::Dx11) {
        if (SUCCEEDED(d11->QueryInterface(IID_PPV_ARGS(&latencyDevice)))) {
            if (FAILED(latencyDevice->GetMaximumFrameLatency(&savedLatency))) {
                release(latencyDevice);
                return;
            }
            info.lowLatencyActive = SUCCEEDED(latencyDevice->SetMaximumFrameLatency(1));
        }
    }
}

static int64_t paced = 0;
static int64_t handed = 0;

// The frame limit paces the start of a frame, so while frame times swing (the first minute on a server, chunks
// streaming in) a slow frame followed by a fast one reaches the display sooner than one refresh after the last. A
// variable-rate display then runs out of its range for that frame and tears or holds it. Only such early frames
// wait here, and only for the difference.
static void holdPresent() {
    int64_t t = now();
    if (tune.presentFloor >= 1.f && handed) {
        int64_t target = handed + int64_t(double(qpf.QuadPart) / tune.presentFloor);
        int64_t remaining = target - t;
        if (remaining > 0 && remaining < qpf.QuadPart / 50) {
            if (!limiterTimer)
                limiterTimer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
            int64_t spin = qpf.QuadPart / 2000;
            if (remaining > spin && limiterTimer) {
                LARGE_INTEGER due;
                due.QuadPart = -((remaining - spin) * 10000000 / qpf.QuadPart);
                if (SetWaitableTimer(limiterTimer, &due, 0, nullptr, nullptr, FALSE))
                    WaitForSingleObject(limiterTimer, DWORD(remaining * 1000 / qpf.QuadPart + 2));
            }
            while (now() < target) YieldProcessor();
            t = now();
            info.heldPresents++;
        }
    }
    handed = t;
}

// Waits after Present, before the game starts its next frame: the next frame then samples input right before
// it is drawn. Waiting before Present would hold back a frame that was already rendered with older input.
static void limit() {
    if (tune.fpsLimit < 1.f) {
        paced = 0;
        return;
    }
    if (!limiterTimer)
        limiterTimer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);

    int64_t period = (int64_t)(qpf.QuadPart / tune.fpsLimit);
    int64_t target = paced + period;
    int64_t t = now();
    if (!paced || t >= target) {
        paced = t;
        return;
    }
    paced = target;

    int64_t remaining = target - t;
    int64_t spin = qpf.QuadPart / 2000;
    bool background = !input::focused();
    if (remaining > spin && limiterTimer) {
        LARGE_INTEGER due;
        due.QuadPart = -((remaining - spin) * 10000000 / qpf.QuadPart);
        if (SetWaitableTimer(limiterTimer, &due, 0, nullptr, nullptr, FALSE)) {
            DWORD timeout = DWORD((remaining * 1000) / qpf.QuadPart + 2);
            if (background && !pacing::waitBackground(limiterTimer, timeout, input::focused)) {
                CancelWaitableTimer(limiterTimer);
                paced = 0;
                return;
            }
            if (!background) WaitForSingleObject(limiterTimer, timeout);
        }
    }
    while (now() < target) YieldProcessor();
}

// The game recreates its swapchain on some fullscreen and video changes. A new one for our window on the same
// device is taken over; anything else is left alone.
static bool adopt(IDXGISwapChain* sc) {
    if (findWindow(sc) != hwnd) return false;
    bool same = false;
    if (current == Api::Dx12) {
        ID3D12Device* dev = nullptr;
        if (SUCCEEDED(sc->GetDevice(IID_PPV_ARGS(&dev)))) {
            same = dev == device12;
            dev->Release();
        }
    } else {
        ID3D11Device* dev = nullptr;
        if (SUCCEEDED(sc->GetDevice(IID_PPV_ARGS(&dev)))) {
            same = dev == d11;
            dev->Release();
        }
    }
    static bool warned = false;
    if (!same) {
        if (!warned) logger::warn("renderer: the game made a swapchain on another device, overlay stays on the old one");
        warned = true;
        return false;
    }
    if (!restoreLatency()) return false;
    dropTargets();
    chain = sc;
    latencyApplied = !tune.lowLatency;
    DXGI_SWAP_CHAIN_DESC desc{};
    sc->GetDesc(&desc);
    info.tearingSupported = (desc.Flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING) != 0;
    logger::info("renderer: took over the game's new swapchain");
    return true;
}

static void drawWrapped(Buffer& buffer) {
    guard::withCleanup([](void* arg) {
        auto& buffer = *static_cast<Buffer*>(arg);
        on12->AcquireWrappedResources(&buffer.wrapped, 1);
        buffer.acquired = true;
        ctx->OMSetRenderTargets(1, &buffer.rtv, nullptr);
        ui::frame();
        capture::grabFinal(d11, ctx);
    }, [](void* arg) {
        auto& buffer = *static_cast<Buffer*>(arg);
        if (buffer.acquired) {
            ctx->OMSetRenderTargets(0, nullptr, nullptr);
            on12->ReleaseWrappedResources(&buffer.wrapped, 1);
            buffer.acquired = false;
            ctx->ClearState();
            ctx->Flush();
        }
        if (!tune.efficientOverlay) {
            releaseBuffer(buffer);
            ctx->Flush();
        }
        info.overlayFlushes = tune.efficientOverlay ? 1 : 2;
    }, &buffer);
}

static void draw(IDXGISwapChain* sc) {
    if (dead) return;
    if (!uiReady) {
        if (setupCooldown > 0) {
            setupCooldown--;
            return;
        }
        if (!setup(sc)) {
            release(on12);
            release(ctx);
            release(d11);
            setupCooldown = 120;
            if (++setupFailures >= 10) {
                logger::error("renderer setup failed {} times, giving up", setupFailures);
                dead = true;
            }
            return;
        }
    }
    if (sc != chain && !adopt(sc)) return;

    applyLatency(sc);

    if (current == Api::Dx12) {
        IDXGISwapChain3* sc3 = nullptr;
        if (FAILED(sc->QueryInterface(IID_PPV_ARGS(&sc3)))) return;
        UINT idx = sc3->GetCurrentBackBufferIndex();
        sc3->Release();

        // Cached references are released before ResizeBuffers, swapchain adoption and unload.
        if (tune.efficientOverlay) {
            if (idx >= 16) return;
            if (cachedBuffers.size() <= idx) cachedBuffers.resize(idx + 1);
            auto& cached = cachedBuffers[idx];
            if (!cached.wrapped && !wrapBuffer(sc, idx, cached)) { releaseBuffer(cached); return; }
            drawWrapped(cached);
            return;
        }
        if (!cachedBuffers.empty()) dropTargets();
        Buffer b;
        if (!wrapBuffer(sc, idx, b)) {
            releaseBuffer(b);
            return;
        }
        drawWrapped(b);
    } else {
        if (!rtv11 && !buildTargets11(sc)) return;
        ctx->OMSetRenderTargets(1, &rtv11, nullptr);
        ui::frame();
        capture::grabFinal(d11, ctx);
    }
}

static void beforePresent(IDXGISwapChain* sc, UINT& sync, UINT& flags) {
    if (flags & DXGI_PRESENT_TEST) return;
    info.nativeSync = sync;
    const int64_t overlayStart = now();
    guard::call("present", [&] { draw(sc); });
    info.overlayMs = double(now() - overlayStart) * 1000.0 / double(qpf.QuadPart);
    if (info.overlayMs >= 250.0) logger::warn("renderer: Monchi frame stalled {:.1f} ms", info.overlayMs);
    if (sc != chain) return;

    if (tune.allowTearing && info.tearingSupported && !(flags & DXGI_PRESENT_TEST)) {
        BOOL fullscreen = FALSE;
        sc->GetFullscreenState(&fullscreen, nullptr);
        if (!fullscreen) {
            sync = 0;
            flags |= DXGI_PRESENT_ALLOW_TEARING;
        }
    }
    if (tune.syncToDisplay) {
        // Every frame waits for the display. A windowed flip without the tearing flag still tears with interval 0:
        // at 179 fps, just under the refresh rate of a variable-rate display, a tear line runs through the lower part
        // of the picture while moving. With the cap just under the refresh rate the wait adds no queue.
        sync = 1;
        flags &= ~DXGI_PRESENT_ALLOW_TEARING;
    }
    info.presentSync = sync;
    info.presentFlags = flags;
    holdPresent();
}

static void afterPresent(IDXGISwapChain* sc, UINT flags, HRESULT result) {
    if (sc != chain || flags & DXGI_PRESENT_TEST || result != S_OK) return;
    int64_t t = now();
    if (lastPresent) info.frameMs = double(t - lastPresent) * 1000.0 / double(qpf.QuadPart);
    lastPresent = t;
    info.presentQpc = t;
    limit();
}

static HRESULT presentVia(PresentFn next, IDXGISwapChain* sc, UINT sync, UINT flags) {
    if (inPresent) return next(sc, sync, flags);
    inPresent = true;
    presenting++;
    beforePresent(sc, sync, flags);
    int64_t started = now();
    HRESULT hr = next(sc, sync, flags);
    double elapsed = double(now() - started) * 1000.0 / double(qpf.QuadPart);
    if (elapsed >= 250.0) logger::warn("renderer: native Present stalled {:.1f} ms, result 0x{:X}, sync {}, cap {:.0f}, focused {}", elapsed, unsigned(hr), sync, tune.fpsLimit, input::focused());
    afterPresent(sc, flags, hr);
    inPresent = false;
    presenting--;
    return hr;
}

static HRESULT present1Via(Present1Fn next, IDXGISwapChain1* sc, UINT sync, UINT flags, const DXGI_PRESENT_PARAMETERS* params) {
    if (inPresent) return next(sc, sync, flags, params);
    inPresent = true;
    presenting++;
    beforePresent(sc, sync, flags);
    // Our HUD changes pixels outside Minecraft's dirty/scroll rectangles.
    DXGI_PRESENT_PARAMETERS full{};
    const auto* presented = sc == chain && uiReady && !dead && !(flags & DXGI_PRESENT_TEST) ? &full : params;
    int64_t started = now();
    HRESULT hr = next(sc, sync, flags, presented);
    double elapsed = double(now() - started) * 1000.0 / double(qpf.QuadPart);
    if (elapsed >= 250.0) logger::warn("renderer: native Present1 stalled {:.1f} ms, result 0x{:X}, sync {}, cap {:.0f}, focused {}", elapsed, unsigned(hr), sync, tune.fpsLimit, input::focused());
    afterPresent(sc, flags, hr);
    inPresent = false;
    presenting--;
    return hr;
}

static thread_local bool inResize = false;

static HRESULT resizeVia(ResizeFn next, IDXGISwapChain* sc, UINT count, UINT w, UINT h, DXGI_FORMAT fmt, UINT flags) {
    if (inResize) return next(sc, count, w, h, fmt, flags);
    inResize = true;
    if (sc == chain) {
        guard::call("resize targets", [] { dropTargets(); ui::invalidate(); });
    }
    HRESULT hr = next(sc, count, w, h, fmt, flags);
    if (sc == chain) {
        DXGI_SWAP_CHAIN_DESC desc{};
        sc->GetDesc(&desc);
        info.tearingSupported = (desc.Flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING) != 0;
        latencyApplied = !tune.lowLatency;
    }
    inResize = false;
    return hr;
}

static HRESULT resize1Via(Resize1Fn next, IDXGISwapChain3* sc, UINT count, UINT w, UINT h, DXGI_FORMAT fmt,
                          UINT flags, const UINT* nodes, IUnknown* const* queues) {
    if (inResize) return next(sc, count, w, h, fmt, flags, nodes, queues);
    inResize = true;
    if (sc == chain) {
        guard::call("resize targets", [] { dropTargets(); ui::invalidate(); });
    }
    HRESULT hr = next(sc, count, w, h, fmt, flags, nodes, queues);
    if (sc == chain) {
        DXGI_SWAP_CHAIN_DESC desc{};
        sc->GetDesc(&desc);
        info.tearingSupported = (desc.Flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING) != 0;
        latencyApplied = !tune.lowLatency;
    }
    inResize = false;
    return hr;
}

static HRESULT WINAPI present(IDXGISwapChain* sc, UINT sync, UINT flags) { return presentVia(oPresent, sc, sync, flags); }

static HRESULT WINAPI present1(IDXGISwapChain1* sc, UINT sync, UINT flags, const DXGI_PRESENT_PARAMETERS* params) {
    return present1Via(oPresent1, sc, sync, flags, params);
}

static HRESULT WINAPI resize(IDXGISwapChain* sc, UINT count, UINT w, UINT h, DXGI_FORMAT fmt, UINT flags) {
    return resizeVia(oResize, sc, count, w, h, fmt, flags);
}

static HRESULT WINAPI resize1(IDXGISwapChain3* sc, UINT count, UINT w, UINT h, DXGI_FORMAT fmt, UINT flags,
                              const UINT* nodes, IUnknown* const* queues) {
    return resize1Via(oResize1, sc, count, w, h, fmt, flags, nodes, queues);
}

// Overlays like RTSS and MSI Afterburner restore the first bytes of dxgi's Present from time to time, which
// silently removes an inline hook. The swapchain vtable is patched as well, so frames keep coming either way;
// the thread-local flags stop a frame from being drawn twice when both paths are live.
struct TableSlot {
    void** at = nullptr;
    void* original = nullptr;
    void* detour = nullptr;
};
static TableSlot slotPresent, slotPresent1, slotResize, slotResize1;

static HRESULT WINAPI presentTable(IDXGISwapChain* sc, UINT sync, UINT flags) {
    static bool seen = false;
    if (!seen && !inPresent) {
        seen = true;
        logger::info("renderer: frames arrive through the swapchain table");
    }
    return presentVia(reinterpret_cast<PresentFn>(slotPresent.original), sc, sync, flags);
}

static HRESULT WINAPI present1Table(IDXGISwapChain1* sc, UINT sync, UINT flags, const DXGI_PRESENT_PARAMETERS* params) {
    return present1Via(reinterpret_cast<Present1Fn>(slotPresent1.original), sc, sync, flags, params);
}

static HRESULT WINAPI resizeTable(IDXGISwapChain* sc, UINT count, UINT w, UINT h, DXGI_FORMAT fmt, UINT flags) {
    return resizeVia(reinterpret_cast<ResizeFn>(slotResize.original), sc, count, w, h, fmt, flags);
}

static HRESULT WINAPI resize1Table(IDXGISwapChain3* sc, UINT count, UINT w, UINT h, DXGI_FORMAT fmt, UINT flags,
                                   const UINT* nodes, IUnknown* const* queues) {
    return resize1Via(reinterpret_cast<Resize1Fn>(slotResize1.original), sc, count, w, h, fmt, flags, nodes, queues);
}

static void patchTable(void* object, int index, void* detour, TableSlot& slot) {
    void** at = *static_cast<void***>(object) + index;
    DWORD old = 0;
    if (!VirtualProtect(at, sizeof(void*), PAGE_READWRITE, &old)) return;
    slot = {at, *at, detour};
    *at = detour;
    VirtualProtect(at, sizeof(void*), old, &old);
}

static void restoreTable(TableSlot& slot) {
    if (!slot.at) return;
    DWORD old = 0;
    if (VirtualProtect(slot.at, sizeof(void*), PAGE_READWRITE, &old)) {
        if (*slot.at == slot.detour) *slot.at = slot.original;
        VirtualProtect(slot.at, sizeof(void*), old, &old);
    }
    slot = {};
}

void unhookTables() {
    restoreTable(slotPresent);
    restoreTable(slotPresent1);
    restoreTable(slotResize);
    restoreTable(slotResize1);
}

static void WINAPI execute(ID3D12CommandQueue* q, UINT n, ID3D12CommandList* const* lists) {
    if (!queue) {
        D3D12_COMMAND_QUEUE_DESC d = q->GetDesc();
        if (d.Type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
            q->AddRef();
            queue = q;
        }
    }
    oExecute(q, n, lists);
}

static LRESULT CALLBACK dummyProc(HWND w, UINT m, WPARAM wp, LPARAM lp) {
    return DefWindowProcW(w, m, wp, lp);
}

bool install() {
    QueryPerformanceFrequency(&qpf);

    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = dummyProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"MonchiDummy";
    RegisterClassExW(&wc);
    HWND tmp = CreateWindowExW(0, wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr,
                               wc.hInstance, nullptr);

    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 1;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = tmp;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    IDXGISwapChain* sc = nullptr;
    ID3D11Device* dev = nullptr;
    ID3D11DeviceContext* dc = nullptr;
    D3D_FEATURE_LEVEL fl = D3D_FEATURE_LEVEL_11_0;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, &fl, 1,
                                               D3D11_SDK_VERSION, &sd, &sc, &dev, nullptr, &dc);
    if (FAILED(hr)) {
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, &fl, 1, D3D11_SDK_VERSION, &sd,
                                           &sc, &dev, nullptr, &dc);
    }
    if (FAILED(hr)) {
        logger::error("dummy swapchain failed 0x{:08X}", (unsigned)hr);
        DestroyWindow(tmp);
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return false;
    }

    bool ok = hook::create("Present", hook::vfunc(sc, 8), present, &oPresent) &&
              hook::create("ResizeBuffers", hook::vfunc(sc, 13), resize, &oResize);

    IDXGISwapChain1* sc1 = nullptr;
    if (SUCCEEDED(sc->QueryInterface(IID_PPV_ARGS(&sc1)))) {
        hook::create("Present1", hook::vfunc(sc1, 22), present1, &oPresent1);
        patchTable(sc1, 22, reinterpret_cast<void*>(present1Table), slotPresent1);
        sc1->Release();
    }
    if (ok) {
        patchTable(sc, 8, reinterpret_cast<void*>(presentTable), slotPresent);
        patchTable(sc, 13, reinterpret_cast<void*>(resizeTable), slotResize);
    }

    IDXGISwapChain3* sc3 = nullptr;
    if (SUCCEEDED(sc->QueryInterface(IID_PPV_ARGS(&sc3)))) {
        if (hook::create("ResizeBuffers1", hook::vfunc(sc3, 39), resize1, &oResize1))
            patchTable(sc3, 39, reinterpret_cast<void*>(resize1Table), slotResize1);
        sc3->Release();
    }

    ID3D12Device* d12 = nullptr;
    if (SUCCEEDED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d12)))) {
        D3D12_COMMAND_QUEUE_DESC qd{};
        ID3D12CommandQueue* q = nullptr;
        if (SUCCEEDED(d12->CreateCommandQueue(&qd, IID_PPV_ARGS(&q)))) {
            hook::create("ExecuteCommandLists", hook::vfunc(q, 10), execute, &oExecute);
            q->Release();
        }
        d12->Release();
    }

    sc->Release();
    dc->Release();
    dev->Release();
    DestroyWindow(tmp);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return ok;
}

bool uninstall() {
    dead = true;
    if (!restoreLatency()) return false;
    if (uiReady && !ui::shutdown()) return false;
    uiReady = false;
    dropTargets();
    release(on12);
    release(ctx);
    release(d11);
    release(fence);
    if (fenceEvent) CloseHandle(fenceEvent);
    fenceEvent = nullptr;
    release(queue);
    if (limiterTimer) CloseHandle(limiterTimer);
    limiterTimer = nullptr;
    return true;
}

Api api() { return current; }
HWND window() { return hwnd; }
IDXGISwapChain* swapchain() { return chain; }
Tuning& tuning() { return tune; }
const FrameInfo& frame() { return info; }

bool busy() { return presenting.load() > 0; }

}
