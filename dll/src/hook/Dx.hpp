#pragma once

#include <windows.h>

struct IDXGISwapChain;

#include <cstdint>

namespace dx {

enum class Api { None, Dx11, Dx12 };

struct Tuning {
    bool allowTearing = false;
    bool syncToDisplay = false;
    bool lowLatency = false;
    float fpsLimit = 0.f;
    // no two frames reach the display closer together than one frame at this rate
    float presentFloor = 0.f;
    bool efficientOverlay = true;
};

struct FrameInfo {
    int64_t presentQpc = 0;
    double frameMs = 0;
    double overlayMs = 0;
    unsigned overlayFlushes = 0;
    bool tearingSupported = false;
    bool lowLatencyActive = false;
    int bufferCount = 0;
    unsigned nativeSync = 0, presentSync = 0, presentFlags = 0;
    unsigned heldPresents = 0;
};

bool install();
void unhookTables();
bool uninstall();

Api api();
HWND window();
IDXGISwapChain* swapchain();
Tuning& tuning();
const FrameInfo& frame();
// a frame of the game is inside Monchi's part of Present right now
bool busy();

}
