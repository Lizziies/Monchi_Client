#pragma once

#include <string>
#include <vector>

namespace net {

struct Peer {
    std::string ip;
    std::string host;
    int packets = 0;
};

void install();
// Looks a host name up and remembers its addresses under `as`. The game joins the servers of its own list by address,
// without a lookup of its own, so their names have to be known beforehand.
void learn(const std::string& host, const std::string& as);
void waitIdle(int timeoutMs);
bool session();
std::vector<Peer> drain();

}
