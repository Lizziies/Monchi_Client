#pragma once

#include "gui/Notify.hpp"
#include "gui/Theme.hpp"
#include "modules/Module.hpp"
#include "modules/common/Needs.hpp"
#include "modules/server/ServerChat.hpp"
#include "sdk/Game.hpp"

#include <array>
#include <atomic>
#include <format>

class HiveUtils : public Module {
public:
    HiveUtils()
        : Module("Hive Utils", "Requeue helper, map avoider, role requeue, chat cleanup and auto accept for The Hive. Only runs on The Hive.",
                 Category::Server, {"chat", "timing"}) {
        sub("The Hive");
        require(need::chat | need::board, need::sigs({"ChatEvents", "ScoreboardData"}));
        solo_.visible = [this] { return requeue_.b; };
        onElim_.visible = [this] { return requeue_.b; };
        cmd_.visible = [this] { return requeue_.b || avoid_.b || anyRole(); };
        custom_.visible = [this] { return cmd_.i == 2 && (requeue_.b || avoid_.b || anyRole()); };
        endWords_.visible = [this] { return requeue_.b; };
        elimWords_.visible = [this] { return requeue_.b && onElim_.b; };
        avoidGame_.visible = [this] { return avoid_.b; };
        avoidMode_.visible = [this] { return avoid_.b; };
        limit_.visible = [this] { return drLimit_.b; };
        codePrefix_.visible = [this] { return copyCode_.b; };
        promoWords_.visible = [this] { return cleanPromo_.b; };
        unlockWords_.visible = [this] { return cleanUnlock_.b; };
        joinWords_.visible = [this] { return cleanJoin_.b; };
        teamWords_.visible = [this] { return cleanTeaming_.b; };
        friendWords_.visible = [this] { return friend_.b; };
        friendCmd_.visible = [this] { return friend_.b; };
        partyWords_.visible = [this] { return party_.b; };
        partyCmd_.visible = [this] { return party_.b; };
        voteMaps_.visible = [this] { return vote_.b; };
        voteSay_.visible = [this] { return vote_.b; };
        voteText_.visible = [this] { return vote_.b && voteSay_.b; };
        voteNotify_.visible = [this] { return vote_.b; };

        for (int g = 0; g < 5; g++)
            for (int m = 0; m < 4; m++) {
                auto& list = textSetting(std::format("avoid{}_{}", g, m), "Maps to avoid (comma)", "");
                list.visible = [this, g, m] { return avoid_.b && avoidGame_.i == g && avoidMode_.i == m; };
                lists_[size_t(g * 4 + m)] = &list;
            }

        game::filterChat([this](const std::string& raw) { return hides(raw); });
    }

    void onDisable() override { reset(); }
    void onServer(const ServerEvent&) override { reset(); }

    void onKey(KeyEvent& ev) override {
        if (ev.down && !ev.repeat && ev.vk == key_.i && key_.i) keyHit_ = true;
    }

    // the key arrives on the window thread; the server name and the outbox belong to the frame
    void onFrame() override {
        bool hit = keyHit_.exchange(false);
        if (hit && here()) requeue(i18n::tr("Hotkey"), 0.f);
        if (!here()) {
            reset();
            return;
        }
        readBoard();
        for (auto& e : game::events()) {
            if (e.kind == game::EventKind::Chat) chat(e.text);
            else if (e.kind == game::EventKind::Death) died();
        }
        outbox_.flush(chatKey_.i);
    }

    void drawSettings() override {
        auto& t = theme::current();
        ImGui::Spacing();
        if (!here()) {
            ImGui::TextColored(t.warn, i18n::tr("Not on The Hive. This module only works there."));
            return;
        }
        ImGui::TextColored(t.ok, i18n::tr("Connected to The Hive"));
        std::string gameName = game_ >= 0 ? i18n::tr(gameNames[game_]) : "–";
        std::string modeName = mode_ >= 0 ? i18n::tr(modeNames[mode_]) : "–";
        ImGui::TextDisabled("%s", i18n::fmt("Game: {}  ·  Mode: {}  ·  Map: {}", gameName, modeName, map_.empty() ? "–" : map_).c_str());
        ImGui::TextDisabled("%s", i18n::fmt("Role: {}  ·  Deaths this game: {}", role_.empty() ? "–" : i18n::tr(role_.c_str()), deaths_).c_str());
        if (ImGui::Button(i18n::tr("Requeue now"))) requeue(i18n::tr("Button"), 0.f);
        auto& sent = inject::sent();
        if (!sent.empty()) {
            ImGui::TextDisabled("%s", i18n::tr("Last commands"));
            int shown = 0;
            for (auto it = sent.rbegin(); it != sent.rend() && shown < 4; ++it, ++shown)
                ImGui::TextDisabled("%s%s", it->text.c_str(), it->real ? "" : i18n::tr("  (demo, not sent)"));
        }
    }

private:
    std::atomic<bool> keyHit_{false};
    static constexpr const char* gameNames[] = {"BedWars", "SkyWars", "Treasure Wars", "Ground Wars", "Capture the Flag"};
    static constexpr const char* modeNames[] = {"Solos", "Duos", "Squads", "Mega"};

