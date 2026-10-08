#pragma once

#include <string>

namespace music {

enum class Action { PlayPause, Next, Previous, VolumeUp, VolumeDown, Mute };

struct Track {
    std::string player;
    std::string title;
    bool playing = false;
    bool found = false;
};

void use(bool on);
Track now();
void send(Action a);

}
