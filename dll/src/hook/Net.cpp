#include "Net.hpp"
#include "Hook.hpp"
#include "core/Log.hpp"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <atomic>
#include <set>
#include <map>
#include <mutex>

namespace net {

using SendToFn = int(WSAAPI*)(SOCKET, const char*, int, int, const sockaddr*, int);
using WSASendToFn = int(WSAAPI*)(SOCKET, LPWSABUF, DWORD, LPDWORD, DWORD, const sockaddr*, int, LPWSAOVERLAPPED,
                                  LPWSAOVERLAPPED_COMPLETION_ROUTINE);
using GetAddrInfoFn = int(WSAAPI*)(PCSTR, PCSTR, const ADDRINFOA*, PADDRINFOA*);
using GetAddrInfoWFn = int(WSAAPI*)(PCWSTR, PCWSTR, const ADDRINFOW*, PADDRINFOW*);

static SendToFn oSendTo = nullptr;
static WSASendToFn oWSASendTo = nullptr;
static GetAddrInfoFn oGetAddrInfo = nullptr;
static GetAddrInfoWFn oGetAddrInfoW = nullptr;

static std::mutex lock;
static std::map<std::string, std::string> hosts;
static std::map<std::string, int> counts;
static ULONGLONG windowStart = 0;
static int windowSends = 0;
static int lastRate = 0;
static int streak = 0;

static std::string ipOf(const sockaddr* sa) {
    char buf[64]{};
    if (sa->sa_family == AF_INET) {
        auto* in = reinterpret_cast<const sockaddr_in*>(sa);
        inet_ntop(AF_INET, &in->sin_addr, buf, sizeof(buf));
    } else if (sa->sa_family == AF_INET6) {
        auto* in = reinterpret_cast<const sockaddr_in6*>(sa);
        if (IN6_IS_ADDR_V4MAPPED(&in->sin6_addr)) {
            inet_ntop(AF_INET, &in->sin6_addr.s6_addr[12], buf, sizeof(buf));
        } else {
            inet_ntop(AF_INET6, &in->sin6_addr, buf, sizeof(buf));
        }
    }
    return buf;
}

static int portOf(const sockaddr* sa) {
    if (sa->sa_family == AF_INET) return ntohs(reinterpret_cast<const sockaddr_in*>(sa)->sin_port);
    if (sa->sa_family == AF_INET6) return ntohs(reinterpret_cast<const sockaddr_in6*>(sa)->sin6_port);
    return 0;
}

static bool ignored(const std::string& ip, int port) {
    if (ip.empty() || port == 53) return true;
    return ip.starts_with("127.") || ip == "::1" || ip.starts_with("224.") || ip.starts_with("239.") ||
           ip == "255.255.255.255" || ip.starts_with("ff");
}

static void count(const sockaddr* to, int len) {
    if (!to || len < (int)sizeof(sockaddr_in)) return;
    auto ip = ipOf(to);
    if (ignored(ip, portOf(to))) return;
    std::scoped_lock g(lock);
    // two lobbies of one network can sit behind one address and differ only in the port
    static std::set<std::pair<std::string, int>> told;
    if (told.size() < 64 && told.emplace(ip, portOf(to)).second) logger::info("net: first packet to {} port {}", ip, portOf(to));
    counts[ip]++;
    ULONGLONG now = GetTickCount64();
    if (now - windowStart >= 1000) {
        lastRate = windowSends;
        streak = lastRate >= 12 && now - windowStart < 2500 ? streak + 1 : 0;
        windowSends = 0;
        windowStart = now;
    }
    windowSends++;
}

static void remember(const char* host, const addrinfo* res) {
    if (!host || !res) return;
    std::scoped_lock g(lock);
    for (auto* p = res; p; p = p->ai_next)
        if (p->ai_addr) hosts[ipOf(p->ai_addr)] = host;
}

static int WSAAPI sendTo(SOCKET s, const char* buf, int len, int flags, const sockaddr* to, int tolen) {
    count(to, tolen);
    return oSendTo(s, buf, len, flags, to, tolen);
}

static int WSAAPI wsaSendTo(SOCKET s, LPWSABUF bufs, DWORD n, LPDWORD sent, DWORD flags, const sockaddr* to, int tolen,
                            LPWSAOVERLAPPED ov, LPWSAOVERLAPPED_COMPLETION_ROUTINE cr) {
    count(to, tolen);
    return oWSASendTo(s, bufs, n, sent, flags, to, tolen, ov, cr);
}

// name lookups can block for seconds; unloading waits until no thread is still inside one of these
static std::atomic<int> inside{0};

struct Inside {
    Inside() { inside++; }
    ~Inside() { inside--; }
};

void waitIdle(int timeoutMs) {
    for (int waited = 0; inside > 0 && waited < timeoutMs; waited += 20) Sleep(20);
}

static int WSAAPI getAddrInfo(PCSTR node, PCSTR service, const ADDRINFOA* hints, PADDRINFOA* res) {
    Inside guard;
    int r = oGetAddrInfo(node, service, hints, res);
    if (r == 0 && res) remember(node, *res);
    return r;
}

static int WSAAPI getAddrInfoW(PCWSTR node, PCWSTR service, const ADDRINFOW* hints, PADDRINFOW* res) {
    Inside guard;
    int r = oGetAddrInfoW(node, service, hints, res);
    if (r == 0 && res && *res && node) {
        auto host = logger::narrow(node);
        std::scoped_lock g(lock);
        for (auto* p = *res; p; p = p->ai_next)
            if (p->ai_addr) hosts[ipOf(p->ai_addr)] = host;
    }
    return r;
}

void install() {
    hook::create("sendto", hook::exported(L"ws2_32.dll", "sendto"), sendTo, &oSendTo);
    hook::create("WSASendTo", hook::exported(L"ws2_32.dll", "WSASendTo"), wsaSendTo, &oWSASendTo);
    hook::create("getaddrinfo", hook::exported(L"ws2_32.dll", "getaddrinfo"), getAddrInfo, &oGetAddrInfo);
    hook::create("GetAddrInfoW", hook::exported(L"ws2_32.dll", "GetAddrInfoW"), getAddrInfoW, &oGetAddrInfoW);
}

bool session() {
    std::scoped_lock g(lock);
    return GetTickCount64() - windowStart < 3000 && streak >= 3;
}

void learn(const std::string& host, const std::string& as) {
    Inside guard;
    addrinfo hints{};
    hints.ai_socktype = SOCK_DGRAM;
    addrinfo* res = nullptr;
    auto lookup = oGetAddrInfo ? oGetAddrInfo : getaddrinfo;
    if (lookup(host.c_str(), nullptr, &hints, &res) != 0 || !res) return;
    {
        std::scoped_lock g(lock);
        for (auto* p = res; p; p = p->ai_next)
            if (p->ai_addr) hosts.try_emplace(ipOf(p->ai_addr), as);
    }
    freeaddrinfo(res);
}

std::vector<Peer> drain() {
    std::scoped_lock g(lock);
    std::vector<Peer> out;
    for (auto& [ip, n] : counts) {
        auto it = hosts.find(ip);
        out.push_back({ip, it == hosts.end() ? "" : it->second, n});
    }
    counts.clear();
    return out;
}

}
