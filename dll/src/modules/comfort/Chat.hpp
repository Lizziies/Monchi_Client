#pragma once

#include "render/GameText.hpp"

#include "core/Log.hpp"
#include "core/Paths.hpp"
#include "gui/Gui.hpp"
#include "gui/Notify.hpp"
#include "gui/Theme.hpp"
#include "hook/Input.hpp"
#include "modules/HudModule.hpp"
#include "modules/Manager.hpp"
#include "modules/client/ClientSettings.hpp"
#include "modules/common/Colors.hpp"
#include "modules/common/Icons.hpp"
#include "modules/common/Nick.hpp"
#include "modules/common/GameHud.hpp"
#include "modules/common/Needs.hpp"
#include "modules/common/Sounds.hpp"
#include "modules/common/Text.hpp"
#include "modules/common/FoldLess.hpp"
#include "modules/flarial/FlarialModules.hpp"
#include "modules/online/MonchiOnline.hpp"
#include "modules/server/ServerChat.hpp"
#include "render/Fonts.hpp"
#include "render/Ui.hpp"
#include "sdk/Effects.hpp"
#include "sdk/Game.hpp"
#include "sdk/Inject.hpp"

#include <windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <atomic>
#include <ctime>
#include <format>
#include <fstream>
#include <map>
#include <random>
#include <set>
#include <sstream>

inline std::vector<std::string> splitList(const std::string& in, char sep) {
    std::vector<std::string> out;
    std::stringstream ss(in);
    std::string part;
    while (std::getline(ss, part, sep)) {
        auto a = part.find_first_not_of(' '), b = part.find_last_not_of(' ');
        if (a != std::string::npos) out.push_back(part.substr(a, b - a + 1));
    }
    return out;
}

class AutoGG : public Module {
public:
    AutoGG()
        : Module("Auto GG", "Automatically writes gg at the end of a game. On a kill it is banned on some servers.",
                 Category::Comfort, {"chat"}) {
        sub("Chat");
        require(need::chat, need::sigs({"ChatEvents"}));
        triggers_.visible = [this] { return preset_.i == 1; };
        killMessage_.visible = [this] { return onKill_.b; };
    }

    void onDisable() override { pending_ = false; }

    void onFrame() override {
        double now = ui::time();
        // a message planned in one game is not said in the next world or after the module was off
        if (!game::state().inWorld) {
            pending_ = false;
            return;
        }
        if (pending_ && now > due_ + 15.0) pending_ = false;
        if (pending_ && now >= due_ && input::focused() && input::grabbed() && !gui::open()) {
            pending_ = false;
            inject::say(message_, chatKey_.i);
        }
        for (auto& e : game::events()) {
            if (e.kind == game::EventKind::Chat && onEnd_.b && !srv::typed(e.text) && endMatches(e.text)) schedule(pick(messages_.text), delay_.f);
            if (e.kind == game::EventKind::Kill && onKill_.b && !optionBlocked("onKill")) schedule(pick(killMessage_.text), 0.4f);
        }
    }

    void drawSettings() override {
        if (optionBlocked("onKill")) ImGui::TextColored(theme::current().warn, i18n::tr("On this server the message on a kill is banned and disabled."));
    }

private:
    bool endMatches(const std::string& raw) const {
        std::string line = text::lower(text::strip(raw));
        std::vector<std::string> list = preset_.i == 1 ? splitList(text::lower(triggers_.text), ',') : srv::endWords(game::state().server);
        for (auto& t : list)
            if (line.find(t) != std::string::npos) return true;
        return false;
    }

    std::string pick(const std::string& pool) {
        auto items = splitList(pool, '|');
        if (items.empty()) return "gg";
        return items[std::uniform_int_distribution<size_t>(0, items.size() - 1)(rng_)];
    }

    void schedule(const std::string& msg, float delay) {
        double now = ui::time();
        if (now - last_ < cooldown_.f) return;
        last_ = now;
        message_ = msg;
        due_ = now + delay;
        pending_ = true;
    }

    Setting& onEnd_ = toggleSetting("onEnd", "At the end of a game", true);
    Setting& onKill_ = toggleSetting("onKill", "After a kill", false);
    Setting& preset_ = choice("preset", "Detect game end", {"Automatic by server", "Own words"});
    Setting& triggers_ = textSetting("triggers", "Words in chat (comma)", "game over, victory");
    Setting& messages_ = textSetting("messages", "Messages (random, separate with |)", "gg|gg wp|good game");
    Setting& killMessage_ = textSetting("killMessage", "Message after kill", "gg");
    Setting& delay_ = slider("delay", "Delay (s)", 1.5f, 0.3f, 8.f, "%.1f s");
    Setting& cooldown_ = slider("cooldown", "Minimum gap (s)", 12.f, 3.f, 60.f, "%.0f s");
    Setting& chatKey_ = keySetting("chatKey", "Chat key in game", 'T');
    std::mt19937 rng_{std::random_device{}()};
    std::string message_;
    double due_ = 0.0;
    double last_ = -100.0;
    bool pending_ = false;
};

class MessageLogger : public Module {
public:
    MessageLogger()
        : Module("Message Logger", "Saves the chat to a text file per day, with timestamps and filter.", Category::Comfort, {"hud-self"}) {
        sub("Chat");
        require(need::chat, need::sigs({"ChatEvents"}));
    }

    void onFrame() override {
        for (auto& e : game::events()) {
            if (e.kind != game::EventKind::Chat) continue;
            std::string line = colors_.b ? text::strip(e.text) : e.text;
            if (!filter_.text.empty() && text::lower(line).find(text::lower(filter_.text)) == std::string::npos) continue;
            SYSTEMTIME t;
            GetLocalTime(&t);
            auto dir = paths::logs() / L"chat";
            std::error_code ec;
            std::filesystem::create_directories(dir, ec);
            std::ofstream out(dir / std::filesystem::path(std::format("chat-{:04}-{:02}-{:02}.txt", t.wYear, t.wMonth, t.wDay)), std::ios::app);
            if (stamp_.b) out << std::format("[{:02}:{:02}:{:02}] ", t.wHour, t.wMinute, t.wSecond);
            if (server_.b && !game::state().server.empty()) out << "[" << game::state().server << "] ";
            out << line << "\n";
            if (clean_.b) {
                std::ofstream cleanOut(dir / std::filesystem::path(std::format("chat-{:04}-{:02}-{:02}.clean.txt", t.wYear, t.wMonth, t.wDay)), std::ios::app);
                cleanOut << text::strip(e.text) << "\n";
            }
        }
    }

private:
    Setting& colors_ = toggleSetting("strip", "Remove color codes", true);
    Setting& stamp_ = toggleSetting("stamp", "Timestamp", true);
    Setting& server_ = toggleSetting("server", "Prefix the server name", false);
    Setting& clean_ = toggleSetting("clean", "Also write a clean file (no colors, no time)", false);
    Setting& filter_ = textSetting("filter", "Only lines containing (empty = all)", "");
};

