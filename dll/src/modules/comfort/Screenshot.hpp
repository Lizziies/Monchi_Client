#pragma once

#include "core/Log.hpp"
#include "gui/Notify.hpp"
#include "hook/Dx.hpp"
#include "modules/Module.hpp"
#include "modules/post/Capture.hpp"
#include "render/Ui.hpp"

#include <windows.h>
#include <shellapi.h>

#include <format>

class Screenshot : public Module {
public:
    Screenshot()
        : Module("Screenshot+", "Screenshot on a key, with or without the Monchi HUD, as PNG or JPEG.",
                 Category::Comfort, {"cosmetic"}) {
        sub("Capture");
        quality_.visible = [this] { return format_.i == 1; };
        folder_.visible = [this] { return file_.b; };
        name_.visible = [this] { return file_.b; };
    }

    bool defaultEnabled() const override { return false; }

    void onKey(KeyEvent& ev) override {
        int key = inputKey_.load();
        if (!ev.down || ev.repeat || ev.vk != key || !key) return;
        requested_ = true;
    }
    void onEnable() override { inputKey_ = key_.i; }
    void onDisable() override { requested_ = false; armed_ = false; }

    void onFrame() override {
        inputKey_ = key_.i;
        if (requested_.exchange(false)) { due_ = ui::time() + delay_.f; armed_ = true; }
        if (armed_ && ui::time() >= due_) {
            if (capture::request(stage_.i == 0 ? capture::Stage::Overlay : capture::Stage::Game)) armed_ = false;
        }

        capture::Image img;
        if (capture::poll(img)) finish(std::move(img));

        std::filesystem::path path;
        bool ok = false;
        if (capture::takeSaved(path, ok)) {
            if (!toast_.b) return;
            if (ok) notify::push(i18n::tr("Screenshot saved"), path.filename().string(), notify::Kind::Ok);
            else notify::push(i18n::tr("Screenshot failed"), i18n::tr("The file could not be written."), notify::Kind::Error);
        }
    }

    void drawSettings() override {
        ImGui::Spacing();
        if (ImGui::SmallButton(i18n::tr("Capture now"))) {
            due_ = ui::time() + delay_.f;
            armed_ = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton(i18n::tr("Open folder"))) {
            auto dir = folder().wstring();
            std::error_code ec;
            std::filesystem::create_directories(folder(), ec);
            ShellExecuteW(nullptr, L"open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
        ImGui::TextDisabled(i18n::tr("Folder: %s"), folder().string().c_str());
    }

private:
    std::filesystem::path folder() const {
        if (!folder_.text.empty()) return std::filesystem::path(logger::widen(folder_.text));
        wchar_t home[MAX_PATH]{};
        GetEnvironmentVariableW(L"USERPROFILE", home, MAX_PATH);
        return std::filesystem::path(home) / L"Pictures" / L"Monchi";
    }

    void finish(capture::Image img) {
        if (clipboard_.b) capture::copyToClipboard(img, dx::window());
        if (!file_.b) {
            if (toast_.b) notify::push(i18n::tr("Screenshot copied"), i18n::tr("On the clipboard."), notify::Kind::Ok);
            return;
        }
        SYSTEMTIME t;
        GetLocalTime(&t);
        std::string stamp = std::format("{:04}-{:02}-{:02}_{:02}-{:02}-{:02}", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
        std::string base = name_.text.empty() ? "Monchi" : name_.text;
        const char* ext = format_.i == 1 ? ".jpg" : ".png";
        capture::save(std::move(img), folder() / std::filesystem::path(base + "_" + stamp + ext),
                      format_.i == 1 ? capture::Format::Jpeg : capture::Format::Png, quality_.i);
    }

    Setting& key_ = keySetting("shot", "Capture key", VK_F9);
    Setting& stage_ = choice("stage", "Content", {"With Monchi HUD", "Without Monchi HUD"});
    Setting& format_ = choice("format", "Format", {"PNG", "JPEG"});
    Setting& quality_ = intSlider("quality", "JPEG quality", 92, 50, 100);
    Setting& delay_ = slider("delay", "Delay (s)", 0.f, 0.f, 10.f, "%.1f s");
    Setting& file_ = toggleSetting("file", "Save as file", true);
    Setting& clipboard_ = toggleSetting("clipboard", "Copy to the clipboard", false);
    Setting& toast_ = toggleSetting("toast", "Show notice", true);
    Setting& folder_ = textSetting("folder", "Folder (empty = Pictures\\Monchi)", "");
    Setting& name_ = textSetting("name", "File name", "Monchi");
    double due_ = 0.0;
    bool armed_ = false;
    std::atomic<int> inputKey_{0};
    std::atomic<bool> requested_{false};
};
