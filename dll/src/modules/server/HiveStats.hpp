#pragma once

#include "gui/Gui.hpp"
#include "gui/Theme.hpp"
#include "modules/HudModule.hpp"
#include "modules/common/Colors.hpp"
#include "modules/common/GameHud.hpp"
#include "modules/common/Needs.hpp"
#include "modules/common/Nick.hpp"
#include "modules/common/Text.hpp"
#include "modules/common/FoldLess.hpp"
#include "modules/server/HiveApi.hpp"
#include "modules/server/ServerChat.hpp"
#include "render/Fonts.hpp"

#include <algorithm>
#include <ctime>
#include <format>
#include <functional>

inline ImVec2 anchorPivot(int anchor) { return {float(anchor % 3) * 0.5f, float(anchor / 3) * 0.5f}; }

inline std::string shortNumber(double v) {
    if (v >= 1e6) return std::format("{:.1f}M", v / 1e6);
    if (v >= 1e4) return std::format("{:.1f}k", v / 1e3);
    return std::format("{:.0f}", v);
}

class HiveStats : public HudModule {
public:
    HiveStats()
        : HudModule("Hive Stats", "Shows the stats of the players in your lobby from the public Hive API. Only on The Hive.", {"info-others"},
                    {0.75f, 0.6f}) {
        sub("The Hive");
        moveTo(Category::Server);
        require(need::tab, need::sigs({"TabListData"}));
        mid_.visible = [this] { return thresholds_.b; };
        high_.visible = [this] { return thresholds_.b; };
        kdMid_.visible = [this] { return thresholds_.b; };
        kdHigh_.visible = [this] { return thresholds_.b; };
        fkdrMid_.visible = [this] { return thresholds_.b; };
        fkdrHigh_.visible = [this] { return thresholds_.b; };
        winMid_.visible = [this] { return thresholds_.b; };
        winHigh_.visible = [this] { return thresholds_.b; };
        highlightWords_.visible = [this] { return highlight_.b; };
        highlightColor_.visible = [this] { return highlight_.b; };
        overlayHold_.visible = [this] { return overlayKey_.i != 0; };
    }

    void onKey(KeyEvent& ev) override {
        if (!overlayKey_.i || ev.vk != overlayKey_.i || ev.repeat) return;
        if (overlayHold_.b) shown_ = ev.down;
        else if (ev.down) shown_ = !shown_;
    }

    void onRender(ImDrawList* dl) override {
        bool editing = gui::editingHud();
        if (!here() && !editing) return;
        if (!editing && overlayKey_.i && !shown_) return;
        if (!editing && !hubs_.b && game_.i == 0 && hive::gameFromTitle(text::lower(game::state().scoreboard.title)) < 0) return;
        HudModule::onRender(dl);
    }

    void drawSettings() override {
        auto& t = theme::current();
        ImGui::Spacing();
        if (!here()) ImGui::TextColored(t.warn, i18n::tr("Not on The Hive. The overlay only shows there."));
        else ImGui::TextDisabled("%s", i18n::fmt("Game: {}", hive::gameName(gameIndex())).c_str());
        if (game::demo()) ImGui::TextDisabled("%s", i18n::tr("Demo data: the numbers are made up, the Hive API is not called."));
    }

protected:
    ImVec2 pivot() const override { return anchorPivot(anchor_.i); }