class DeathLogger : public GameList {
public:
    DeathLogger()
        : GameList("Death Logger", "Remembers where you died and shows the last death points with coordinates.", need::player,
                   need::sigs({"LocalPlayer", "HurtEvents"}), {"hud-self"}, {0.26f, 0.42f}) {
        sub("Chat");
    }

    // the list and the clipboard belong to the render thread; the key only asks
    void onKey(KeyEvent& ev) override {
        if (ev.down && !ev.repeat && ev.vk == copyKey_.i && copyKey_.i) copyWanted_ = true;
    }

    void onFrame() override {
        if (copyWanted_.exchange(false) && !list_.empty()) {
            auto& d = list_.back();
            ImGui::SetClipboardText(std::format("{} {} {}", int(d.x), int(d.y), int(d.z)).c_str());
            notify::push(i18n::tr("Copied"), i18n::tr("Death point is on the clipboard."), notify::Kind::Ok);
        }
        for (auto& e : game::events()) {
            if (e.kind != game::EventKind::Death) continue;
            auto& p = game::state().player;
            list_.push_back({p.pos.x, p.pos.y, p.pos.z, p.dimension, ui::time()});
            while ((int)list_.size() > keep_.i) list_.erase(list_.begin());
            if (toast_.b) notify::push(i18n::tr("You died"), place(list_.back()), notify::Kind::Info);
        }
    }

    void onRender(ImDrawList* dl) override {
        if (!anyShown() && !gui::editingHud()) return;
        GameList::onRender(dl);
    }

protected:
    ImVec2 content(ImDrawList* dl, ImVec2 o, float s) override {
        float y = 0.f, w = 0.f;
        if (!anyShown()) return emptyNote_.b || gui::editingHud() ? drawText(dl, o, s, i18n::tr("No death yet"), textColor()) : ImVec2{};
        int shown = 0;
        for (auto it = list_.rbegin(); it != list_.rend() && shown < show_.i; ++it) {
            if (ui::time() - it->at > showFor_.f) continue;
            ++shown;
            std::string t = prefix_.text + place(*it);
            if (age_.b) t += sep_.text + text::clock(float(ui::time() - it->at));
            auto sz = drawText(dl, o + ImVec2(0, y), s, t, shown == 1 && newestAccent_.b ? accentColor() : textColor());
            w = std::max(w, sz.x);
            y += sz.y * lineGap_.f;
        }
        return {w, y};
    }

private:
    struct Death {
        float x, y, z;
        int dim;
        double at;
    };

    // a death point stays on screen for the set time, then only the memory (and the copy key) keeps it
    bool anyShown() const {
        double now = ui::time();
        for (auto& d : list_)
            if (now - d.at <= showFor_.f) return true;
        return false;
    }

    std::string place(const Death& d) const {
        static const char* dims[] = {"Overworld", "Nether", "The End"};
        std::string out = coords_.i == 1 ? std::format("{}, {}, {}", int(d.x), int(d.y), int(d.z))
                          : coords_.i == 2 ? std::format("X {} Y {} Z {}", int(d.x), int(d.y), int(d.z)) : std::format("{} {} {}", int(d.x), int(d.y), int(d.z));
        if (dim_.b && d.dim >= 0 && d.dim < 3) out += sep_.text + i18n::tr(dims[d.dim]);
        return out;
    }

    Setting& keep_ = intSlider("keep", "Remembered death points", 5, 1, 20);
    Setting& show_ = intSlider("show", "Shown death points", 3, 1, 10);
    Setting& showFor_ = slider("showFor", "Show for (s)", 45.f, 5.f, 600.f, "%.0f s");
    Setting& age_ = toggleSetting("age", "Time since death", true);
    Setting& toast_ = toggleSetting("toast", "Notice on death", true);
    Setting& copyKey_ = keySetting("copyKey", "Copy last point", 0);
    Setting& dim_ = toggleSetting("dimension", "Show the dimension", true);
    Setting& coords_ = choice("coords", "Coordinates as", {"12 64 -30", "12, 64, -30", "X 12 Y 64 Z -30"});
    Setting& sep_ = textSetting("sep", "Separator", "  ·  ");
    Setting& prefix_ = textSetting("prefix", "Text in front of each point", "");
    Setting& newestAccent_ = toggleSetting("newestAccent", "Newest point in accent color", true);
    Setting& lineGap_ = slider("lineGap", "Line spacing", 1.f, 0.8f, 1.8f, "%.2fx");
    Setting& emptyNote_ = toggleSetting("emptyNote", "Show a note while there is no death", true);
    std::vector<Death> list_;
    std::atomic<bool> copyWanted_{false};
};

class ChatPlus : public HudModule {
public:
    ChatPlus()
        : HudModule("Better Chat", "Your own chat in place of the game's: move it, resize it, change colors and background, stack identical lines like Compact Chat, clear it with a key, hide it, timestamps, filter and highlighting.",
                    {"hud-self"}, {0.005f, 0.65f}) {
        sub("Chat");
        require(need::chat, need::sigs({"ChatEvents"}));
        background_.b = false;
        highlightWords_.visible = [this] { return highlight_.b; };
        highlightColor_.visible = [this] { return highlight_.b; };
        mentionWords_.visible = [this] { return mention_.b; };
        mentionSound_.visible = [this] { return mention_.b; };
        mentionFile_.visible = [this] { return mention_.b && mentionSound_.i == 2; };
        countStyle_.visible = ownCountColor_.visible = [this] { return compact_.b; };
        countColor_.visible = [this] { return compact_.b && ownCountColor_.b; };
        bracketColor_.visible = [this] { return compact_.b && countStyle_.i > 0; };
    }

    bool defaultEnabled() const override { return false; }

