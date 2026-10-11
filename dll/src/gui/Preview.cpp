#include "Preview.hpp"

#include "I18n.hpp"
#include "Theme.hpp"
#include "modules/HudModule.hpp"
#include "modules/Module.hpp"
#include "modules/post/PostFx.hpp"
#include "render/Fonts.hpp"
#include "render/Ui.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>

namespace gui::preview {

namespace {

int cmdMark = 0, idxMark = 0;
bool open = true;
ImVec2 grab{0, 0};

// The HUD of this frame is already in the background draw list. Its triangles are copied into the window's list,
// moved and scaled; shader callbacks (blur) are left out.
void replay(ImDrawList* dl, ImVec2 srcMin, float scale, ImVec2 dstMin, ImVec2 dstMax) {
    ImDrawList* hud = ImGui::GetBackgroundDrawList();
    int cmds = std::min(cmdMark, hud->CmdBuffer.Size);
    for (int c = 0; c < cmds; c++) {
        const ImDrawCmd& cmd = hud->CmdBuffer[c];
        if (cmd.UserCallback) continue;
        int count = std::min(int(cmd.ElemCount), idxMark - int(cmd.IdxOffset));
        count -= count % 3;
        if (count <= 0 || int(cmd.IdxOffset) + count > hud->IdxBuffer.Size) continue;

        ImVec2 clipMin = dstMin + (ImVec2(cmd.ClipRect.x, cmd.ClipRect.y) - srcMin) * scale;
        ImVec2 clipMax = dstMin + (ImVec2(cmd.ClipRect.z, cmd.ClipRect.w) - srcMin) * scale;
        clipMin = ImMax(clipMin, dstMin);
        clipMax = ImMin(clipMax, dstMax);
        if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y) continue;

        dl->PushClipRect(clipMin, clipMax, true);
        dl->PushTexture(cmd.TexRef);
        const ImDrawIdx* idx = hud->IdxBuffer.Data + cmd.IdxOffset;
        // in batches, so one reserve never passes what a 16-bit index can address
        for (int done = 0; done < count;) {
            int n = std::min(count - done, 30000);
            dl->PrimReserve(n, n);
            for (int i = 0; i < n; i++) {
                int at = int(cmd.VtxOffset) + int(idx[done + i]);
                if (at >= hud->VtxBuffer.Size) at = hud->VtxBuffer.Size - 1;
                const ImDrawVert& v = hud->VtxBuffer.Data[at];
                dl->PrimWriteIdx(ImDrawIdx(dl->_VtxCurrentIdx));
                dl->PrimWriteVtx(dstMin + (v.pos - srcMin) * scale, v.uv, v.col);
            }
            done += n;
        }
        dl->PopTexture();
        dl->PopClipRect();
    }
}

}

void mark() {
    ImDrawList* hud = ImGui::GetBackgroundDrawList();
    cmdMark = hud->CmdBuffer.Size;
    idxMark = hud->IdxBuffer.Size;
}

void draw(Module& m) {
    ImVec2 lo, hi;
    auto kind = m.preview(lo, hi);
    if (kind == Module::Preview::None) return;
    auto& t = theme::current();
    float s = ui::scale();
    ImVec2 ds = ImGui::GetIO().DisplaySize;
    if (ds.x < 8.f || ds.y < 8.f) return;

    ImGui::PushFont(fonts::bold(), 11.f);
    ImGui::PushStyleColor(ImGuiCol_Text, theme::col(t.textDim, 0.85f));
    ImGui::TextUnformatted(i18n::tr(open ? "Live preview (click to hide)" : "Live preview (click to show)"));
    ImGui::PopStyleColor();
    ImGui::PopFont();
    if (ImGui::IsItemClicked()) open = !open;
    if (!open) return;

    float w = ImGui::GetContentRegionAvail().x;
    ImVec2 srcMin{0, 0}, srcMax = ds;
    float h = w * ds.y / ds.x;
    if (kind == Module::Preview::Zoomed && hi.x > lo.x && hi.y > lo.y) {
        // the area keeps its shape and fills the width
        h = w * (hi.y - lo.y) / (hi.x - lo.x);
        srcMin = lo;
        srcMax = hi;
    }
    float scale = w / (srcMax.x - srcMin.x);

    ImVec2 at = ImGui::GetCursorScreenPos(), end = at + ImVec2(w, h);
    ImGui::InvisibleButton("##preview", {w, h});
    bool held = ImGui::IsItemActive(), pressed = ImGui::IsItemActivated();
    auto* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(at, end, true);
    post::frameImage(dl, at, end, {srcMin.x / ds.x, srcMin.y / ds.y}, {srcMax.x / ds.x, srcMax.y / ds.y});
    replay(dl, srcMin, scale, at, end);

    auto toPreview = [&](ImVec2 p) { return at + (p - srcMin) * scale; };
    auto toScreen = [&](ImVec2 p) { return srcMin + (p - at) / scale; };
    if (kind == Module::Preview::Marked && m.isHud()) {
        auto& hudModule = static_cast<HudModule&>(m);
        // dragging in the picture moves the real thing; a click beside it brings it under the cursor
        if (held) {
            ImVec2 mouse = toScreen(ImGui::GetIO().MousePos);
            if (pressed) {
                bool inside = mouse.x >= lo.x && mouse.y >= lo.y && mouse.x <= hi.x && mouse.y <= hi.y;
                grab = inside ? mouse - lo : (hi - lo) * 0.5f;
            }
            hudModule.setPosition(mouse - grab);
            hudModule.preview(lo, hi);
        }
        if (hi.x > lo.x && hi.y > lo.y)
            dl->AddRect(toPreview(lo) - ImVec2(2, 2), toPreview(hi) + ImVec2(2, 2), theme::col(t.accent), 3.f * s, 0, 1.5f * s);
    }
    if (!m.enabled()) {
        const char* note = i18n::tr("Turn the module on to see it here.");
        ImVec2 size = fonts::regular()->CalcTextSizeA(13.f * s, FLT_MAX, 0.f, note);
        ImVec2 p = at + (ImVec2(w, h) - size) * 0.5f;
        dl->AddRectFilled(p - ImVec2(8, 5) * s, p + size + ImVec2(8, 5) * s, IM_COL32(0, 0, 0, 170), 5.f * s);
        dl->AddText(fonts::regular(), 13.f * s, p, IM_COL32(255, 255, 255, 235), note);
    }
    dl->PopClipRect();
    dl->AddRect(at, end, theme::col(t.border), 0.f, 0, 1.f);
    if (kind == Module::Preview::Marked && m.isHud() && m.enabled()) {
        ImGui::PushFont(fonts::regular(), 11.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, theme::col(t.textDim, 0.8f));
        ImGui::TextUnformatted(i18n::tr("Drag it in the picture to move it."));
        ImGui::PopStyleColor();
        ImGui::PopFont();
    }
    ImGui::Dummy({0, 4.f * s});
}

}