    ImVec2 content(ImDrawList* dl, ImVec2 o, float s) override {
        int gi = gameIndex();
        auto names = players();
        float lineH = fonts::hudSize() * s * 1.2f;

        std::vector<std::string> heads{i18n::tr("Player")};
        auto cols = columns(gi);
        for (auto& c : cols) heads.push_back(i18n::tr(c.title));

        std::vector<Entry> entries;
        for (auto& n : names) {
            Entry e;
            e.name = n;
            e.player = game::demo() ? hive::Player{hive::Load::Ready, hive::sample(n)} : hive::player(gi, n, cache_.f * 60.f);
            entries.push_back(std::move(e));
        }
        if (order_.i == 1)
            std::stable_sort(entries.begin(), entries.end(), [&](const Entry& a, const Entry& b) {
                return cols.empty() ? false : cols[0].value(a.player.stats) > cols[0].value(b.player.stats);
            });
        else if (order_.i == 2)
            std::stable_sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) { return text::foldLess(a.name, b.name); });
        if ((int)entries.size() > rows_.i) entries.resize(size_t(rows_.i));

        size_t width = cols.size() + 1;
        std::vector<float> w(width, 0.f);
        struct Row {
            std::vector<std::string> text;
            std::vector<ImU32> color;
            bool mine = false;
            bool marked = false;
        };
        std::vector<Row> rows;
        auto mark = srv::words(highlightWords_.text);
        for (auto& e : entries) {
            Row r;
            r.mine = e.name == game::state().player.name;
            r.marked = highlight_.b && std::find(mark.begin(), mark.end(), text::lower(e.name)) != mark.end();
            r.text.push_back(nick::show(e.name));
            r.color.push_back(nameColor(e.name, r));
            for (auto& c : cols) {
                std::string cell = "–";
                ImU32 col = textColor();
                if (e.player.load == hive::Load::Loading || e.player.load == hive::Load::None) cell = "…";
                else if (e.player.load == hive::Load::Ready) {
                    cell = c.text(e.player.stats);
                    col = c.ramp ? ImGui::GetColorU32(ramp(c.ramp, c.value(e.player.stats))) : textColor();
                }
                r.text.push_back(cell);
                r.color.push_back(col);
            }
            rows.push_back(std::move(r));
        }
        for (size_t i = 0; i < width; i++) {
            if (header_.b) w[i] = textSize(s, heads[i]).x;
            for (auto& r : rows) w[i] = std::max(w[i], textSize(s, r.text[i]).x);
        }

        float gap = 12.f * s, y = 0.f, total = 0.f;
        for (float x : w) total += x + gap;
        total -= gap;
        if (header_.b) {
            float x = 0.f;
            for (size_t i = 0; i < width; i++) {
                drawText(dl, o + ImVec2(x, y), s, heads[i], ImGui::GetColorU32(theme::current().textDim));
                x += w[i] + gap;
            }
            y += lineH;
        }
        for (auto& r : rows) {
            if (r.marked) dl->AddRectFilled(o + ImVec2(-3 * s, y), o + ImVec2(total + 3 * s, y + lineH), ImGui::GetColorU32(withAlpha(highlightColor_.color, 0.3f)), 3 * s);
            float x = 0.f;
            for (size_t i = 0; i < width; i++) {
                float off = i == 0 ? 0.f : w[i] - textSize(s, r.text[i]).x;
                drawText(dl, o + ImVec2(x + off, y), s, r.text[i], r.color[i]);
                x += w[i] + gap;
            }
            y += lineH;
        }
        if (rows.empty()) return drawText(dl, o, s, i18n::tr("No players"), ImGui::GetColorU32(theme::current().textDim));
        return {total, y};
    }