    void onDisable() override {
        game::hideChatHud(false);
        game::drawNativeChat(false);
        flarialModules::nativeText({});
    }

    void onKey(KeyEvent& ev) override {
        if (ev.down && !ev.repeat && clearKey_.i && ev.vk == clearKey_.i) clearedAt_ = ui::time();
    }

    // The game's own hud chat is not moved or covered: lines that arrive while this is on simply get no time on the
    // game's hud (sdk/Live.cpp), whatever a resource pack has made of the chat panel. The chat screen is untouched.
    // A line with characters the menu fonts lack (a server's glyphs come from its pack) is drawn into the box by the
    // game itself, through the core; without the core such a line stays in the game's hud.
    void onFrame() override {
        game::hideChatHud(hideVanilla_.b && show_.b);
        game::drawNativeChat(show_.b && flarialModules::nativeTextReady());
        if (!mention_.b) return;
        for (auto& e : game::events()) {
            if (e.kind != game::EventKind::Chat) continue;
            std::string line = text::lower(text::strip(e.text));
            auto& me = game::state().player.name;
            std::string name = text::lower(me);
            if (!name.empty() && (line.rfind("<" + name + ">", 0) == 0 || line.rfind(name + ":", 0) == 0)) continue;
            bool hit = !name.empty() && line.find(name) != std::string::npos;
            for (auto& w : srv::words(mentionWords_.text))
                if (line.find(w) != std::string::npos) hit = true;
            if (hit && ui::time() - lastMention_ > 1.0) {
                lastMention_ = ui::time();
                ping();
            }
        }
    }

