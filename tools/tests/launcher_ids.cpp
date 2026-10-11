#include "Ui.hpp"
#include <imgui_internal.h>
namespace look { extern int previewCalls; }

int main() {
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.DisplaySize = {ui::width, ui::height};
    io.DeltaTime = 1.f / 60.f;
    io.IniFilename = nullptr;
    auto* font = io.Fonts->AddFontDefault();
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    ui::setFonts(font, font);
    ui::State state;
    state.page = ui::Page::Start;
    ImGui::NewFrame();
    ui::Events home;
    ui::draw(state, home);
    ImGui::Render();
    if (look::previewCalls != 0) return 7;
    state.page = ui::Page::Settings;
    for (int frame = 0; frame < 60; frame++) {
        io.MousePos = {880.f, 375.f};
        ImGui::NewFrame();
        ui::Events events;
        ui::draw(state, events);
        ImGui::Render();
        if (GImGui->HoveredIdPreviousFrameItemCount > 1) return 1;
    }
    state.page = ui::Page::Cosmetics;
    for (int frame = 0; frame < 60; frame++) {
        io.MousePos = {880.f, 150.f};
        ImGui::NewFrame();
        ui::Events events;
        ui::draw(state, events);
        ImGui::Render();
        if (GImGui->HoveredIdPreviousFrameItemCount > 1) return 1;
    }
    if (look::previewCalls == 0) return 8;
    state.page = ui::Page::Cosmetics;
    auto clickFigure = [&](float x, int expected) {
        io.AddMousePosEvent(x, 155.f);
        for (int step = 0; step < 3; ++step) {
            if (step == 1) io.AddMouseButtonEvent(0, true);
            if (step == 2) io.AddMouseButtonEvent(0, false);
            ImGui::NewFrame();
            ui::Events events;
            ui::draw(state, events);
            ImGui::Render();
            if (step == 2 && (!events.settingsChanged || state.settings.figure != expected)) return false;
        }
        return true;
    };
    state.settings.figure = 1;
    if (!clickFigure(300.f, 0) || !clickFigure(375.f, 1)) return 6;
    state.page = ui::Page::Versions;
    state.downloadsOpen = true;
    state.downloads = {{"1.26.52", false, false, true}, {"1.26.51", false, false, false}};
    for (int frame = 0; frame < 60; frame++) {
        io.MousePos = {820.f, 300.f};
        ImGui::NewFrame();
        ui::Events events;
        ui::draw(state, events);
        ImGui::Render();
        if (GImGui->HoveredIdPreviousFrameItemCount > 1) return 2;
    }
    state.updatePrompt = true;
    state.latestVersion = "0.2.0";
    ImGui::NewFrame();
    ui::Events offer;
    ui::draw(state, offer);
    if (!ImGui::IsPopupOpen("updateOffer", ImGuiPopupFlags_AnyPopupId)) return 3;
    ImGui::Render();
    io.AddKeyEvent(ImGuiKey_Escape, true);
    ImGui::NewFrame();
    ui::Events dismissed;
    ui::draw(state, dismissed);
    ImGui::Render();
    if (!dismissed.dismissUpdate || dismissed.update || state.updatePrompt) return 4;
    io.AddKeyEvent(ImGuiKey_Escape, false);
    ImGui::NewFrame();
    ui::Events idle;
    ui::draw(state, idle);
    ImGui::Render();
    if (ImGui::IsPopupOpen("updateOffer", ImGuiPopupFlags_AnyPopupId)) return 5;
    ImGui::DestroyContext();
    return 0;
}
