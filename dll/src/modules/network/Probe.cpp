#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "core/Guard.hpp"
#include "Probe.hpp"
#include "Link.hpp"
#include "core/Log.hpp"
#include "server/Rules.hpp"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <deque>
#include <mutex>
#include <thread>

namespace probe {

using CreateFn = decltype(&IcmpCreateFile);
using SendFn = decltype(&IcmpSendEcho);
using CloseFn = decltype(&IcmpCloseHandle);

static std::mutex lock;
static Config config;
static Snapshot shown;
static std::deque<float> samples;
static std::deque<double> spikes;
static std::atomic<int> users{0};
static std::atomic<int> generation{0};
static std::atomic<int> threads{0};

static double clockSeconds() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

static void sleepWhile(int ms, int gen) {
    for (int waited = 0; waited < ms && generation == gen && users > 0; waited += 50) Sleep(50);
}

class Icmp {
public:
    Icmp() {
        HMODULE lib = lib_ = LoadLibraryW(L"iphlpapi.dll");
        if (!lib) return;
        auto create = reinterpret_cast<CreateFn>(reinterpret_cast<void*>(GetProcAddress(lib, "IcmpCreateFile")));
        send_ = reinterpret_cast<SendFn>(reinterpret_cast<void*>(GetProcAddress(lib, "IcmpSendEcho")));
        close_ = reinterpret_cast<CloseFn>(reinterpret_cast<void*>(GetProcAddress(lib, "IcmpCloseHandle")));
        if (create && send_ && close_) handle_ = create();
    }

    ~Icmp() {
        if (ok()) close_(handle_);
        if (lib_) FreeLibrary(lib_);
    }

    bool ok() const { return handle_ != INVALID_HANDLE_VALUE; }

    float ping(uint32_t addr, int timeoutMs) const {
        char data[32] = {};
        alignas(8) char reply[256];
        LARGE_INTEGER f, a, b;
        QueryPerformanceFrequency(&f);
        QueryPerformanceCounter(&a);
        DWORD n = send_(handle_, addr, data, sizeof(data), nullptr, reply, sizeof(reply), (DWORD)timeoutMs);
        QueryPerformanceCounter(&b);
        if (n == 0 || reinterpret_cast<ICMP_ECHO_REPLY*>(reply)->Status != IP_SUCCESS) return -1.f;
        return float(double(b.QuadPart - a.QuadPart) * 1000.0 / double(f.QuadPart));
    }

private:
    HMODULE lib_ = nullptr;
    HANDLE handle_ = INVALID_HANDLE_VALUE;
    SendFn send_ = nullptr;
    CloseFn close_ = nullptr;
};

class RakNet {
public:
    ~RakNet() { close(); }

    bool open(const sockaddr* sa, int len) {
        close();
        sock_ = socket(sa->sa_family, SOCK_DGRAM, IPPROTO_UDP);
        if (sock_ == INVALID_SOCKET) return false;
        DWORD timeout = 900;
        setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
        if (connect(sock_, sa, len) != 0) {
            close();
            return false;
        }
        return true;
    }

    float ping() {
        static constexpr unsigned char magic[16] = {0x00, 0xff, 0xff, 0x00, 0xfe, 0xfe, 0xfe, 0xfe,
                                                    0xfd, 0xfd, 0xfd, 0xfd, 0x12, 0x34, 0x56, 0x78};
        unsigned char out[33] = {0x01};
        uint64_t stamp = ++counter_;
        for (int i = 0; i < 8; i++) out[1 + i] = (unsigned char)(stamp >> (56 - 8 * i));
        std::copy(magic, magic + 16, out + 9);
        for (int i = 0; i < 8; i++) out[25 + i] = (unsigned char)(0x4d + i);

        LARGE_INTEGER f, a, b;
        QueryPerformanceFrequency(&f);
        QueryPerformanceCounter(&a);
        if (send(sock_, reinterpret_cast<const char*>(out), sizeof(out), 0) != (int)sizeof(out)) return -1.f;

        unsigned char in[512];
        for (int tries = 0; tries < 4; tries++) {
            int n = recv(sock_, reinterpret_cast<char*>(in), sizeof(in), 0);
            if (n < 9) return -1.f;
            if (in[0] == 0x1c && std::equal(in + 1, in + 9, out + 1)) {
                QueryPerformanceCounter(&b);
                return float(double(b.QuadPart - a.QuadPart) * 1000.0 / double(f.QuadPart));
            }
        }
        return -1.f;
    }