    struct Role {
        const char* name;
        std::vector<std::string> phrases;
        Setting* flag;
    };

    bool here() const { return game::state().server == "The Hive"; }

    bool anyRole() const { return mmMurderer_.b || mmSheriff_.b || mmInnocent_.b || hsHider_.b || hsSeeker_.b || drDeath_.b || drRunner_.b || drLimit_.b; }

    void died() {
        deaths_++;
        if (drLimit_.b && deathRun_ && deaths_ >= limit_.i) requeue(i18n::fmt("{} deaths", deaths_), delay_.f);
    }

    static std::string tail(const std::string& raw) {
        std::string clean = text::strip(raw);
        size_t colon = clean.find(':');
        if (colon == std::string::npos) return "";
        size_t from = clean.find_first_not_of(' ', colon + 1);
        return from == std::string::npos ? "" : clean.substr(from);
    }

    void reset() {
        outbox_.clear();
        game_ = mode_ = -1;
        map_.clear();
        role_.clear();
        deaths_ = 0;
        title_.clear();
        deathRun_ = false;
    }

    std::string command() const { return cmd_.i == 0 ? "/q" : cmd_.i == 1 ? "/hub" : custom_.text; }

    void say(const std::string& why, const std::string& text, float delay) {
        outbox_.later(delay, text);
        if (toast_.b) notify::push(why, text, notify::Kind::Info);
    }

    void requeue(const std::string& why, float delay) {
        double now = ui::time();
        if (now - lastRequeue_ < cooldown_.f && delay > 0.f) return;
        lastRequeue_ = now;
        say(why, command(), delay);
    }

    static int indexIn(const std::string& line, std::initializer_list<const char*> keys) {
        int i = 0;
        for (auto k : keys) {
            if (line.find(k) != std::string::npos) return i;
            i++;
        }
        return -1;
    }

    void readBoard() {
        auto& sb = game::state().scoreboard;
        std::string title = srv::plain(sb.title);
        if (title != title_) {
            title_ = title;
            role_.clear();
            deaths_ = 0;
            map_.clear();
            game_ = indexIn(title, {"bed", "sky", "treasure", "ground", "capture"});
            deathRun_ = title.find("death") != std::string::npos;
        }
        for (auto& [raw, score] : sb.lines) {
            std::string line = srv::plain(raw);
            int m = indexIn(line, {"solo", "duo", "squad", "mega"});
            if (m >= 0) mode_ = m;
            if (line.rfind("map", 0) != 0) continue;
            setMap(tail(raw));
        }
    }

    void setMap(const std::string& name) {
        if (name.empty() || text::lower(name) == text::lower(map_)) return;
        map_ = name;
        if (avoid_.b && avoided(map_)) requeue(i18n::fmt("Avoiding map {}", map_), delay_.f);
    }

    bool avoided(const std::string& map) const {
        auto check = [&](int g, int m) {
            for (auto& entry : srv::words(lists_[size_t(g * 4 + m)]->text))
                if (entry == text::lower(map) || (entry.size() >= 4 && text::lower(map).find(entry) != std::string::npos)) return true;
            return false;
        };
        if (game_ < 0) return false;
        if (mode_ >= 0) return check(game_, mode_);
        for (int m = 0; m < 4; m++)
            if (check(game_, m)) return true;
        return false;
    }

    bool soloOk() const { return !solo_.b || mode_ == 0; }