    void onRender(ImDrawList* dl) override {
        if (!game::state().inWorld && !gui::editingHud()) return;
        if (!show_.b && !gui::editingHud()) return;
        HudModule::onRender(dl);
    }

protected:
    ImVec2 content(ImDrawList* dl, ImVec2 o, float s) override {
        auto& chat = game::state().chat;
        bool open = game::state().screen == game::Screen::Chat || gui::editingHud();
        double now = ui::time();
        // The saved place is the box's upper or lower left corner, whichever it is anchored by. When the anchor is
        // changed the place is converted once, so the box stays where it is on the screen.
        if (anchorBottom_.b != anchored_.b && size().y > 0.f) {
            ImVec2 where = position();
            anchored_.b = anchorBottom_.b;
            setPosition(where);
        }
        // names and the text behind them have their own size; a line is as high as the bigger of the two
        float textS = s * textScale_.f, nameS = s * nameScale_.f;
        float w = width_.f * s, lineH = fonts::hudSize() * std::max(textS, nameS) * 1.15f * spacing_.f;
        auto drop = [&](float size) { return (lineH - fonts::hudSize() * size * 1.15f) * 0.5f; };
        std::vector<std::pair<const game::ChatLine*, int>> lines;
        std::vector<std::string> words = splitList(text::lower(highlightWords_.text), ',');
        std::string filter = text::lower(filter_.text);
        bool nativeReady = flarialModules::nativeTextReady();
        std::vector<flarialModules::NativeLine> nativeLines;

        for (auto& l : chat) {
            if (l.time <= clearedAt_ || (l.native && !nativeReady)) continue;
            std::string plain = text::lower(text::strip(l.text));
            if (!filter.empty() && plain.find(filter) == std::string::npos) continue;
            if (compact_.b && !lines.empty() && lines.back().first->text == l.text) {
                lines.back().second++;
                lines.back().first = &l;
                continue;
            }
            lines.push_back({&l, 1});
        }
        int total = int(lines.size());
        int from = std::max(0, total - lines_.i);
        float y = 0.f;
        ImDrawListSplitter layers;
        layers.Split(dl, 3);
        layers.SetCurrentChannel(dl, 2);

        for (int i = from; i < total; i++) {
            // newest at the bottom is how a chat reads; at the top it stays put when the box sits at the upper edge
            auto& [l, count] = lines[size_t(newestTop_.i == 1 ? total - 1 - (i - from) : i)];
            float age = float(now - l->time);
            float a = open ? 1.f : std::clamp((fade_.f - age) / 1.f, 0.f, 1.f);
            if (a <= 0.f) {
                y += 0.f;
                continue;
            }
            ImU32 base = ImGui::GetColorU32(withAlpha(textColor_.color, a));
            float x = 0.f;
            float startY = y;
            // A server's own symbols (a rank badge) can only be drawn by the game's font. Everything else in such a
            // line is drawn like any other line; only the symbols are handed to the game, once it has told how wide
            // they come out. Until then, and with the option off, the game's font draws the whole line.
            bool mixedLine = false;
            if (l->native && mixed_.b) {
                mixedLine = true;
                for (float size : {textS, nameS}) {
                    float nlh = fonts::hudSize() * size * 1.15f;
                    glyphRuns(text::strip(l->text), [&](const std::string& run, bool glyph) {
                        if (!glyph || flarialModules::nativeWidth(run, nlh) > 0.f) return;
                        mixedLine = false;
                        // drawn without ink, only to be measured
                        nativeLines.push_back({run, o.x, o.y + y, 100000.f, nlh, 0u});
                    });
                }
            }
            if (l->native && !mixedLine) {
                std::string shown = colors_.b ? l->text : text::strip(l->text);
                if (stamp_.b) shown = stampText(age) + shown;
                if (count > 1) shown += std::format(" \xC2\xA7r[x{}]", count);
                int rows = flarialModules::nativeRows(shown, w, lineH);
                // before the game has drawn the line once, its width is a guess
                if (rows <= 0) rows = std::max(1, int(std::ceil(float(text::strip(shown).size()) * lineH * 0.45f / w)));
                nativeLines.push_back({std::move(shown), o.x, o.y + y, w, lineH, base});
                y += lineH * float(rows);
                continue;
            }
            auto wrapped = [&](const std::string& value, ImU32 color, float size) {
                for (size_t at = 0; at < value.size();) {
                    if (value[at] == '\n') { x = 0.f; y += lineH; ++at; continue; }
                    bool space = value[at] == ' ' || value[at] == '\t';
                    size_t end = at + 1;
                    while (end < value.size() && value[end] != '\n' &&
                           (value[end] == ' ' || value[end] == '\t') == space) ++end;
                    std::string token = value.substr(at, end - at);
                    if (mixedLine && hasGlyph(token)) {
                        float nlh = fonts::hudSize() * size * 1.15f;
                        glyphRuns(token, [&](const std::string& run, bool glyph) {
                            float rw = glyph ? flarialModules::nativeWidth(run, nlh) : textSize(size, run).x;
                            if (x > 0.f && x + rw > w) { x = 0.f; y += lineH; }
                            if (glyph) nativeLines.push_back({run, o.x + x, o.y + y + drop(size), 100000.f, nlh, color});
                            else drawText(dl, o + ImVec2(x, y + drop(size)), size, run, color);
                            x += rw;
                        });
                        at = end;
                        continue;
                    }
                    float tw = textSize(size, token).x;
                    if (x > 0.f && x + tw > w) { x = 0.f; y += lineH; }
                    if (!space || x > 0.f) {
                        if (tw <= w) x += drawText(dl, o + ImVec2(x, y + drop(size)), size, token, color).x;
                        else for (size_t k = 0; k < token.size();) {
                            size_t next = k + 1;
                            while (next < token.size() && (static_cast<unsigned char>(token[next]) & 0xc0) == 0x80) ++next;
                            std::string glyph = token.substr(k, next - k);
                            float gw = textSize(size, glyph).x;
                            if (x > 0.f && x + gw > w) { x = 0.f; y += lineH; }
                            x += drawText(dl, o + ImVec2(x, y + drop(size)), size, glyph, color).x;
                            k = next;
                        }
                    }
                    at = end;
                }
            };
            if (stamp_.b) wrapped(stampText(age), ImGui::GetColorU32(withAlpha(theme::current().textDim, a)), textS);
            // the name is what stands before the '>' of "<name> text" or before the first ':' of "rank name: text"
            size_t done = 0, nameBytes = 0;
            auto nameEnd = [](const std::string& flat) {
                if (!flat.empty() && flat[0] == '<') {
                    size_t close = flat.find('>');
                    return close == std::string::npos ? size_t(0) : close + 1;
                }
                size_t colon = flat.find(':');
                return colon == std::string::npos || colon > 48 ? size_t(0) : colon + 1;
            };
            auto piece = [&](const std::string& value, ImU32 color) {
                size_t head = done < nameBytes ? std::min(value.size(), nameBytes - done) : 0;
                if (head) wrapped(value.substr(0, head), color, nameS);
                if (head < value.size()) wrapped(value.substr(head), color, textS);
                done += value.size();
            };
            std::string plain = text::lower(text::strip(l->text));
            bool hit = false;
            if (highlight_.b)
                for (auto& wd : words)
                    if (plain.find(wd) != std::string::npos) hit = true;

            std::string shownText = nick::replaceIn(l->text);
            if (auto* mo = modules::get<MonchiOnline>(); mo && mo->enabled()) shownText = online::tagLine(shownText, mo->colors() && onlineColors_.b, mo->hearts() && hearts_.b);
            if (colors_.b) {
                auto* cs = modules::get<ClientSettings>();
                auto segments = text::colored(cs ? cs->tagged(shownText, true) : shownText, base);
                std::string flat;
                for (auto& seg : segments)
                    if (seg.text != "\x01") flat += seg.text;
                nameBytes = nameEnd(flat);
                for (auto& seg : segments) {
                    ImVec4 c = ImGui::ColorConvertU32ToFloat4(seg.color);
                    c.w *= a;
                    if (seg.text == "\x01") {
                        float hs = fonts::hudSize() * nameS * 0.86f;
                        if (x > 0.f && x + hs + 2 * s > w) { x = 0.f; y += lineH; }
                        online::heartIcon(dl, o + ImVec2(x + hs * 0.5f, y + lineH * 0.5f), hs, ImGui::GetColorU32(c));
                        x += hs + 2 * s;
                        continue;
                    }
                    piece(seg.text, ImGui::GetColorU32(c));
                }
            } else {
                auto* cs = modules::get<ClientSettings>();
                std::string flat = text::strip(cs ? cs->tagged(shownText, true) : shownText);
                nameBytes = nameEnd(flat);
                piece(flat, base);
            }
            if (count > 1) {
                float cw = textSize(textS, std::format("[x{}]", count)).x + 4 * s;
                if (x > 0.f && x + cw > w) { x = 0.f; y += lineH; }
                counter(dl, o + ImVec2(x + 4 * s, y + drop(textS)), textS, count, a);
            }
            y += lineH;
            if (hit) {
                layers.SetCurrentChannel(dl, 1);
                dl->AddRectFilled(o + ImVec2(0, startY), o + ImVec2(w, y), ImGui::GetColorU32(withAlpha(highlightColor_.color, 0.25f * a)), 3 * s);
                layers.SetCurrentChannel(dl, 2);
            }
        }
        if (background_.b && y > 0.f) {
            layers.SetCurrentChannel(dl, 0);
            dl->AddRectFilled(o - ImVec2(4 * s, 2 * s), o + ImVec2(w + 4 * s, y + 2 * s), ImGui::GetColorU32(bgColor_.color), rounding_.f * s);
        }
        layers.Merge(dl);
        if (nativeReady) flarialModules::nativeText(nativeLines);
        return {w, std::max(y, lineH)};
    }

private:
    static std::string stampText(float age) {
        std::time_t t = std::time(nullptr) - std::time_t(age);
        std::tm tm{};
        localtime_s(&tm, &t);
        return std::format("[{:02}:{:02}] ", tm.tm_hour, tm.tm_min);
    }

    void counter(ImDrawList* dl, ImVec2 at, float s, int count, float a) {
        static const char* brackets[] = {"", "()", "[]", "{}", "<>"};
        const char* b = brackets[std::clamp(countStyle_.i, 0, 4)];
        ImVec4 num = ownCountColor_.b ? countColor_.color : theme::current().accent;
        ImU32 edge = ImGui::GetColorU32(withAlpha(bracketColor_.color, a));
        if (*b) at.x += drawText(dl, at, s, std::string(1, b[0]), edge).x;
        at.x += drawText(dl, at, s, std::format("x{}", count), ImGui::GetColorU32(withAlpha(num, a))).x;
        if (*b) drawText(dl, at, s, std::string(1, b[1]), edge);
    }

