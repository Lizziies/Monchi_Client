#pragma once

#include "Music.hpp"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>

namespace music {

inline bool songTitle(const std::string& title) {
    if (title.empty()) return false;
    std::string low = title;
    std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    if (low == "default ime" || low == "msctfime ui") return false;
    return low.find(".exe") == std::string::npos ||
        (low.find("amazon") == std::string::npos && low.find("spotify") == std::string::npos &&
         low.find("applemusic") == std::string::npos && low.find("deezer") == std::string::npos);
}

class TrackState {
public:
    Track update(Track next, uint64_t now) {
        if (!songTitle(next.title)) next.title.clear();
        if (next.found && !next.title.empty()) {
            last_ = next;
            at_ = now;
            return next;
        }
        if (last_.found && now >= at_ && now - at_ <= 1500 &&
            (!next.found || next.player == last_.player)) {
            Track kept = last_;
            if (next.found) kept.playing = next.playing;
            return kept;
        }
        last_ = {};
        return next;
    }
private:
    Track last_;
    uint64_t at_ = 0;
};

}
