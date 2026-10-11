#include "FlarialLink.hpp"
#include "Guard.hpp"
#include "Log.hpp"
#include "Paths.hpp"
#include "flarial/Bridge/Bridge.hpp"
#include "flarial/Bridge/Modules.hpp"

#include <imgui.h>
#include <chrono>

namespace flarialLink {

namespace {

HMODULE core = nullptr;
MonchiFlarialFrame drawCore = nullptr;
MonchiFlarialStop stopCore = nullptr;
MonchiFlarialEjectRequested ejectCore = nullptr;
bool tried = false;

template <class T>
T get(const char* name) {
    return reinterpret_cast<T>(GetProcAddress(core, name));
}

// The core installs game hooks and adds its fonts to ImGui when it starts, so it starts on the render thread right
// before the first frame that has an ImGui context.
void start() {
    tried = true;
    auto startAt = std::chrono::steady_clock::now();
    auto file = paths::dllDir() / L"MonchiFlarial.dll";
    core = LoadLibraryW(file.c_str());
    auto loadedAt = std::chrono::steady_clock::now();
    if (!core) {
        logger::info("flarial core not found, running on monchi's modules only");
        return;
    }
    auto begin = get<MonchiFlarialStart>("monchiFlarialStart");
    auto abi = get<MonchiFlarialAbi>("monchiFlarialAbi");
    drawCore = get<MonchiFlarialFrame>("monchiFlarialFrame");
    stopCore = get<MonchiFlarialStop>("monchiFlarialStop");
    ejectCore = get<MonchiFlarialEjectRequested>("monchiFlarialEjectRequested");
    MonchiFlarialImGui imgui{ImGui::GetCurrentContext()};
    ImGui::GetAllocatorFunctions(&imgui.alloc, &imgui.free, &imgui.user);
    bool started = false;
    if (begin && drawCore && stopCore && ejectCore && abi)
        guard::call("flarial start", [&] { if (abi() == 1) started = begin(&imgui); });
    auto readyAt = std::chrono::steady_clock::now();
    logger::info("native startup: load {:.1f} ms, initialize {:.1f} ms",
        std::chrono::duration<double, std::milli>(loadedAt - startAt).count(),
        std::chrono::duration<double, std::milli>(readyAt - loadedAt).count());
    if (started) {
        logger::info("flarial core started");
        return;
    }
    logger::warn("flarial core did not start");
    if (auto error = get<MonchiFlarialStartError>("monchiFlarialStartError"))
        guard::call("flarial start error", [&] { logger::warn("flarial core: {}", error()); });
    drawCore = nullptr;
    // a start that failed half way may already have hooks in the game; they are taken out before anything else
    stop();
}

}

void frame(ID3D11Device* device, ID3D11DeviceContext* context, IDXGISwapChain* swapchain) {
    if (!tried) start();
    if (drawCore && !guard::call("flarial frame", [&] { drawCore(device, context, swapchain); })) {
        drawCore = nullptr;
        logger::warn("native drawing stopped after a frame fault; restart Minecraft before testing again");
    }
}

bool stop() {
    if (!core) return true;
    drawCore = nullptr;
    bool clean = false;
    bool guarded = stopCore && guard::call("flarial stop", [&] { clean = stopCore(); });
    // only a confirmed clean stop may free the code; otherwise the handle is kept and stop() can be tried again
    if (!guarded || !clean) {
        logger::warn("flarial core did not stop cleanly, it stays loaded");
        return false;
    }
    if (!FreeLibrary(core)) {
        logger::warn("flarial core release failed, keeping the module handle");
        return false;
    }
    core = nullptr;
    return true;
}

bool ejectRequested() {
    if (!drawCore || !ejectCore) return false;
    bool requested = false;
    guard::call("flarial eject request", [&] { requested = ejectCore(); });
    return requested;
}

}