    void ping() const {
        int kind = mentionSound_.i;
        std::wstring file = kind == 2 ? logger::widen(mentionFile_.text) : std::wstring();
        sounds::post([kind, file] {
            if (kind == 2 && !file.empty() && PlaySoundW(file.c_str(), nullptr, SND_FILENAME | SND_ASYNC)) return;
            if (kind == 1) {
                PlaySoundA("SystemExclamation", nullptr, SND_ALIAS | SND_ASYNC);
                return;
            }
            MessageBeep(MB_ICONASTERISK);
        });
    }

    Setting& width_ = slider("width", "Width", 420.f, 200.f, 900.f, "%.0f");
    Setting& hearts_ = toggleSetting("hearts", "Hearts of Monchi users", true);
    Setting& onlineColors_ = toggleSetting("onlineColors", "Name colors of Monchi users", true);
    Setting& textScale_ = slider("textScale", "Text size", 1.f, 0.6f, 2.5f, "%.2fx");
    Setting& nameScale_ = slider("nameScale", "Name size", 1.f, 0.6f, 2.5f, "%.2fx");
    Setting& spacing_ = slider("lineSpacing", "Line spacing", 1.f, 0.8f, 2.f, "%.2fx");
    Setting& mention_ = toggleSetting("mention", "Sound when someone mentions you", false);
    Setting& mentionWords_ = textSetting("mentionWords", "More words ( @here, comma)", "@here, @everyone");
    Setting& mentionSound_ = choice("mentionSound", "Mention sound", {"System ding", "System alert", "Own WAV file"});
    Setting& mentionFile_ = textSetting("mentionFile", "WAV file path", "");
    double lastMention_ = -100.0;
    Setting& newestTop_ = choice("newest", "Newest message", {"At the bottom", "At the top"});
    Setting& anchorBottom_ = toggleSetting("anchorBottom", "Lower edge stays in place (the chat grows upward)", true);
    Setting& anchored_ = hiddenFlag("anchored");

    Setting& hiddenFlag(const char* id) {
        Setting& st = toggleSetting(id, id, false);
        st.hidden = true;
        return st;
    }

protected:
    // with few lines the chat then sits at the lower edge of its place instead of hanging at the top of it
    ImVec2 pivot() const override { return {0.f, anchored_.b ? 1.f : 0.f}; }

private:
    Setting& mixed_ = toggleSetting("mixedFont", "Server symbols inside my chat font (rank badges)", true);

    // private use characters are a server's own pictures: U+E000 to U+F8FF, three bytes starting EE or EF 80..A3
    static bool glyphAt(const std::string& s, size_t i) {
        unsigned char a = static_cast<unsigned char>(s[i]);
        if (i + 2 >= s.size() + 0 && i + 2 > s.size() - 1) return false;
        if (a == 0xEE) return true;
        return a == 0xEF && static_cast<unsigned char>(s[i + 1]) <= 0xA3;
    }
    static bool hasGlyph(const std::string& s) {
        for (size_t i = 0; i + 2 < s.size(); i++)
            if (glyphAt(s, i)) return true;
        return false;
    }
    // hands over the text in runs of symbols and runs of everything else, in order
    template <class F>
    static void glyphRuns(const std::string& s, F&& each) {
        size_t from = 0;
        bool glyph = false;
        for (size_t i = 0; i < s.size();) {
            unsigned char c = static_cast<unsigned char>(s[i]);
            size_t n = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
            if (i + n > s.size()) n = s.size() - i;
            bool g = n == 3 && glyphAt(s, i);
            if (g != glyph && i > from) {
                each(s.substr(from, i - from), glyph);
                from = i;
            }
            glyph = g;
            i += n;
        }
        if (s.size() > from) each(s.substr(from), glyph);
    }
    Setting& lines_ = intSlider("lines", "Visible lines", 10, 3, 30);
    Setting& fade_ = slider("fade", "Visible for (s)", 10.f, 3.f, 60.f, "%.0f s");
    Setting& stamp_ = toggleSetting("stamp", "Timestamp", false);
    Setting& compact_ = toggleSetting("compact", "Merge identical lines", true);
    Setting& colors_ = toggleSetting("colors", "Show color codes", true);
    Setting& highlight_ = toggleSetting("highlight", "Highlight words", false);
    Setting& highlightWords_ = textSetting("highlightWords", "Words (comma)", "");
    Setting& highlightColor_ = colorSetting("highlightColor", "Highlight", {1.f, 0.82f, 0.49f, 1.f});
    Setting& filter_ = textSetting("filter", "Only lines containing (empty = all)", "");
    Setting& hideVanilla_ = toggleSetting("hideVanilla", "Hide the original chat", true);
    Setting& show_ = toggleSetting("show", "Show the chat", true);
    Setting& clearKey_ = keySetting("clearKey", "Clear chat key", 0);
    double clearedAt_ = -1.0;
    Setting& countStyle_ = choice("countStyle", "Counter brackets", {"None", "( )", "[ ]", "{ }", "< >"});
    Setting& ownCountColor_ = toggleSetting("ownCountColor", "Own counter color", false);
    Setting& countColor_ = colorSetting("countColor", "Counter color", {0.67f, 0.f, 0.67f, 1.f});
    Setting& bracketColor_ = colorSetting("bracketColor", "Bracket color", {1.f, 1.f, 1.f, 1.f});
};

class PlayerNotifier : public Module {
public:
    PlayerNotifier()
        : Module("Player Notifier", "Tells you when a player from your list joins the server, for example friends.", Category::Comfort, {"info-others"}) {
        sub("Chat");
        require(need::tab, need::sigs({"TabListData"}));
    }

    void onKey(KeyEvent& ev) override {
        if (ev.down && !ev.repeat && checkKey_.i && ev.vk == checkKey_.i) asked_ = true;
    }

    // the key arrives on the window thread; the player list is read where the frame is drawn
    void onFrame() override {
        if (asked_.exchange(false)) report(false);
        double now = ui::time();
        if (now - last_ < 1.0) return;
        last_ = now;
        auto names = splitList(text::lower(list_.text), ',');
        std::set<std::string> present;
        for (auto& e : game::state().tab) {
            std::string n = text::lower(e.name);
            present.insert(n);
            if (watched(names, n) && !seen_.count(n) && primed_) {
                notify::push(i18n::tr("Player online"), e.name, notify::Kind::Info, 6.f);
                if (sound_.b) sounds::beep(MB_ICONASTERISK);
            }
        }
        if (leave_.b)
            for (auto& n : seen_)
                if (!present.count(n) && watched(names, n)) notify::push(i18n::tr("Player left"), n, notify::Kind::Info, 4.f);
        seen_ = present;
        primed_ = true;
        if (repeat_.f > 0.f && now - reminded_ >= repeat_.f) {
            reminded_ = now;
            report(true);
        }
    }

