#pragma once

#include <imgui.h>

#include <string>
#include <vector>

namespace post {

struct Params {
    float saturation = 1.f;
    float hue = 0.f;
    float brightness = 0.f;
    float contrast = 1.f;
    float gamma = 1.f;
    float sharpen = 0.f;
    float fry = 0.f;
    float flip = 0.f;
    float tint[4] = {0.f, 0.f, 0.f, 0.f};
    int tintMode = 0;
    float night[4] = {1.f, 1.f, 1.f, 0.f};
    float vignette = 0.f;
    int colorMode = 0;
    float dof = 0.f;
    float dofSharp = 0.2f;
    float dofEdge = 0.9f;
    float dofFocus = 0.5f;
    bool dofBand = false;
    float dofReach = 0.03f;
    int dofRings = 1;
    float paint = 0.f;
    float dir[2] = {0.f, 0.f};
    int dirSamples = 0;
    float blend = 0.f;
    float blur = 0.f;
    int shader = -1;
    float shaderMix = 1.f;
    float zoom = 1.f;

    bool active() const;
    bool basic() const;
};

struct ShaderInfo {
    std::string name;
    std::string source;
    bool builtin = false;
};

void blur(ImDrawList* dl, ImVec2 min, ImVec2 max, float rounding, float radius, ImVec4 tint, float angle = 0.f);
const std::vector<ShaderInfo>& shaders();
void reloadShaders();
std::string shaderError(int index);

// The game's picture as it was before Monchi drew on it, put into a draw list without blending: the settings preview
// shows it small. Asking keeps the copy coming; the first frame has none yet and gets a dark fill.
void frameImage(ImDrawList* dl, ImVec2 min, ImVec2 max, ImVec2 uv0, ImVec2 uv1);

Params& params();
void begin();
void submit(ImDrawList* dl);
void finish();
void shutdown();
void releaseCompiled();

}
