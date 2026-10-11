#pragma once

#include <string>
#include <vector>

namespace mediasession {

struct Session {
    std::string app;
    std::string title;
    std::string artist;
    bool playing = false;
    bool current = false;
};

bool list(std::vector<Session>& out);
void reset();

}