    void chat(const std::string& raw) {
        if (srv::typed(raw)) return;
        std::string line = srv::plain(raw);

        if (map_.empty() && line.find("map: ") != std::string::npos) setMap(tail(raw));

        roleLine(line);

        if (requeue_.b && soloOk()) {
            if (srv::hasAny(line, srv::words(endWords_.text))) requeue(i18n::tr("Game over"), delay_.f);
            else if (onElim_.b && srv::hasAny(line, srv::words(elimWords_.text))) requeue(i18n::tr("Eliminated"), delay_.f);
        }

        if (copyCode_.b) {
            std::string code = srv::wordAfter(raw, {"custom server code", "server code", "join code"}, 4, 10);
            if (!code.empty()) {
                ImGui::SetClipboardText((codePrefix_.b ? "/cs " + code : code).c_str());
                notify::push(i18n::tr("Server code copied"), code, notify::Kind::Ok);
            }
        }

        if (friend_.b) accept(raw, srv::words(friendWords_.text), friendCmd_.text);
        if (party_.b) accept(raw, srv::words(partyWords_.text), partyCmd_.text);

        if (vote_.b) voteLine(line);
    }

    void accept(const std::string& raw, const std::vector<std::string>& phrases, const std::string& pattern) {
        std::string name = srv::nameBefore(raw, phrases);
        if (name.empty()) return;
        say(i18n::tr("Accepting"), srv::fill(pattern, "{name}", name), 0.8f);
    }

    void voteLine(const std::string& line) {
        if (line.find("vote") == std::string::npos) return;
        double now = ui::time();
        if (now - lastVote_ < 45.0) return;
        for (auto& favorite : srv::words(voteMaps_.text)) {
            if (line.find(favorite) == std::string::npos) continue;
            lastVote_ = now;
            std::string name = favorite;
            if (!name.empty()) name[0] = char(std::toupper((unsigned char)name[0]));
            if (voteNotify_.b) notify::push(i18n::tr("Map vote"), i18n::fmt("Vote for {}", name), notify::Kind::Info);
            if (voteSay_.b) outbox_.later(1.2f, srv::fill(voteText_.text, "{map}", name));
            return;
        }
    }

    void roleLine(const std::string& line) {
        std::array<Role, 7> roles{{
            {"Murderer", {"you are the murderer", "you are murderer", "your role: murderer"}, &mmMurderer_},
            {"Sheriff", {"you are the sheriff", "you are sheriff", "your role: sheriff"}, &mmSheriff_},
            {"Innocent", {"you are innocent", "you are an innocent", "your role: innocent"}, &mmInnocent_},
            {"Hider", {"you are a hider", "you are hiding", "your role: hider"}, &hsHider_},
            {"Seeker", {"you are a seeker", "you are the seeker", "your role: seeker"}, &hsSeeker_},
            {"Death", {"you are death", "you are the death", "your role: death"}, &drDeath_},
            {"Runner", {"you are a runner", "you are runner", "your role: runner"}, &drRunner_},
        }};
        for (auto& r : roles) {
            if (!srv::hasAny(line, r.phrases)) continue;
            role_ = r.name;
            if (r.flag->b) requeue(i18n::fmt("Role {}", i18n::tr(r.name)), delay_.f);
            return;
        }
    }

    bool hides(const std::string& raw) {
        if (!enabled() || !here()) return false;
        std::string line = srv::plain(raw);
        if (cleanPromo_.b && srv::hasAny(line, srv::words(promoWords_.text))) return true;
        if (cleanUnlock_.b && srv::hasAny(line, srv::words(unlockWords_.text))) return true;
        if (cleanJoin_.b && srv::hasAny(line, srv::words(joinWords_.text))) return true;
        if (cleanTeaming_.b && srv::hasAny(line, srv::words(teamWords_.text))) return true;
        if (!hideRankless_.b && !hidePlus_.b) return false;
        std::string tag;
        if (!srv::playerChat(text::strip(raw), tag)) return false;
        if (hideRankless_.b && tag.empty()) return true;
        return hidePlus_.b && text::lower(tag).find("hive+") != std::string::npos;
    }

    Setting& requeue_ = toggleSetting("requeue", "Auto requeue after a game", true);
    Setting& solo_ = toggleSetting("solo", "Only in solo modes", false);
    Setting& onElim_ = toggleSetting("onElim", "Also when your team is eliminated", false);
    Setting& cmd_ = choice("command", "Requeue command", {"/q", "/hub", "Custom"});
    Setting& custom_ = textSetting("custom", "Custom command", "/q");
    Setting& delay_ = slider("delay", "Delay (s)", 3.f, 0.5f, 15.f, "%.1f s");
    Setting& cooldown_ = slider("cooldown", "Minimum gap between requeues (s)", 15.f, 5.f, 120.f, "%.0f s");
    Setting& key_ = keySetting("requeueKey", "Requeue key", 0);
    Setting& chatKey_ = keySetting("chatKey", "Chat key in game", 'T');
    Setting& toast_ = toggleSetting("toast", "Show a notice for every action", true);
    Setting& endWords_ = textSetting("endWords", "Game end words (comma)", "game over, victory, you win");
    Setting& elimWords_ = textSetting("elimWords", "Elimination words (comma)", "you have been eliminated, your team has been eliminated");

