#include "render/GameText.hpp"
#include <imgui.h>
#include <cstdlib>

int main() {
    ImGui::CreateContext();
    auto* font = ImGui::GetIO().Fonts->AddFontDefault();
    if (!gameText::canDraw(font, "Zeqa Network")) std::abort();
    if (!gameText::canDraw(font, "\xC2\xA7" "eZeqa\nNetwork")) std::abort();
    if (gameText::canDraw(font, "\xEE\x80\x80")) std::abort();
    if (gameText::canDraw(nullptr, "Zeqa")) std::abort();
    ImGui::DestroyContext();
}
