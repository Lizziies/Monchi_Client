#include "Fonts.hpp"
#include "core/Client.hpp"
#include "core/Log.hpp"
#include "Ui.hpp"

#include "../../res/resource.h"

#include <windows.h>

#include <filesystem>

namespace fonts {

static ImFont* reg = nullptr;
static ImFont* bld = nullptr;
static const ImWchar ranges[] = {0x20, 0x52F, 0x1D00, 0x1EFF, 0x2000, 0x206F, 0x2190, 0x21FF, 0x2500, 0x27BF, 0};

static ImFont* fromResource(int id, float size) {
    HMODULE self = client::module();
    HRSRC res = FindResourceW(self, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(10));
    if (!res) return nullptr;
    HGLOBAL data = LoadResource(self, res);
    void* bytes = data ? LockResource(data) : nullptr;
    int len = (int)SizeofResource(self, res);
    if (!bytes || !len) return nullptr;

    ImFontConfig cfg;
    cfg.FontDataOwnedByAtlas = false;
    cfg.OversampleH = 2;
    return ImGui::GetIO().Fonts->AddFontFromMemoryTTF(bytes, len, size, &cfg, ranges);
}

static void symbols(ImFont* font) {
    wchar_t windows[MAX_PATH]{};
    if (!GetWindowsDirectoryW(windows, MAX_PATH)) return;
    for (const wchar_t* name : {L"segoeui.ttf", L"seguisym.ttf"}) {
        auto file = std::filesystem::path(windows) / L"Fonts" / name;
        std::error_code ec;
        if (!std::filesystem::exists(file, ec)) continue;
        ImFontConfig cfg;
        cfg.MergeMode = true;
        cfg.DstFont = font;
        ImGui::GetIO().Fonts->AddFontFromFileTTF(file.string().c_str(), 16.f, &cfg, ranges);
    }
}

void load() {
    auto& io = ImGui::GetIO();
    reg = fromResource(IDR_FONT_REGULAR, 16.f);
    if (reg) symbols(reg);
    bld = fromResource(IDR_FONT_BOLD, 16.f);
    if (bld) symbols(bld);
    if (!reg) {
        logger::warn("embedded font missing, falling back to default");
        reg = io.Fonts->AddFontDefault();
    }
    if (!bld) bld = reg;
    io.FontDefault = reg;
}

bool covers(unsigned code) {
    for (const ImWchar* r = ranges; r[0]; r += 2)
        if (code >= r[0] && code <= r[1]) return true;
    return code == '\n' || code == '\r' || code == '\t';
}

ImFont* regular() { return reg; }
ImFont* bold() { return bld; }
ImFont* hud() { return bld; }
float hudSize() { return 18.f * ui::scale(); }

}