    void close() {
        if (sock_ != INVALID_SOCKET) closesocket(sock_);
        sock_ = INVALID_SOCKET;
    }

private:
    SOCKET sock_ = INVALID_SOCKET;
    uint64_t counter_ = 0;
};

struct Target {
    std::string text;
    sockaddr_storage addr{};
    int len = 0;
    bool v4 = false;
};

static bool resolve(const std::string& host, int port, bool wantV4, Target& out) {
    out.text = host;
    out.addr = {};
    sockaddr_in* v4 = reinterpret_cast<sockaddr_in*>(&out.addr);
    sockaddr_in6* v6 = reinterpret_cast<sockaddr_in6*>(&out.addr);
    if (inet_pton(AF_INET, host.c_str(), &v4->sin_addr) == 1) {
        v4->sin_family = AF_INET;
        v4->sin_port = htons((u_short)port);
        out.len = sizeof(sockaddr_in);
        out.v4 = true;
        return true;
    }
    if (!wantV4 && inet_pton(AF_INET6, host.c_str(), &v6->sin6_addr) == 1) {
        v6->sin6_family = AF_INET6;
        v6->sin6_port = htons((u_short)port);
        out.len = sizeof(sockaddr_in6);
        out.v4 = false;
        return true;
    }
    hostent* he = gethostbyname(host.c_str());
    if (!he || he->h_addrtype != AF_INET || !he->h_addr_list[0]) return false;
    v4->sin_family = AF_INET;
    v4->sin_port = htons((u_short)port);
    std::memcpy(&v4->sin_addr, he->h_addr_list[0], 4);
    out.len = sizeof(sockaddr_in);
    out.v4 = true;
    return true;
}

static void analyse() {
    std::vector<float> ok;
    int lost = 0;
    for (float s : samples) {
        if (s < 0.f) lost++;
        else ok.push_back(s);
    }

    shown.history.assign(samples.begin(), samples.end());
    shown.loss = samples.empty() ? 0.f : 100.f * lost / samples.size();
    shown.avg = shown.min = shown.max = shown.jitter = 0.f;
    if (ok.empty()) return;

    float sum = 0.f, diff = 0.f;
    shown.min = shown.max = ok.front();
    for (size_t i = 0; i < ok.size(); i++) {
        sum += ok[i];
        shown.min = std::min(shown.min, ok[i]);
        shown.max = std::max(shown.max, ok[i]);
        if (i) diff += std::fabs(ok[i] - ok[i - 1]);
    }
    shown.avg = sum / ok.size();
    shown.jitter = ok.size() > 1 ? diff / (ok.size() - 1) : 0.f;

    std::vector<float> sorted = ok;
    std::sort(sorted.begin(), sorted.end());
    float median = sorted[sorted.size() / 2];
    float last = ok.back();
    double now = clockSeconds();
    if (samples.back() >= 0.f && last > std::max(median * 2.5f, median + 15.f) &&
        (spikes.empty() || now - spikes.back() > 2.0))
        spikes.push_back(now);
    while (!spikes.empty() && now - spikes.front() > 240.0) spikes.pop_front();

    shown.spikePeriod = 0.f;
    if (spikes.size() >= 4) {
        double total = spikes.back() - spikes.front();
        double mean = total / double(spikes.size() - 1);
        bool even = mean >= 8.0 && mean <= 130.0;
        for (size_t i = 1; i < spikes.size() && even; i++) {
            double gap = spikes[i] - spikes[i - 1];
            even = std::fabs(gap - mean) <= mean * 0.25;
        }
        if (even) shown.spikePeriod = (float)mean;
    }
}

static void push(float rtt, const Config& cfg) {
    std::scoped_lock g(lock);
    samples.push_back(rtt);
    while ((int)samples.size() > std::max(10, cfg.window)) samples.pop_front();
    shown.sent++;
    if (rtt >= 0.f) {
        shown.received++;
        shown.last = rtt;
    } else {
        shown.last = -1.f;
    }
    analyse();
}

static void loop(int gen) {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return;
    Icmp icmp;
    RakNet raknet;

    Target target;
    std::string wanted;
    Method method = Method::Icmp;
    int port = 0;
    double nextLink = 0;

    while (generation == gen && users > 0) {
        Config cfg;
        {
            std::scoped_lock g(lock);
            cfg = config;
        }

        std::string host = cfg.host;
        if (host.empty()) host = rules::status().ip;
        if (host.empty()) {
            {
                std::scoped_lock g(lock);
                shown.resolved = false;
                shown.target.clear();
            }
            sleepWhile(500, gen);
            continue;
        }

        if (host != wanted || cfg.method != method || cfg.port != port) {
            wanted = host;
            method = cfg.method;
            port = cfg.port;
            bool ok = resolve(host, port, method == Method::Icmp, target);
            if (ok && method == Method::RakNet) ok = raknet.open(reinterpret_cast<sockaddr*>(&target.addr), target.len);
            {
                std::scoped_lock g(lock);
                samples.clear();
                spikes.clear();
                shown = Snapshot{};
                shown.running = true;
                shown.resolved = ok;
                shown.target = host;
            }
            if (!ok) {
                wanted.clear();
                sleepWhile(2000, gen);
                continue;
            }
        }

        double now = clockSeconds();
        if (now >= nextLink) {
            nextLink = now + 3.0;
            std::string ip = wanted;
            auto l = netlink::query(ip);
            std::scoped_lock g(lock);
            shown.link = l;
        }

        float rtt = -1.f;
        if (method == Method::Icmp) {
            auto* in = reinterpret_cast<sockaddr_in*>(&target.addr);
            if (icmp.ok() && target.v4) rtt = icmp.ping(in->sin_addr.S_un.S_addr, 800);
        } else {
            rtt = raknet.ping();
        }
        push(rtt, cfg);

        int wait = std::max(100, cfg.intervalMs - (rtt > 0 ? int(rtt) : 800));
        sleepWhile(wait, gen);
    }

    raknet.close();
    WSACleanup();
    {
        std::scoped_lock g(lock);
        shown.running = false;
    }
    threads--;
}

void use(bool on) {
    if (on) {
        if (users++ == 0) {
            int gen = ++generation;
            threads++;
            std::thread([gen] { guard::call("probe", [gen] { loop(gen); }); }).detach();
        }
        return;
    }
    if (users > 0) users--;
}

void configure(const Config& c) {
    std::scoped_lock g(lock);
    config = c;
}

Snapshot snapshot() {
    std::scoped_lock g(lock);
    return shown;
}

Metrics metrics() {
    std::scoped_lock g(lock);
    return static_cast<const Metrics&>(shown);
}

void shutdown() {
    users = 0;
    for (int i = 0; i < 60 && threads > 0; i++) Sleep(50);
}

}
