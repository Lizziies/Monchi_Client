#pragma once

#include <imgui.h>

namespace post {

struct Backdrop {
    ImDrawList* list = nullptr;
    int first = 0;

    void reserve(ImDrawList* draw, ImDrawCallback callback, ImDrawCallback reset) {
        list = draw;
        draw->AddCallback(callback, nullptr);
        first = draw->CmdBuffer.Size - 2;
        draw->AddCallback(reset, nullptr);
    }

    void finish(bool needed) {
        if (!list) return;
        if (!needed) {
            auto& commands = list->CmdBuffer;
            commands.erase(commands.Data + first, commands.Data + first + 2);
        }
        list = nullptr;
    }
};

}
