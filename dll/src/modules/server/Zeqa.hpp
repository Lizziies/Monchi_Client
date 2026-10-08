#pragma once

#include "gui/Notify.hpp"
#include "gui/Theme.hpp"
#include "modules/Module.hpp"
#include "modules/common/Needs.hpp"
#include "modules/server/ServerChat.hpp"
#include "sdk/Game.hpp"

class ZeqaUtils : public Module {
public:
    ZeqaUtils()
        : Module("Zeqa Utils", "Requeue for duels after a match, chat cleanup and auto accept for Zeqa. Only runs on Zeqa.", Category::Server,
                 {"chat", "timing"}) {
        sub("Zeqa");
        require(need::chat, need::sigs({"ChatEvents"}));
        queueType_.visible = [this] { return requeue_.b; };
        queueMode_.visible = [this] { return requeue_.b; };
        queueCmd_.visible = [this] { return requeue_.b; };
        endWords_.visible = [this] { return requeue_.b; };
        delay_.visible = [this] { return requeue_.b; };
        promoWords_.visible = [this] { return cleanPromo_.b; };
        joinWords_.visible = [this] { return cleanJoin_.b; };
        leaveWords_.visible = [this] { return cleanLeave_.b; };
        streakWords_.visible = [this] { return cleanStreak_.b; };
        duelWords_.visible = [this] { return duel_.b; };
        duelCmd_.visible = [this] { return duel_.b; };
        friendWords_.visible = [this] { return friend_.b; };
        friendCmd_.visible = [this] { return friend_.b; };
        game::filterChat([this](const std::string& raw) { return hides(raw); });
    }

    void onDisable() override { outbox_.clear(); }
    void onServer(const ServerEvent&) override { outbox_.clear(); }

    void onKey(KeyEvent& ev) override {
        if (ev.down && !ev.repeat && ev.vk == requeueKey_.i && requeueKey_.i) keyHit_ = true;
    }

    // the key arrives on the window thread; the server name and the outbox belong to the frame
    void onFrame() override {
        bool hit = keyHit_.exchange(false);
        if (hit && here()) queue(i18n::tr("Hotkey"), 0.f);
        if (!here()) {
            outbox_.clear();
            return;
        }
        for (auto& e : game::events())
            if (e.kind == game::EventKind::Chat) chat(e.text);
        outbox_.flush(chatKey_.i);
    }

    void drawSettings() override {
        auto& t = theme::current();
        ImGui::Spacing();
        if (!here()) {
            ImGui::TextColored(t.warn, i18n::tr("Not on Zeqa. This module only works there."));
            return;
        }
        ImGui::TextColored(t.ok, i18n::tr("Connected to Zeqa"));
        if (ImGui::Button(i18n::tr("Requeue now"))) queue(i18n::tr("Button"), 0.f);
        auto& sent = inject::sent();
        if (sent.empty()) return;
        ImGui::TextDisabled("%s", i18n::tr("Last commands"));
        int shown = 0;
        for (auto it = sent.rbegin(); it != sent.rend() && shown < 4; ++it, ++shown)
            ImGui::TextDisabled("%s%s", it->text.c_str(), it->real ? "" : i18n::tr("  (demo, not sent)"));
    }

private:
    std::atomic<bool> keyHit_{false};
    bool here() const { return game::state().server == "Zeqa"; }

    void say(const std::string& why, const std::string& text, float delay) {
        outbox_.later(delay, text);
        if (toast_.b) notify::push(why, text, notify::Kind::Info);
    }

    void queue(const std::string& why, float delay) {
        double now = ui::time();
        if (delay > 0.f && now - lastQueue_ < cooldown_.f) return;
        lastQueue_ = now;
        std::string cmd = srv::fill(queueCmd_.text, "{type}", queueType_.i == 0 ? "ranked" : "unranked");
        say(why, srv::fill(cmd, "{mode}", queueMode_.text), delay);
    }

