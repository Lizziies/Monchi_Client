#include "Look.hpp"
#include "App.hpp"
#include "Embedded.hpp"
#include "Files.hpp"
#include "I18n.hpp"
#include "Ui.hpp"

#include "../res/resource.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <d3d11.h>
#include <dwmapi.h>
#include <dxgi.h>
#include <windowsx.h>

#include <cmath>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace {

HWND window = nullptr;
ID3D11Device* device = nullptr;
ID3D11DeviceContext* context = nullptr;
IDXGISwapChain* swap = nullptr;
ID3D11RenderTargetView* target = nullptr;
float scale = 1.f;

void makeTarget() {
    ID3D11Texture2D* back = nullptr;
    swap->GetBuffer(0, IID_PPV_ARGS(&back));
    device->CreateRenderTargetView(back, nullptr, &target);
    back->Release();
}

void dropTarget() {
    if (target) target->Release();
    target = nullptr;
}

bool createDevice() {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = window;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL level;
    const D3D_FEATURE_LEVEL wanted[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, wanted, 2, D3D11_SDK_VERSION,
                                               &sd, &swap, &device, &level, &context);
    if (FAILED(hr))
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, wanted, 2, D3D11_SDK_VERSION, &sd,
                                           &swap, &device, &level, &context);
    if (FAILED(hr)) return false;
    makeTarget();
    return true;
}

void resize(int w, int h) {
    if (!swap || w <= 0 || h <= 0) return;
    dropTarget();
    swap->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0);
    makeTarget();
}

ImFont* loadFont(int id) {
    HRSRC res = FindResourceW(nullptr, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(10));
    if (!res) return nullptr;
    void* bytes = LockResource(LoadResource(nullptr, res));
    int len = (int)SizeofResource(nullptr, res);
    if (!bytes || !len) return nullptr;
    ImFontConfig cfg;
    cfg.FontDataOwnedByAtlas = false;
    cfg.OversampleH = 2;
    return ImGui::GetIO().Fonts->AddFontFromMemoryTTF(bytes, len, 17.f, &cfg);
}

LPARAM unscaled(LPARAM lp) {
    return MAKELPARAM(int(GET_X_LPARAM(lp) / scale), int(GET_Y_LPARAM(lp) / scale));
}

LRESULT CALLBACK proc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    LPARAM forwarded = (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST && msg != WM_MOUSEWHEEL && msg != WM_MOUSEHWHEEL)
                           ? unscaled(lp)
                           : lp;
    if (ImGui_ImplWin32_WndProcHandler(w, msg, wp, forwarded)) return 1;

    switch (msg) {
    case WM_NCHITTEST: {
        POINT p{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        ScreenToClient(w, &p);
        bool title = p.y >= 0 && p.y < ui::titleHeight * scale && p.x < (ui::width - ui::controlsWidth) * scale;
        return title ? HTCAPTION : HTCLIENT;
    }
    case WM_NCCALCSIZE:
        if (wp) return 0;
        break;
    case WM_DPICHANGED: {
        scale = float(HIWORD(wp)) / 96.f;
        auto* r = reinterpret_cast<RECT*>(lp);
        SetWindowPos(w, nullptr, r->left, r->top, int(ui::width * scale), int(ui::height * scale), SWP_NOZORDER | SWP_NOACTIVATE);
        resize(int(ui::width * scale), int(ui::height * scale));
        return 0;
    }
    case WM_CLOSE:
        if (!app::canClose()) return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(w, msg, wp, lp);
}

void roundCorners() {
    int preference = 2;
    DwmSetWindowAttribute(window, 33, &preference, sizeof(preference));
    MARGINS m{1, 1, 1, 1};
    DwmExtendFrameIntoClientArea(window, &m);
}

}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR args, int) {
    if (wcsstr(args, L"--extract")) {
        std::string error;
        return embedded::install(error) ? 0 : 1;
    }
    if (auto wait = wcsstr(args, L"--wait-for ")) {
        DWORD pid = wcstoul(wait + 11, nullptr, 10);
        if (pid && pid != GetCurrentProcessId()) {
            HANDLE previous = OpenProcess(SYNCHRONIZE, FALSE, pid);
            if (previous) {
                DWORD result = WaitForSingleObject(previous, 30000);
                CloseHandle(previous);
                if (result != WAIT_OBJECT_0) return 1;
            }
        }
    }
    HANDLE once = CreateMutexW(nullptr, TRUE, L"Monchi.Launcher");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (HWND other = FindWindowW(L"MonchiLauncher", nullptr)) {
            ShowWindow(other, SW_RESTORE);
            SetForegroundWindow(other);
        }
        return 0;
    }
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    WNDCLASSEXW wc{sizeof(wc)};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(IDI_APP));
    wc.lpszClassName = L"MonchiLauncher";
    RegisterClassExW(&wc);

    UINT dpi = 96;
    if (HDC dc = GetDC(nullptr)) {
        dpi = GetDeviceCaps(dc, LOGPIXELSX);
        ReleaseDC(nullptr, dc);
    }
    scale = float(dpi) / 96.f;
    int w = int(ui::width * scale), h = int(ui::height * scale);
    int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2, y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

    window = CreateWindowExW(WS_EX_APPWINDOW, wc.lpszClassName, L"Monchi", WS_POPUP | WS_MINIMIZEBOX | WS_SYSMENU, x, y, w,
                             h, nullptr, nullptr, inst, nullptr);
    if (!window || !createDevice()) {
        MessageBoxW(nullptr, L"Could not start DirectX 11.", L"Monchi", MB_ICONERROR);
        return 1;
    }
    roundCorners();
    if (UINT real = GetDpiForWindow(window); real != dpi) {
        scale = float(real) / 96.f;
        SetWindowPos(window, nullptr, 0, 0, int(ui::width * scale), int(ui::height * scale), SWP_NOMOVE | SWP_NOZORDER);
        resize(int(ui::width * scale), int(ui::height * scale));
    }
    ShowWindow(window, SW_SHOW);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    ImFont* regular = loadFont(IDR_FONT_REGULAR);
    ImFont* bold = loadFont(IDR_FONT_BOLD);
    if (!regular) regular = io.Fonts->AddFontDefault();
    ui::setFonts(regular, bold);
    ImGui_ImplWin32_Init(window);
    ImGui_ImplDX11_Init(device, context);
    look::init(device, context);

    ui::State state;
    state.changelog = "";
    app::init(state);
    if (wcsstr(args, L"--cosmetics")) state.page = ui::Page::Cosmetics;
    if (wcsstr(args, L"--versions")) {
        state.page = ui::Page::Versions;
        state.downloadsOpen = true;
        ui::Events events;
        events.loadVersions = true;
        app::handle(state, events, window);
    }

    bool running = true;
    while (running) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) running = false;
        }
        if (!running) break;
        if (app::wantsQuit()) PostMessageW(window, WM_CLOSE, 0, 0);
        if (IsIconic(window)) {
            Sleep(60);
            continue;
        }

        app::sync(state);
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        io.DisplaySize = {ui::width, ui::height};
        io.DisplayFramebufferScale = {scale, scale};
        ImGui::NewFrame();

        ui::Events events;
        ui::draw(state, events);
        app::handle(state, events, window);

        ImGui::Render();
        const float clear[4] = {28.f / 255.f, 29.f / 255.f, 33.f / 255.f, 1.f};
        context->OMSetRenderTargets(1, &target, nullptr);
        context->ClearRenderTargetView(target, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        swap->Present(1, 0);
    }

    look::stop();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    dropTarget();
    swap->Release();
    context->Release();
    device->Release();
    CloseHandle(once);
    return 0;
}