    void onServer(const ServerEvent&) override {
        seen_.clear();
        primed_ = false;
        reminded_ = ui::time();
    }

private:
    bool watched(const std::vector<std::string>& names, const std::string& n) const {
        for (auto& w : names)
            if (n == w || (partial_.b && n.find(w) != std::string::npos)) return true;
        return false;
    }

    std::atomic<bool> asked_{false};

    void report(bool quiet) {
        auto names = splitList(text::lower(list_.text), ',');
        std::string online;
        for (auto& e : game::state().tab)
            if (watched(names, text::lower(e.name))) online += (online.empty() ? "" : ", ") + e.name;
        if (online.empty() && quiet) return;
        notify::push(i18n::tr("Players online"), online.empty() ? i18n::tr("Nobody from your list is here.") : online, notify::Kind::Info, 6.f);
    }

    Setting& list_ = textSetting("list", "Names (comma)", "");
    Setting& sound_ = toggleSetting("sound", "Sound", true);
    Setting& leave_ = toggleSetting("leave", "Also report when they leave", false);
    Setting& partial_ = toggleSetting("partial", "Match part of the name", false);
    Setting& repeat_ = slider("repeat", "Remind who is online every (s, 0 = never)", 0.f, 0.f, 600.f, "%.0f s");
    Setting& checkKey_ = keySetting("checkKey", "Check now", 0);
    std::set<std::string> seen_;
    bool primed_ = false;
    double last_ = 0.0;
    double reminded_ = 0.0;
};

class ScoreboardPlus : public HudModule {
public:
    ScoreboardPlus()
        : HudModule("Scoreboard", "Your own movable scoreboard without red numbers.",
                    {"hud-self"}, {0.84f, 0.3f}) {
        sub("HUD parts");
        require(need::board, need::sigs({"ScoreboardData"}));
    }

    void onDisable() override { flarialModules::hideScoreboard(false); }

    void onFrame() override {
        bool can = drawable();
        bool hide = hideVanilla_.b && can;
        if (hide) fx::skip(fx::Id::HideScoreboard);
        flarialModules::hideScoreboard(hide);
        // A server that builds its board from its own symbols (Zeqa) cannot be drawn in Monchi's font. The game's own
        // board stays then, and the player is told once per server instead of wondering why nothing changes.
        auto& st = game::state();
        bool has = !st.scoreboard.title.empty() || !st.scoreboard.lines.empty();
        if (!st.inWorld || !has || can) return;
        if (toldFor_ == st.server && told_) return;
        toldFor_ = st.server;
        told_ = true;
        notify::push(i18n::tr("Scoreboard"), i18n::tr("This server draws its scoreboard with its own symbols. Monchi leaves the game's scoreboard as it is here."), notify::Kind::Warn, 7.f);
    }

    std::string proof() const override {
        auto& sb = game::state().scoreboard;
        if (sb.title.empty() && sb.lines.empty()) return "no scoreboard on screen, 0 times";
        return drawable() ? std::format("draws {} lines of the server's board", sb.lines.size()) : "the server's board uses its own symbols, the game's board is left on, 0 times";
    }

    void onRender(ImDrawList* dl) override {
        if (!game::state().inWorld && !gui::editingHud()) return;
        if (!drawable() && !gui::editingHud()) return;
        HudModule::onRender(dl);
    }

protected:
    ImVec2 content(ImDrawList* dl, ImVec2 o, float s) override {
        // the lines are measured and drawn where the game state keeps them, nothing is copied per frame
        auto& sb = game::state().scoreboard;
        float w = 0.f, y = 0.f;
        size_t rows = std::min(sb.lines.size(), size_t(std::max(0, max_.i)));
        const std::string& title = sb.title;
        float titleW = titleOn_.b && !title.empty() ? textSize(s, title).x : 0.f;
        w = titleW;
        char number[16];
        for (size_t i = 0; i < rows; i++) {
            float lineW = textSize(s, sb.lines[i].first).x;
            if (numbers_.b) {
                std::snprintf(number, sizeof(number), "%d", sb.lines[i].second);
                lineW += textSize(s, number).x + numberGap_.f * s;
            }
            w = std::max(w, lineW);
        }
        if (titleOn_.b && !title.empty()) {
            float tx = titleAlign_.i == 0 ? (w - titleW) * 0.5f : titleAlign_.i == 1 ? 0.f : w - titleW;
            drawText(dl, o + ImVec2(tx, 0), s, title, titleAccent_.b ? accentColor() : textColor());
            y += fonts::hudSize() * s * (1.12f * lineGap_.f + titleGap_.f);
        }
        ImU32 numberColor = ImGui::GetColorU32(numberColor_.color);
        for (size_t i = 0; i < rows; i++) {
            drawText(dl, o + ImVec2(0, y), s, sb.lines[i].first, textColor());
            if (numbers_.b) {
                std::snprintf(number, sizeof(number), "%d", sb.lines[i].second);
                drawText(dl, o + ImVec2(w - textSize(s, number).x, y), s, number, numberColor);
            }
            y += fonts::hudSize() * s * 1.12f * lineGap_.f;
        }
        return {std::max(w, 80.f * s), std::max(y, 14.f * s)};
    }

private:
    bool drawable() const {
        const auto& board = game::state().scoreboard;
        if (board.title.empty() && board.lines.empty()) return false;
        if (!gameText::canDraw(fonts::hud(), board.title)) return false;
        for (const auto& row : board.lines)
            if (!gameText::canDraw(fonts::hud(), row.first)) return false;
        return true;
    }