private:
    struct Entry {
        std::string name;
        hive::Player player;
    };

    struct Cell {
        const char* title;
        std::function<double(const hive::Stats&)> value;
        std::function<std::string(const hive::Stats&)> text;
        int ramp = 0;
    };

    bool here() const { return game::state().server == "The Hive"; }

    int gameIndex() const {
        if (game_.i > 0) return game_.i - 1;
        int g = hive::gameFromTitle(text::lower(game::state().scoreboard.title));
        return g >= 0 ? g : 0;
    }

    std::vector<std::string> players() const {
        std::vector<std::string> out;
        auto add = [&](const std::string& raw) {
            std::string n = text::strip(raw);
            if (n.empty() || std::find(out.begin(), out.end(), n) != out.end()) return;
            out.push_back(n);
        };
        if (self_.b) add(game::state().player.name);
        for (auto& t : game::state().tab) add(t.name);
        return out;
    }

    ImU32 nameColor(const std::string& name, const auto& row) const {
        if (row.marked) return ImGui::GetColorU32(highlightColor_.color);
        if (teamColors_.b) {
            static const ImU32 palette[] = {IM_COL32(255, 85, 85, 255), IM_COL32(85, 85, 255, 255), IM_COL32(85, 255, 85, 255), IM_COL32(255, 255, 85, 255),
                                            IM_COL32(85, 255, 255, 255), IM_COL32(255, 255, 255, 255), IM_COL32(255, 85, 255, 255), IM_COL32(170, 170, 170, 255)};
            for (auto& o : game::state().others)
                if (text::lower(o.name) == text::lower(name) && o.team > 0) return palette[(o.team - 1) % 8];
        }
        return row.mine ? accentColor() : textColor();
    }

    ImVec4 ramp(int kind, double v) const {
        float mid = kind == 1 ? kdMid_.f : kind == 2 ? fkdrMid_.f : winMid_.f;
        float high = kind == 1 ? kdHigh_.f : kind == 2 ? fkdrHigh_.f : winHigh_.f;
        if (!thresholds_.b) return textColor_.color;
        if (v >= high) return high_.color;
        return v >= mid ? mid_.color : low_.color;
    }

    std::vector<Cell> columns(int gi) const {
        auto kd = [](const hive::Stats& s) { return double(s.kd()); };
        auto fkdr = [](const hive::Stats& s) { return double(s.fkdr()); };
        auto win = [](const hive::Stats& s) { return double(s.winRate()); };
        auto num2 = [](double v) { return std::format("{:.2f}", v); };
        Cell cKd{"K/D", kd, [=](auto& s) { return num2(s.kd()); }, 1};
        Cell cFkdr{"FKDR", fkdr, [=](auto& s) { return num2(s.fkdr()); }, 2};
        Cell cWin{"Win %", win, [](auto& s) { return std::format("{:.0f}%", s.winRate()); }, 3};
        Cell cWins{"Wins", [](auto& s) { return s.wins; }, [](auto& s) { return shortNumber(s.wins); }};
        Cell cBeds{"Beds", [](auto& s) { return s.beds; }, [](auto& s) { return shortNumber(s.beds); }};

        std::vector<Cell> out;
        if (levelCol_.b)
            out.push_back({"Level", [](auto& s) { return s.xp; }, [](auto& s) { return shortNumber(s.xp) + (s.prestige > 0 ? std::format(" P{:.0f}", s.prestige) : ""); }});
        if (primary_.b) {
            bool bed = gi == 0;
            out.push_back(bed ? cFkdr : gi == 1 || gi == 2 || gi == 7 ? cKd : cWins);
            out.push_back(bed ? cBeds : gi == 6 ? cKd : cWin);
        }
        if (kdCol_.b) out.push_back(cKd);
        if (winCol_.b) out.push_back(cWin);
        if (fkdrCol_.b) out.push_back(cFkdr);
        if (winsCol_.b) out.push_back(cWins);
        if (lossCol_.b) out.push_back({"Losses", [](auto& s) { return s.losses(); }, [](auto& s) { return shortNumber(s.losses()); }});
        if (killCol_.b) out.push_back({"Kills", [](auto& s) { return s.kills; }, [](auto& s) { return shortNumber(s.kills); }});
        if (finalCol_.b) out.push_back({"Final kills", [](auto& s) { return s.finalKills; }, [](auto& s) { return shortNumber(s.finalKills); }});
        if (deathCol_.b) out.push_back({"Deaths", [](auto& s) { return s.deaths; }, [](auto& s) { return shortNumber(s.deaths); }});
        if (gamesCol_.b) out.push_back({"Games", [](auto& s) { return s.played; }, [](auto& s) { return shortNumber(s.played); }});
        if (streakCol_.b) out.push_back({"Streak", [](auto& s) { return s.streak; }, [](auto& s) { return shortNumber(s.streak); }});
        if (firstCol_.b)
            out.push_back({"First game", [](auto& s) { return double(s.firstPlayed); }, [](auto& s) {
                               if (s.firstPlayed <= 0) return std::string("–");
                               std::time_t t = std::time_t(s.firstPlayed);
                               std::tm tm{};
                               localtime_s(&tm, &t);
                               return std::format("{:04}-{:02}-{:02}", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
                           }});
        return out;
    }

    Setting& game_ = choice("game", "Game", {"Automatic", "BedWars", "SkyWars", "Treasure Wars", "Murder Mystery", "Hide and Seek", "Death Run",
                                              "Capture the Flag", "Ground Wars", "Just Build", "Block Drop", "Gravity", "Survival Games", "Block Party"});
    Setting& rows_ = intSlider("rows", "Players shown", 8, 1, 24);
    Setting& order_ = choice("order", "Order", {"Tab list", "Best first", "Name"});
    Setting& self_ = toggleSetting("self", "Show yourself first", true);
    Setting& header_ = toggleSetting("header", "Column titles", true);
    Setting& anchor_ = choice("anchor", "Anchor", {"Top left", "Top center", "Top right", "Middle left", "Center", "Middle right", "Bottom left", "Bottom center", "Bottom right"});
    Setting& cache_ = slider("cache", "Refresh stats after (min)", 10.f, 1.f, 60.f, "%.0f min");

    Setting& levelCol_ = toggleSetting("level", "Level", false);
    Setting& primary_ = toggleSetting("primary", "Primary and secondary value of the game", true);
    Setting& kdCol_ = toggleSetting("kd", "K/D", false);
    Setting& winCol_ = toggleSetting("win", "Win rate", false);
    Setting& fkdrCol_ = toggleSetting("fkdr", "FKDR", false);
    Setting& winsCol_ = toggleSetting("wins", "Wins", false);
    Setting& lossCol_ = toggleSetting("losses", "Losses", false);
    Setting& killCol_ = toggleSetting("kills", "Kills", false);
    Setting& finalCol_ = toggleSetting("finals", "Final kills", false);
    Setting& deathCol_ = toggleSetting("deaths", "Deaths", false);
    Setting& gamesCol_ = toggleSetting("games", "Games played", false);
    Setting& streakCol_ = toggleSetting("streak", "Win streak", false);
    Setting& firstCol_ = toggleSetting("first", "First game", false);

    Setting& thresholds_ = toggleSetting("thresholds", "Color values by size", true);
    Setting& kdMid_ = slider("kdMid", "K/D yellow from", 1.f, 0.f, 5.f, "%.1f");
    Setting& kdHigh_ = slider("kdHigh", "K/D green from", 2.f, 0.f, 10.f, "%.1f");
    Setting& fkdrMid_ = slider("fkdrMid", "FKDR yellow from", 1.f, 0.f, 5.f, "%.1f");
    Setting& fkdrHigh_ = slider("fkdrHigh", "FKDR green from", 3.f, 0.f, 15.f, "%.1f");
    Setting& winMid_ = slider("winMid", "Win rate yellow from (%)", 20.f, 0.f, 100.f, "%.0f");
    Setting& winHigh_ = slider("winHigh", "Win rate green from (%)", 40.f, 0.f, 100.f, "%.0f");
    Setting& teamColors_ = toggleSetting("teamColors", "Color names by team", true);
    Setting& highlight_ = toggleSetting("highlight", "Highlight players", false);
    Setting& highlightWords_ = textSetting("highlightWords", "Players (comma)", "");
    Setting& highlightColor_ = colorSetting("highlightColor", "Highlight", {1.f, 0.82f, 0.49f, 1.f});
    Setting& low_ = colorSetting("low", "Color low", {1.f, 0.4f, 0.45f, 1.f});
    Setting& mid_ = colorSetting("mid", "Color medium", {1.f, 0.82f, 0.49f, 1.f});
    Setting& high_ = colorSetting("high", "Color high", {0.55f, 0.91f, 0.69f, 1.f});
    Setting& overlayKey_ = keySetting("overlayKey", "Overlay key (0 = always shown)", 0);
    Setting& overlayHold_ = toggleSetting("overlayHold", "Only while the key is held", false);
    Setting& hubs_ = toggleSetting("hubs", "Also show in the hub", true);
    bool shown_ = false;
};

