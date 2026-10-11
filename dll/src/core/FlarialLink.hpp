#pragma once

struct ID3D11Device;
struct ID3D11DeviceContext;
struct IDXGISwapChain;

// Loads MonchiFlarial.dll (the Flarial core) from the client's folder when it is there and drives it from the
// render thread. Without the file Monchi runs on its own modules only.
namespace flarialLink {

void frame(ID3D11Device* device, ID3D11DeviceContext* context, IDXGISwapChain* swapchain);
// true once the core is gone; false keeps it loaded (and with it ImGui, which it still uses)
bool stop();
bool ejectRequested();

}