    void chat(const std::string& raw) {
        if (srv::typed(raw)) return;
        std::string line = srv::plain(raw);
        if (requeue_.b && srv::hasAny(line, srv::words(endWords_.text))) queue(i18n::tr("Match over"), delay_.f);
        if (duel_.b) accept(raw, srv::words(duelWords_.text), duelCmd_.text);
        if (friend_.b) accept(raw, srv::words(friendWords_.text), friendCmd_.text);
    }

    void accept(const std::string& raw, const std::vector<std::string>& phrases, const std::string& pattern) {
        std::string name = srv::nameBefore(raw, phrases);
        if (name.empty()) return;
        say(i18n::tr("Accepting"), srv::fill(pattern, "{name}", name), 0.8f);
    }

    bool hides(const std::string& raw) {
        if (!enabled() || !here()) return false;
        std::string line = srv::plain(raw);
        if (cleanPromo_.b && srv::hasAny(line, srv::words(promoWords_.text))) return true;
        if (cleanJoin_.b && srv::hasAny(line, srv::words(joinWords_.text))) return true;
        if (cleanLeave_.b && srv::hasAny(line, srv::words(leaveWords_.text))) return true;
        return cleanStreak_.b && srv::hasAny(line, srv::words(streakWords_.text));
    }

    Setting& requeue_ = toggleSetting("requeue", "Requeue after a match", true);
    Setting& queueType_ = choice("queueType", "Queue", {"Ranked", "Unranked"});
    Setting& queueMode_ = textSetting("queueMode", "Duel mode", "nodebuff");
    Setting& queueCmd_ = textSetting("queueCmd", "Queue command ({type} {mode})", "/queue {type} {mode}");
    Setting& endWords_ = textSetting("endWords", "Match end words (comma)", "won the duel, has won, you won, you lost");
    Setting& delay_ = slider("delay", "Delay (s)", 3.f, 0.5f, 15.f, "%.1f s");
    Setting& cooldown_ = slider("cooldown", "Minimum gap between requeues (s)", 10.f, 3.f, 120.f, "%.0f s");
    Setting& requeueKey_ = keySetting("requeueKey", "Requeue key", 0);
    Setting& chatKey_ = keySetting("chatKey", "Chat key in game", 'T');
    Setting& toast_ = toggleSetting("toast", "Show a notice for every action", true);

    Setting& cleanPromo_ = toggleSetting("cleanPromo", "Hide promo messages", true);
    Setting& promoWords_ = textSetting("promoWords", "Promo words (comma)", "[!], discord.gg, store.zeqa");
    Setting& cleanJoin_ = toggleSetting("cleanJoin", "Hide join messages", false);
    Setting& joinWords_ = textSetting("joinWords", "Join words (comma)", "joined the server, has joined");
    Setting& cleanLeave_ = toggleSetting("cleanLeave", "Hide leave messages", false);
    Setting& leaveWords_ = textSetting("leaveWords", "Leave words (comma)", "left the server, has left");
    Setting& cleanStreak_ = toggleSetting("cleanStreak", "Hide kill streak messages", false);
    Setting& streakWords_ = textSetting("streakWords", "Kill streak words (comma)", "kill streak, killstreak");

    Setting& duel_ = toggleSetting("duel", "Accept duel requests", false);
    Setting& duelWords_ = textSetting("duelWords", "Duel request words (comma)", "sent you a duel request, challenged you to a duel");
    Setting& duelCmd_ = textSetting("duelCmd", "Duel accept command", "/duel accept {name}");
    Setting& friend_ = toggleSetting("friend", "Accept friend requests", false);
    Setting& friendWords_ = textSetting("friendWords", "Friend request words (comma)", "sent you a friend request, wants to be your friend");
    Setting& friendCmd_ = textSetting("friendCmd", "Friend accept command", "/friend accept {name}");

    srv::Outbox outbox_;
    double lastQueue_ = -100.0;
};