class HiveLeaderboard : public HudModule {
public:
    HiveLeaderboard()
        : HudModule("Hive Leaderboard", "Shows the top players of a Hive game, all time or this month. Only on The Hive.", {"info-others"}, {0.62f, 0.6f}) {
        sub("The Hive");
        moveTo(Category::Server);
    }

    void onRender(ImDrawList* dl) override {
        if (!here() && !gui::editingHud()) return;
        HudModule::onRender(dl);
    }

protected:
    ImVec2 pivot() const override { return anchorPivot(anchor_.i); }

    ImVec2 content(ImDrawList* dl, ImVec2 o, float s) override {
        int gi = game_.i > 0 ? game_.i - 1 : std::max(0, hive::gameFromTitle(text::lower(game::state().scoreboard.title)));
        hive::Board b = game::demo() ? hive::sampleBoard(rows_.i) : hive::board(gi, period_.i == 1, rows_.i, refresh_.f);
        std::string title = i18n::fmt("{}  ·  {}", hive::gameName(gi), i18n::tr(period_.i == 1 ? "This month" : "All time"));
        float y = 0.f, w = 0.f;
        if (showTitle_.b) {
            auto sz = drawText(dl, o, s, title, accentColor());
            w = sz.x;
            y += sz.y;
        }
        if (b.rows.empty()) {
            auto m = drawText(dl, o + ImVec2(0, y), s, i18n::tr(b.load == hive::Load::Failed ? "Could not load" : "Loading …"), ImGui::GetColorU32(theme::current().textDim));
            return {std::max(w, m.x), y + m.y};
        }
        float rankW = 0.f, nameW = 0.f, valueW = 0.f;
        for (auto& r : b.rows) {
            rankW = std::max(rankW, textSize(s, std::format("{}{}", rankText_.text, r.rank)).x);
            nameW = std::max(nameW, textSize(s, r.name).x);
            valueW = std::max(valueW, textSize(s, shortNumber(r.value)).x);
        }
        float gap = columnGap_.f * s;
        for (auto& r : b.rows) {
            float x = 0.f;
            if (showRank_.b) {
                drawText(dl, o + ImVec2(x, y), s, std::format("{}{}", rankText_.text, r.rank), ImGui::GetColorU32(theme::current().textDim));
                x += rankW + gap;
            }
            ImU32 col = r.rank <= topAccent_.i ? accentColor() : textColor();
            auto n = drawText(dl, o + ImVec2(x, y), s, r.name, col);
            x += nameW + gap;
            if (showValue_.b) {
                std::string v = shortNumber(r.value);
                drawText(dl, o + ImVec2(x + valueW - textSize(s, v).x, y), s, v, textColor());
                x += valueW;
            }
            w = std::max(w, x);
            y += n.y * rowGap_.f;
        }
        return {w, y};
    }

private:
    bool here() const { return game::state().server == "The Hive"; }

