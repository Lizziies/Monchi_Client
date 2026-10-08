#pragma once

#include <windows.h>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct IDXGISwapChain;
struct ImGuiContext;

// The interface of MonchiFlarial.dll, the Flarial core. Monchi owns the window hook, the swapchain hook, ImGui
// and the menu; the core starts Flarial's game hooks and modules and draws them into Monchi's frame. Plain C
// functions, because the two dlls must not share any C++ type (both have their own Module, KeyEvent, ...).
struct MonchiFlarialImGui {
    ImGuiContext* context;
    void* (*alloc)(size_t size, void* user);
    void (*free)(void* ptr, void* user);
    void* user;
};

extern "C" {
using MonchiFlarialStart = bool (*)(const MonchiFlarialImGui* imgui);
using MonchiFlarialStartError = const char* (*)();
using MonchiFlarialFrame = void (*)(ID3D11Device* device, ID3D11DeviceContext* context, IDXGISwapChain* swapchain);
// false while work of the core may still run; the dll must then stay loaded
using MonchiFlarialStop = bool (*)();
// Flarial's ".eject" command asks for an unload; Monchi then unloads the whole client
using MonchiFlarialEjectRequested = bool (*)();
}
