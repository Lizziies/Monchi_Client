#pragma once

#include "gui/Notify.hpp"
#include "gui/Theme.hpp"
#include "modules/Module.hpp"
#include "modules/common/Options.hpp"

#include <json.hpp>

#include <filesystem>
#include <fstream>
#include <vector>

class PackChanger : public Module {
public:
    PackChanger()
        : Module("Pack Changer", "Lists your resource packs and writes the active list for the next game start, so you do not need the slow pack menu.",
                 Category::Comfort, {"cosmetic"}) {
        sub("Packs");
    }

    void onEnable() override { scan(); }

    void drawSettings() override {
        if (!scanned_) scan();
        auto& t = theme::current();
        ImGui::Spacing();
        if (ImGui::SmallButton(i18n::tr("Scan again"))) scan();
        if (packs_.empty()) {
            ImGui::TextDisabled("%s", i18n::tr("No resource packs found."));
            return;
        }
        int up = -1;
        for (size_t i = 0; i < packs_.size(); i++) {
            auto& p = packs_[i];
            ImGui::PushID(int(i));
            ImGui::Checkbox("##on", &p.active);
            ImGui::SameLine();
            ImGui::TextColored(p.active ? t.text : t.textDim, "%s", p.name.c_str());
            if (p.active && i > 0) {
                ImGui::SameLine();
                if (ImGui::SmallButton(i18n::tr("Up"))) up = int(i);
            }
            ImGui::PopID();
        }
        if (up > 0) std::swap(packs_[size_t(up)], packs_[size_t(up) - 1]);
        if (ImGui::Button(i18n::tr("Save the active list"))) save();
        ImGui::TextDisabled("%s", i18n::tr("The game reads the list when it starts. Restart it to apply."));
    }

private:
    struct Pack {
        std::string name;
        std::string uuid;
        std::vector<int> version;
        bool active = false;
    };

    std::filesystem::path root() const { return mcopt::newest(L"resource_packs"); }

    std::filesystem::path activeFile() const { return mcopt::newest(L"minecraftpe\\global_resource_packs.json"); }

    void scan() {
        scanned_ = true;
        packs_.clear();
        std::error_code ec;
        auto dir = root();
        auto active = nlohmann::json::array();
        if (std::ifstream in(activeFile()); in) {
            auto j = nlohmann::json::parse(in, nullptr, false);
            if (j.is_array()) active = j;
        }
        for (auto& e : std::filesystem::directory_iterator(dir, ec)) {
            std::ifstream in(e.path() / "manifest.json");
            if (!in) continue;
            auto j = nlohmann::json::parse(in, nullptr, false);
            if (j.is_discarded() || !j.contains("header")) continue;
            Pack p;
            p.name = j["header"].value("name", e.path().filename().string());
            p.uuid = j["header"].value("uuid", "");
            if (j["header"].contains("version") && j["header"]["version"].is_array())
                for (auto& v : j["header"]["version"]) p.version.push_back(v.get<int>());
            for (auto& a : active)
                if (a.value("pack_id", "") == p.uuid) p.active = true;
            packs_.push_back(std::move(p));
        }
        std::stable_sort(packs_.begin(), packs_.end(), [](const Pack& a, const Pack& b) { return a.active > b.active; });
    }

    void save() {
        auto file = activeFile();
        if (file.empty()) return;
        std::error_code ec;
        std::filesystem::copy_file(file, std::filesystem::path(file).replace_extension(".json.bak"), std::filesystem::copy_options::overwrite_existing, ec);
        auto out = nlohmann::json::array();
        for (auto& p : packs_)
            if (p.active) out.push_back({{"pack_id", p.uuid}, {"version", p.version}});
        std::ofstream(file, std::ios::trunc) << out.dump(2);
        notify::push(i18n::tr("Pack Changer"), i18n::tr("Saved. Restart the game to apply."), notify::Kind::Ok);
    }

    std::vector<Pack> packs_;
    bool scanned_ = false;
};
