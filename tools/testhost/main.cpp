#include <windows.h>
#include <mmsystem.h>
#include <d3d11.h>
#include <dxgi.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <vector>

static HWND hwnd;
static int viewW = 1280, viewH = 720;
static bool running = true;

static LRESULT CALLBACK proc(HWND w, UINT m, WPARAM wp, LPARAM lp) {
    if (m == WM_DESTROY || m == WM_CLOSE) {
        running = false;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(w, m, wp, lp);
}

static void key(int vk, bool down) {
    PostMessageW(hwnd, down ? WM_KEYDOWN : WM_KEYUP, vk, 1);
}

struct Queued {
    DWORD due;
    UINT msg;
    WPARAM wp;
    LPARAM lp;
};
static std::vector<Queued> queued;

static void click(int x, int y) {
    LPARAM lp = MAKELPARAM(x, y);
    DWORD now = GetTickCount();
    queued.push_back({now, WM_MOUSEMOVE, 0, lp});
    queued.push_back({now + 150, WM_LBUTTONDOWN, MK_LBUTTON, lp});
    queued.push_back({now + 260, WM_LBUTTONUP, 0, lp});
}

static void wheel(int x, int y, int notches) {
    LPARAM lp = MAKELPARAM(x, y);
    DWORD now = GetTickCount();
    queued.push_back({now, WM_MOUSEMOVE, 0, lp});
    queued.push_back({now + 100, WM_MOUSEWHEEL, WPARAM(MAKELONG(0, short(-120 * notches))), lp});
}

static void flushQueued() {
    DWORD now = GetTickCount();
    for (size_t i = 0; i < queued.size();) {
        if (queued[i].due <= now) {
            PostMessageW(hwnd, queued[i].msg, queued[i].wp, queued[i].lp);
            queued.erase(queued.begin() + i);
        } else i++;
    }
}

struct Step {
    double at;
    char kind;
    int a, b;
    bool done;
};

static std::vector<Step> loadScript() {
    std::vector<Step> out;
    const char* env = std::getenv("TESTHOST_SCRIPT");
    if (!env) return out;
    std::string all = env;
    size_t from = 0;
    while (from < all.size()) {
        size_t to = all.find(';', from);
        if (to == std::string::npos) to = all.size();
        Step s{};
        char kind = 0;
        int a = 0, b = 0;
        if (std::sscanf(all.substr(from, to - from).c_str(), "%lf:%c:%d,%d", &s.at, &kind, &a, &b) >= 2) {
            s.kind = kind;
            s.a = a;
            s.b = b;
            out.push_back(s);
        }
        from = to + 1;
    }
    return out;
}

int wmain(int argc, wchar_t** argv) {
    if (argc < 2) {
        std::printf("usage: testhost <Monchi.dll> [seconds]\n");
        return 1;
    }
    int seconds = argc > 2 ? _wtoi(argv[2]) : 26;
    int targetFps = std::getenv("TESTHOST_FPS") ? std::atoi(std::getenv("TESTHOST_FPS")) : 0;
    timeBeginPeriod(1);
    LARGE_INTEGER qf, qlast, qnow;
    QueryPerformanceFrequency(&qf);
    QueryPerformanceCounter(&qlast);
    std::vector<double> intervals;

    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"TestHost";
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    RegisterClassExW(&wc);

    if (const char* size = std::getenv("TESTHOST_SIZE")) std::sscanf(size, "%dx%d", &viewW, &viewH);
    bool manual = std::getenv("TESTHOST_MANUAL") != nullptr;
    RECT r{0, 0, viewW, viewH};
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    int x = std::getenv("TESTHOST_OFFSCREEN") ? -20000 : 0;
    hwnd = CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, wc.lpszClassName, L"TestHost", WS_OVERLAPPEDWINDOW, x, 0,
                           r.right - r.left, r.bottom - r.top, nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);

    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = viewW;
    sd.BufferDesc.Height = viewH;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    IDXGISwapChain* sc = nullptr;
    ID3D11Device* dev = nullptr;
    ID3D11DeviceContext* ctx = nullptr;
    D3D_FEATURE_LEVEL fl = D3D_FEATURE_LEVEL_11_0;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, &fl, 1, D3D11_SDK_VERSION,
                                               &sd, &sc, &dev, nullptr, &ctx);
    if (FAILED(hr)) {
        std::printf("device failed 0x%08lx\n", (unsigned long)hr);
        return 2;
    }

    ID3D11Texture2D* back = nullptr;
    sc->GetBuffer(0, IID_PPV_ARGS(&back));
    ID3D11Texture2D* pattern = nullptr;
    if (const char* wantPattern = std::getenv("TESTHOST_PATTERN"); wantPattern && *wantPattern) {
        std::vector<unsigned> px(size_t(viewW) * viewH);
        for (int y = 0; y < viewH; y++)
            for (int x = 0; x < viewW; x++) {
                bool a = ((x / 32) + (y / 32)) % 2 == 0;
                unsigned r = a ? 230 : 40, g = unsigned(60 + (x * 160) / viewW), b = unsigned(80 + (y * 150) / viewH);
                px[size_t(y) * viewW + x] = 0xFF000000u | (b << 16) | (g << 8) | r;
            }
        D3D11_TEXTURE2D_DESC td{};
        td.Width = viewW;
        td.Height = viewH;
        td.MipLevels = td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        D3D11_SUBRESOURCE_DATA init{px.data(), UINT(viewW * 4), 0};
        dev->CreateTexture2D(&td, &init, &pattern);
    }
    ID3D11RenderTargetView* rtv = nullptr;
    dev->CreateRenderTargetView(back, nullptr, &rtv);

    constexpr int tileSize = 48;
    std::vector<unsigned> tilePixels(tileSize * tileSize);
    for (int y = 0; y < tileSize; y++)
        for (int x = 0; x < tileSize; x++) tilePixels[size_t(y * tileSize + x)] = ((x / 8 + y / 8) & 1) ? 0xff2a7a3a : 0xff358f45;
    D3D11_TEXTURE2D_DESC tileDesc{tileSize, tileSize, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, {1, 0}, D3D11_USAGE_DEFAULT, 0, 0, 0};
    D3D11_SUBRESOURCE_DATA tileData{tilePixels.data(), tileSize * 4, 0};
    ID3D11Texture2D* tile = nullptr;
    dev->CreateTexture2D(&tileDesc, &tileData, &tile);

    HMODULE monchi = LoadLibraryW(argv[1]);
    std::printf("LoadLibrary -> %p (err %lu)\n", (void*)monchi, monchi ? 0 : GetLastError());
    std::fflush(stdout);

    auto script = loadScript();
    DWORD start = GetTickCount();
    int step = 0;
    int frames = 0;
    while (running) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        flushQueued();
        double t = (GetTickCount() - start) / 1000.0;

        float sky = 0.5f + 0.1f * std::sin(t);
        float color[4] = {0.35f, sky, 0.9f, 1.f};
        ctx->OMSetRenderTargets(1, &rtv, nullptr);
        D3D11_VIEWPORT vp{0, 0, (float)viewW, (float)viewH, 0, 1};
        ctx->RSSetViewports(1, &vp);
        ctx->ClearRenderTargetView(rtv, color);
        if (pattern) ctx->CopyResource(back, pattern);
        else if (tile) {
            ID3D11Texture2D* bb = nullptr;
            sc->GetBuffer(0, IID_PPV_ARGS(&bb));
            for (int y = viewH * 11 / 20; y + tileSize <= viewH; y += tileSize)
                for (int x = 0; x + tileSize <= viewW; x += tileSize) ctx->CopySubresourceRegion(bb, 0, x, y, 0, tile, 0, nullptr);
            bb->Release();
        }

        sc->Present(0, 0);
        frames++;

        struct Action {
            double at;
            const char* name;
            void (*run)();
        };
        static const Action actions[] = {
            {3, "open", [] { key(VK_RSHIFT, true); key(VK_RSHIFT, false); }},
            {6, "card", [] { click(580, 255); }},
            {9, "themes", [] { click(333, 248); }},
            {12, "settings", [] { click(325, 306); }},
            {15, "hudedit", [] { click(365, 557); }},
            {18, "back", [] { key(VK_ESCAPE, true); key(VK_ESCAPE, false); }},
            {21, "unload", [] { key(VK_CONTROL, true); key('L', true); key('L', false); key(VK_CONTROL, false); }},
        };
        for (auto& s : script) {
            if (s.done || t < s.at) continue;
            s.done = true;
            if (s.kind == 'k') {
                key(s.a, true);
                key(s.a, false);
            } else if (s.kind == 't') {
                PostMessageW(hwnd, WM_CHAR, WPARAM(s.a), 1);
            } else if (s.kind == 'c') {
                click(s.a, s.b);
            } else if (s.kind == 'w') {
                wheel(s.a, s.b, 3);
            } else if (s.kind == 'u') {
                key(VK_CONTROL, true);
                key('L', true);
                key('L', false);
                key(VK_CONTROL, false);
            }
        }
        if (!manual && step < (int)(sizeof(actions) / sizeof(actions[0])) && t > actions[step].at) {
            actions[step].run();
            std::printf("action %s\n", actions[step].name);
            std::fflush(stdout);
            step++;
        }
        static double lastReport = 0.0;
        static int lastFrames = 0;
        if (t - lastReport >= 2.0) {
            std::printf("t=%.0f fps=%.1f\n", t, (frames - lastFrames) / (t - lastReport));
            std::fflush(stdout);
            lastReport = t;
            lastFrames = frames;
        }
        if (t > seconds) break;
        if (targetFps > 0) {
            double want = 1.0 / targetFps;
            for (;;) {
                QueryPerformanceCounter(&qnow);
                double spent = double(qnow.QuadPart - qlast.QuadPart) / double(qf.QuadPart);
                if (spent >= want) break;
                if (want - spent > 0.002) Sleep(1);
            }
        } else {
            Sleep(4);
        }
        QueryPerformanceCounter(&qnow);
        intervals.push_back(double(qnow.QuadPart - qlast.QuadPart) * 1000.0 / double(qf.QuadPart));
        qlast = qnow;
    }
    if (!intervals.empty()) {
        std::vector<double> sorted = intervals;
        std::sort(sorted.begin(), sorted.end());
        double sum = 0;
        for (double v : sorted) sum += v;
        std::printf("host frame ms: mean %.2f median %.2f p99 %.2f max %.2f\n", sum / sorted.size(), sorted[sorted.size() / 2],
                    sorted[size_t(sorted.size() * 0.99)], sorted.back());
    }
    std::printf("frames=%d still_loaded=%d\n", frames, GetModuleHandleW(L"Monchi.dll") != nullptr);
    return 0;
}
