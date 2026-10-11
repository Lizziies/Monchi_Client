#pragma once

#include <string>
#include <vector>

namespace probe {

enum class Method { Icmp, RakNet };

struct Config {
    Method method = Method::Icmp;
    int intervalMs = 1000;
    int window = 60;
    int port = 19132;
    std::string host;
};

enum class LinkKind { Unknown, Wired, Wifi, Other };

struct Link {
    LinkKind kind = LinkKind::Unknown;
    std::string adapter;
    std::string ssid;
    std::string standard;
    std::string band;
    int channel = 0;
    int signal = -1;
    int rssi = 0;
    int linkMbps = 0;
    int rxMbps = 0;
    int txMbps = 0;
    int powerSaving = -1;
};

struct Metrics {
    bool running = false;
    bool resolved = false;
    float last = -1.f;
    float avg = 0.f;
    float min = 0.f;
    float max = 0.f;
    float jitter = 0.f;
    float loss = 0.f;
    int sent = 0;
    int received = 0;
    float spikePeriod = 0.f;
};

struct Snapshot : Metrics {
    std::string target;
    std::vector<float> history;
    Link link;
};

void use(bool on);
void configure(const Config& c);
Snapshot snapshot();
Metrics metrics();
void shutdown();

}