    Setting& titleOn_ = toggleSetting("title", "Title", true);
    Setting& numbers_ = toggleSetting("numbers", "Numbers on the right", false);
    Setting& numberColor_ = colorSetting("numberColor", "Number color", {1.f, 0.4f, 0.45f, 1.f});
    Setting& max_ = intSlider("max", "Maximum lines", 15, 3, 20);
    Setting& titleAlign_ = choice("titleAlign", "Title position", {"Centered", "Left", "Right"});
    Setting& titleAccent_ = toggleSetting("titleAccent", "Title in accent color", true);
    Setting& titleGap_ = slider("titleGap", "Gap under the title", 0.18f, 0.f, 1.f, "%.2f");
    Setting& lineGap_ = slider("lineGap", "Line spacing", 1.f, 0.8f, 1.8f, "%.2fx");
    Setting& numberGap_ = slider("numberGap", "Gap in front of the numbers", 16.f, 4.f, 60.f, "%.0f");
    Setting& hideVanilla_ = toggleSetting("hideVanilla", "Hide the original scoreboard", true);
    std::string toldFor_;
    bool told_ = false;
};

class TabList : public HudModule {
public:
    TabList()
        : HudModule("Tab List", "Java-style player list with heads, platform icons, ping, world name, columns, sorting and highlights.",
                    {"info-others"}, {0.30f, 0.05f}) {
        sub("HUD parts");
        require(need::tab, need::sigs({"TabListData"}));
        rows_.visible = [this] { return columns_.i == 0; };
        worldName_.visible = [this] { return header_.b; };
        count_.visible = [this] { return header_.b; };
        serverPing_.visible = [this] { return header_.b; };
        pingBars_.visible = [this] { return ping_.b; };
        toggleKey_.visible = [this] { return !onHold_.b; };
        highlightWords_.visible = [this] { return highlight_.b; };
        highlightColor_.visible = [this] { return highlight_.b; };
    }

    void onKey(KeyEvent& ev) override {
        if (ev.down && !ev.repeat && toggleKey_.i && ev.vk == toggleKey_.i) shown_ = !shown_;
    }

    void onRender(ImDrawList* dl) override {
        if (!game::state().inWorld && !gui::editingHud()) return;
        bool visible = onHold_.b ? input::down(VK_TAB) : shown_ || !toggleKey_.i;
        if (!visible && !gui::editingHud()) return;
        HudModule::onRender(dl);
    }

protected:
    ImVec4 backgroundColor() const override {
        ImVec4 color = bgColor_.color;
        color.w = std::max(color.w, 0.82f);
        return color;
    }

