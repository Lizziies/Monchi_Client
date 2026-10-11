#pragma once

#include <string>
#include <vector>

namespace rules {

struct Status {
    std::string server;
    std::string host;
    std::string ip;
    std::string notice;
    std::string rulesUrl;
    int blocked = 0;
    int warned = 0;
    std::vector<std::string> notes;
};

void init();
void tick();
// applies the current server's rules again right away, for modules registered after joining
void refresh();
Status status();
std::string source();

}