    Setting& showTitle_ = toggleSetting("title", "Title", true);
    Setting& rankText_ = textSetting("rankText", "Text in front of the rank", "#");
    Setting& topAccent_ = intSlider("topAccent", "Top ranks in accent color", 3, 0, 10);
    Setting& columnGap_ = slider("columnGap", "Gap between the columns", 10.f, 2.f, 40.f, "%.0f");
    Setting& rowGap_ = slider("rowGap", "Line spacing", 1.f, 0.8f, 1.8f, "%.2fx");
    Setting& game_ = choice("game", "Game", {"Automatic", "BedWars", "SkyWars", "Treasure Wars", "Murder Mystery", "Hide and Seek", "Death Run",
                                              "Capture the Flag", "Ground Wars", "Just Build", "Block Drop", "Gravity", "Survival Games", "Block Party"});
    Setting& period_ = choice("period", "List", {"All time", "This month"});
    Setting& rows_ = intSlider("rows", "Rows", 10, 1, 100);
    Setting& refresh_ = slider("refresh", "Refresh every (s)", 60.f, 5.f, 300.f, "%.0f s");
    Setting& showRank_ = toggleSetting("rank", "Show rank", true);
    Setting& showValue_ = toggleSetting("value", "Show value", true);
    Setting& anchor_ = choice("anchor", "Anchor", {"Top left", "Top center", "Top right", "Middle left", "Center", "Middle right", "Bottom left", "Bottom center", "Bottom right"});
};