    ImVec2 content(ImDrawList* dl, ImVec2 o, float s) override {
        auto& st = game::state();
        std::vector<game::TabEntry> list = st.tab;
        for (auto& entry : list) {
            std::replace(entry.name.begin(), entry.name.end(), '\n', ' ');
            std::replace(entry.name.begin(), entry.name.end(), '\r', ' ');
        }
        if (sort_.i == 0) std::sort(list.begin(), list.end(), [](auto& a, auto& b) { return text::foldLess(a.name, b.name); });
        else if (sort_.i == 1) std::sort(list.begin(), list.end(), [](auto& a, auto& b) { return a.ping < b.ping; });
        auto* cs = modules::get<ClientSettings>();
        auto* mo = modules::get<MonchiOnline>();
        if (mo && !mo->enabled()) mo = nullptr;
        if (mo && monchiFirst_.b)
            std::stable_partition(list.begin(), list.end(), [](auto& e) {
                online::User u;
                return online::find(e.name, u);
            });
        int per = columns_.i > 0 ? int((list.size() + size_t(columns_.i) - 1) / size_t(columns_.i)) : rows_.i;
        per = std::max(per, 1);
        int cols = std::max(1, int((list.size() + size_t(per) - 1) / size_t(per)));
        float rowH = fonts::hudSize() * s * 1.15f + spacing_.f * s;
        float icon = rowH - spacing_.f * s - 2 * s;
        auto marks = highlights();

        float head = 0.f, y0 = 0.f, headerW = 0.f;
        if (header_.b) {
            std::string title;
            if (worldName_.b) title = st.world.name.empty() ? st.server : st.world.name;
            if (count_.b) title += (title.empty() ? "" : "  ·  ") + i18n::fmt("{} players", list.size());
            float x = 0.f;
            if (!title.empty()) x += drawText(dl, o, s, title, accentColor()).x;
            if (serverPing_.b) {
                ImVec4 c4 = rampColor(float(st.world.ping), 40.f, 200.f, good_.color, mid_.color, bad_.color);
                std::string t = std::format("{} ms", st.world.ping);
                float dot = 4.f * s;
                x += title.empty() ? 0.f : 10.f * s;
                dl->AddCircleFilled(o + ImVec2(x + dot, rowH * 0.5f), dot, ImGui::GetColorU32(c4));
                x += dot * 2 + 4 * s;
                x += drawText(dl, o + ImVec2(x, 0), s, t, ImGui::GetColorU32(c4)).x;
            }
            headerW = x;
            y0 = rowH + 2 * s;
        }

        float x = 0.f, total = 0.f;
        for (int c = 0; c < cols; c++) {
            float colW = 60.f * s;
            for (int r = 0; r < per; r++) {
                size_t i = size_t(c * per + r);
                if (i >= list.size()) break;
                std::string extra = list[i].name == st.player.name && cs ? cs->tabTag() : "";
                online::User peer;
                bool monchi = mo && online::find(list[i].name, peer);
                if (monchi && list[i].name != st.player.name) extra = peer.style.tag;
                if (monchi && mo->hearts() && peer.style.heart) extra += "    ";
                if (monchi && !peer.role.empty()) extra += " " + online::badge(peer.role);
                float w = textSize(s, list[i].name).x + (extra.empty() ? 0.f : textSize(s, extra).x + 6 * s) + (heads_.b && list[i].hasHead ? rowH : 0.f) + (platform_.b && list[i].platform != game::Platform::Unknown ? rowH : 0.f) + (ping_.b ? 56.f * s : 8.f * s);
                colW = std::max(colW, w);
            }
            for (int r = 0; r < per; r++) {
                size_t i = size_t(c * per + r);
                if (i >= list.size()) break;
                auto& e = list[i];
                bool me = e.name == st.player.name;
                auto mark = highlight_.b ? marks.find(text::lower(e.name)) : marks.end();
                bool marked = mark != marks.end();
                ImVec4 markColor = marked ? mark->second : highlightColor_.color;
                ImVec2 row = o + ImVec2(x, y0 + r * rowH);
                if (marked) dl->AddRectFilled(row - ImVec2(3 * s, 0), row + ImVec2(colW + 3 * s, rowH), ImGui::GetColorU32(withAlpha(markColor, 0.3f)), 3 * s);
                float cx = 0.f;
                if (heads_.b && e.hasHead) {
                    icons::head(dl, e, row + ImVec2(0, (rowH - icon) * 0.5f), icon);
                    cx += rowH;
                }
                if (platform_.b && e.platform != game::Platform::Unknown) {
                    icons::platform(dl, e.platform, row + ImVec2(cx, (rowH - icon) * 0.5f), icon, ImGui::GetColorU32(theme::current().textDim));
                    cx += rowH;
                }
                ImU32 nameColor = marked ? ImGui::GetColorU32(markColor) : me ? accentColor() : textColor();
                if (nick::mine(e.name)) nameColor = nick::colorOf(nick::color, nameColor);
                std::string shownName = nick::show(e.name);
                online::User peer;
                bool monchi = mo && online::find(e.name, peer);
                if (monchi && mo->hearts() && peer.style.heart) {
                    float hs = icon * 0.8f;
                    online::heartIcon(dl, {row.x + cx + hs * 0.5f, row.y + rowH * 0.5f}, hs, online::rgb(peer.style.heartColor));
                    cx += hs + 4 * s;
                }
                ImVec2 namePos = row + ImVec2(cx, spacing_.f * s * 0.5f);
                // a name the server has colored itself keeps the server's colors: painting it letter by letter would
                // show its color codes as text and push the tag into the name
                bool styled = shownName.find("§") != std::string::npos;
                if (monchi && mo->colors() && !marked && !styled) online::paint(shownName, peer.style, ui::time(), [&](const std::string& piece, ImU32 col, float x) { return drawText(dl, namePos + ImVec2(x, 0), s, piece, col).x; });
                else drawText(dl, namePos, s, shownName, nameColor);
                float after = namePos.x + textSize(s, shownName).x + 6 * s;
                std::string tabTag = me && cs ? cs->tabTag() : monchi ? peer.style.tag : "";
                if (!tabTag.empty()) {
                    drawText(dl, {after, namePos.y}, s, tabTag, me && cs ? ImGui::GetColorU32(cs->tagColor()) : online::rgb(peer.style.tagColor));
                    after += textSize(s, tabTag).x + 6 * s;
                }
                if (monchi && !peer.role.empty()) drawText(dl, {after, namePos.y}, s, online::badge(peer.role), online::rgb(0x3BA7EC));
                if (ping_.b) {
                    ImVec4 c4 = rampColor(float(e.ping), 40.f, 200.f, good_.color, mid_.color, bad_.color);
                    if (pingBars_.b) {
                        int lit = e.ping < 60 ? 4 : e.ping < 110 ? 3 : e.ping < 180 ? 2 : 1;
                        for (int b = 0; b < 4; b++) {
                            float bh = (3 + b * 2) * s;
                            ImVec2 base{row.x + colW - 26 * s + b * 5 * s, row.y + rowH - 3 * s};
                            dl->AddRectFilled(base - ImVec2(0, bh), base + ImVec2(3 * s, 0), b < lit ? ImGui::GetColorU32(c4) : IM_COL32(255, 255, 255, 40));
                        }
                    } else {
                        std::string t = std::format("{}", e.ping);
                        drawText(dl, row + ImVec2(colW - textSize(s, t).x - 6 * s, spacing_.f * s * 0.5f), s, t, ImGui::GetColorU32(c4));
                    }
                }
            }
            x += colW + 12 * s;
            total = x;
        }
        (void)head;
        return {std::max({total - 12 * s, 60.f * s, headerW}), y0 + std::min(per, int(list.size())) * rowH};
    }

private:
    // "name" uses the highlight color, "name=ff8800" its own
    std::map<std::string, ImVec4> highlights() const {
        std::map<std::string, ImVec4> out;
        for (auto& entry : srv::words(highlightWords_.text)) {
            auto eq = entry.find('=');
            ImVec4 c = highlightColor_.color;
            if (eq != std::string::npos) {
                size_t hex = entry.find_first_not_of(" #", eq + 1);
                if (hex != std::string::npos && entry.size() - hex >= 6) {
                    unsigned v = std::strtoul(entry.substr(hex, 6).c_str(), nullptr, 16);
                    c = {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f, 1.f};
                }
            }
            std::string name = entry.substr(0, eq);
            while (!name.empty() && name.back() == ' ') name.pop_back();
            out[name] = c;
        }
        return out;
    }

    Setting& columns_ = intSlider("columns", "Columns (0 = by rows)", 0, 0, 6);
    Setting& rows_ = intSlider("rows", "Rows per column", 20, 5, 40);
    Setting& sort_ = choice("sort", "Sorting", {"Name", "Ping", "As sent by the server"}, 2);
    Setting& spacing_ = slider("spacing", "Row spacing", 0.f, 0.f, 8.f, "%.0f");
    Setting& heads_ = toggleSetting("heads", "Player heads", true);
    Setting& platform_ = toggleSetting("platform", "Platform icons", true);
    Setting& header_ = toggleSetting("header", "Header", true);
    Setting& worldName_ = toggleSetting("worldName", "World name", true);
    Setting& count_ = toggleSetting("count", "Player count", true);
    Setting& serverPing_ = toggleSetting("serverPing", "Server ping with color dot", true);
    Setting& ping_ = toggleSetting("ping", "Show ping", true);
    Setting& pingBars_ = toggleSetting("pingBars", "Ping as bars", true);
    Setting& highlight_ = toggleSetting("highlight", "Highlight players", false);
    Setting& highlightWords_ = textSetting("highlightWords", "Players (comma, name=ff8800 for an own color)", "");
    Setting& onHold_ = toggleSetting("onHold", "Only while Tab is held", true);
    Setting& toggleKey_ = keySetting("toggleKey", "Toggle key", 0);
    Setting& good_ = colorSetting("good", "Ping good", {0.55f, 0.91f, 0.69f, 1.f});
    Setting& mid_ = colorSetting("mid", "Ping medium", {1.f, 0.82f, 0.49f, 1.f});
    Setting& bad_ = colorSetting("bad", "Ping bad", {1.f, 0.4f, 0.45f, 1.f});
    Setting& highlightColor_ = colorSetting("highlightColor", "Highlight", {1.f, 0.82f, 0.49f, 1.f});
    Setting& monchiFirst_ = toggleSetting("monchiFirst", "Monchi players first", false);
    bool shown_ = false;
};
