#pragma once

#include <windows.h>

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace ui {

bool init(HWND window, ID3D11Device* device, ID3D11DeviceContext* context);
void frame();
bool shutdown();
void invalidate();

bool wndProc(HWND w, UINT msg, WPARAM wp, LPARAM lp);
void mouseButton(int button, bool down);
void mouseWheel(float delta);
bool wantsCursor();
bool capturing();

float scale();
float dt();
double time();

}