    Setting& avoid_ = toggleSetting("avoid", "Map avoider", false);
    Setting& avoidGame_ = choice("avoidGame", "Game", {"BedWars", "SkyWars", "Treasure Wars", "Ground Wars", "Capture the Flag"});
    Setting& avoidMode_ = choice("avoidMode", "Mode", {"Solos", "Duos", "Squads", "Mega"});

    Setting& mmMurderer_ = toggleSetting("mmMurderer", "Murder Mystery: requeue as Murderer", false);
    Setting& mmSheriff_ = toggleSetting("mmSheriff", "Murder Mystery: requeue as Sheriff", false);
    Setting& mmInnocent_ = toggleSetting("mmInnocent", "Murder Mystery: requeue as Innocent", false);
    Setting& hsHider_ = toggleSetting("hsHider", "Hide and Seek: requeue as Hider", false);
    Setting& hsSeeker_ = toggleSetting("hsSeeker", "Hide and Seek: requeue as Seeker", false);
    Setting& drDeath_ = toggleSetting("drDeath", "Death Run: requeue as Death", false);
    Setting& drRunner_ = toggleSetting("drRunner", "Death Run: requeue as Runner", false);
    Setting& drLimit_ = toggleSetting("drLimit", "Death Run: requeue after too many deaths", false);
    Setting& limit_ = intSlider("deathLimit", "Deaths before requeue", 5, 1, 100);

    Setting& copyCode_ = toggleSetting("copyCode", "Copy custom server codes", true);
    Setting& codePrefix_ = toggleSetting("codePrefix", "Put /cs in front of the code", true);

    Setting& cleanPromo_ = toggleSetting("cleanPromo", "Hide promo messages", true);
    Setting& promoWords_ = textSetting("promoWords", "Promo words (comma)", "[!]");
    Setting& cleanUnlock_ = toggleSetting("cleanUnlock", "Hide unlock hints", true);
    Setting& unlockWords_ = textSetting("unlockWords", "Unlock words (comma)", "to unlock, unlock this, unlocked with");
    Setting& cleanJoin_ = toggleSetting("cleanJoin", "Hide join messages", false);
    Setting& joinWords_ = textSetting("joinWords", "Join words (comma)", "joined the game, joined the lobby");
    Setting& cleanTeaming_ = toggleSetting("cleanTeaming", "Hide No Teaming messages", false);
    Setting& teamWords_ = textSetting("teamWords", "Teaming words (comma)", "no teaming, teaming is not allowed");
    Setting& hideRankless_ = toggleSetting("hideRankless", "Hide chat from players without a rank", false);
    Setting& hidePlus_ = toggleSetting("hidePlus", "Hide chat from Hive+ players", false);

    Setting& friend_ = toggleSetting("friend", "Accept friend requests", false);
    Setting& friendWords_ = textSetting("friendWords", "Friend request words (comma)", "sent you a friend request, wants to be your friend");
    Setting& friendCmd_ = textSetting("friendCmd", "Friend accept command", "/friend accept {name}");
    Setting& party_ = toggleSetting("party", "Accept party invites", false);
    Setting& partyWords_ = textSetting("partyWords", "Party invite words (comma)", "invited you to their party, invited you to join their party");
    Setting& partyCmd_ = textSetting("partyCmd", "Party accept command", "/party accept {name}");

    Setting& vote_ = toggleSetting("vote", "Map vote helper", false);
    Setting& voteMaps_ = textSetting("voteMaps", "Favorite maps (comma)", "");
    Setting& voteSay_ = toggleSetting("voteSay", "Announce your vote in chat", false);
    Setting& voteText_ = textSetting("voteText", "Announcement ({map})", "@here vote for {map}!");
    Setting& voteNotify_ = toggleSetting("voteNotify", "Notice when a favorite map is up for vote", true);

    std::array<Setting*, 20> lists_{};
    srv::Outbox outbox_;
    std::string title_;
    std::string map_;
    std::string role_;
    bool deathRun_ = false;
    int game_ = -1;
    int mode_ = -1;
    int deaths_ = 0;
    double lastRequeue_ = -100.0;
    double lastVote_ = -100.0;
};
