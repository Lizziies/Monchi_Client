#include "Link.hpp"

#include "I18n.hpp"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <wlanapi.h>

#include <cstdio>
#include <format>
#include <vector>

namespace netlink {

using BestInterfaceFn = decltype(&GetBestInterfaceEx);
using AdaptersFn = decltype(&GetAdaptersAddresses);
using WlanOpenFn = decltype(&WlanOpenHandle);
using WlanCloseFn = decltype(&WlanCloseHandle);
using WlanEnumFn = decltype(&WlanEnumInterfaces);
using WlanQueryFn = decltype(&WlanQueryInterface);
using WlanFreeFn = decltype(&WlanFreeMemory);

template <class F>
static F load(HMODULE m, const char* name) {
    return m ? reinterpret_cast<F>(reinterpret_cast<void*>(GetProcAddress(m, name))) : nullptr;
}

static std::string narrow(const wchar_t* w) {
    if (!w || !*w) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) return {};
    std::string s(n - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
    return s;
}

static std::string guidText(const GUID& g) {
    return std::format("{{{:08X}-{:04X}-{:04X}-{:02X}{:02X}-{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}}}", g.Data1, g.Data2,
                       g.Data3, g.Data4[0], g.Data4[1], g.Data4[2], g.Data4[3], g.Data4[4], g.Data4[5], g.Data4[6],
                       g.Data4[7]);
}

static const char* standardName(int phy) {
    switch (phy) {
    case 4: return "802.11a";
    case 5: return "802.11b";
    case 6: return "802.11g";
    case 7: return "802.11n";
    case 8: return "802.11ac";
    case 10: return "802.11ax";
    case 11: return "802.11be";
    }
    return "";
}

static std::string bandOf(int channel) {
    if (channel <= 0) return {};
    if (channel <= 14) return i18n::tr("2.4 GHz");
    return "5/6 GHz";
}

static int powerSaving(const std::string& adapterGuid) {
    HKEY cls;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e972-e325-11ce-bfc1-08002be10318}",
                      0, KEY_READ, &cls) != ERROR_SUCCESS)
        return -1;

    int result = -1;
    char name[64];
    for (DWORD i = 0;; i++) {
        DWORD len = sizeof(name);
        if (RegEnumKeyExA(cls, i, name, &len, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
        HKEY sub;
        if (RegOpenKeyExA(cls, name, 0, KEY_READ, &sub) != ERROR_SUCCESS) continue;

        char id[80]{};
        DWORD size = sizeof(id) - 1, type = 0;
        bool match = RegQueryValueExA(sub, "NetCfgInstanceId", nullptr, &type, reinterpret_cast<BYTE*>(id), &size) == ERROR_SUCCESS &&
                     _stricmp(id, adapterGuid.c_str()) == 0;
        if (match) {
            DWORD caps = 0;
            size = sizeof(caps);
            bool have = RegQueryValueExA(sub, "PnPCapabilities", nullptr, &type, reinterpret_cast<BYTE*>(&caps), &size) == ERROR_SUCCESS;
            result = (!have || !(caps & 0x10)) ? 1 : 0;
        }
        RegCloseKey(sub);
        if (match) break;
    }
    RegCloseKey(cls);
    return result;
}

static void queryWifi(const std::string& adapterGuid, probe::Link& out) {
    HMODULE lib = LoadLibraryW(L"wlanapi.dll");
    if (!lib) return;
    auto open = load<WlanOpenFn>(lib, "WlanOpenHandle");
    auto close = load<WlanCloseFn>(lib, "WlanCloseHandle");
    auto enumerate = load<WlanEnumFn>(lib, "WlanEnumInterfaces");
    auto query = load<WlanQueryFn>(lib, "WlanQueryInterface");
    auto release = load<WlanFreeFn>(lib, "WlanFreeMemory");
    if (!open || !close || !enumerate || !query || !release) {
        FreeLibrary(lib);
        return;
    }

    HANDLE h = nullptr;
    DWORD ver = 0;
    if (open(2, nullptr, &ver, &h) == ERROR_SUCCESS) {
        PWLAN_INTERFACE_INFO_LIST list = nullptr;
        if (enumerate(h, nullptr, &list) == ERROR_SUCCESS && list) {
            for (DWORD i = 0; i < list->dwNumberOfItems; i++) {
                auto& info = list->InterfaceInfo[i];
                if (info.isState != wlan_interface_state_connected) continue;
                if (!adapterGuid.empty() && _stricmp(guidText(info.InterfaceGuid).c_str(), adapterGuid.c_str()) != 0) continue;

                DWORD size = 0;
                PWLAN_CONNECTION_ATTRIBUTES attr = nullptr;
                if (query(h, &info.InterfaceGuid, wlan_intf_opcode_current_connection, nullptr, &size,
                          reinterpret_cast<PVOID*>(&attr), nullptr) == ERROR_SUCCESS && attr) {
                    auto& a = attr->wlanAssociationAttributes;
                    out.ssid.assign(reinterpret_cast<const char*>(a.dot11Ssid.ucSSID), a.dot11Ssid.uSSIDLength);
                    out.signal = (int)a.wlanSignalQuality;
                    out.rssi = int(a.wlanSignalQuality / 2) - 100;
                    out.rxMbps = int(a.ulRxRate / 1000);
                    out.txMbps = int(a.ulTxRate / 1000);
                    out.standard = standardName((int)a.dot11PhyType);
                    release(attr);
                }

                ULONG* channel = nullptr;
                size = 0;
                if (query(h, &info.InterfaceGuid, wlan_intf_opcode_channel_number, nullptr, &size,
                          reinterpret_cast<PVOID*>(&channel), nullptr) == ERROR_SUCCESS && channel) {
                    out.channel = (int)*channel;
                    out.band = bandOf(out.channel);
                    release(channel);
                }
                break;
            }
            release(list);
        }
        close(h, nullptr);
    }
    FreeLibrary(lib);
}

probe::Link query(const std::string& targetIp) {
    probe::Link out;
    HMODULE ip = LoadLibraryW(L"iphlpapi.dll");
    auto best = load<BestInterfaceFn>(ip, "GetBestInterfaceEx");
    auto adapters = load<AdaptersFn>(ip, "GetAdaptersAddresses");
    if (!best || !adapters) return out;

    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    if (inet_pton(AF_INET, targetIp.c_str(), &dest.sin_addr) != 1) inet_pton(AF_INET, "1.1.1.1", &dest.sin_addr);

    DWORD index = 0;
    if (best(reinterpret_cast<sockaddr*>(&dest), &index) != NO_ERROR) return out;

    ULONG size = 0;
    constexpr ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    adapters(AF_UNSPEC, flags, nullptr, nullptr, &size);
    std::vector<char> buf(size + 1024);
    auto* first = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data());
    if (adapters(AF_UNSPEC, flags, nullptr, first, &size) != NO_ERROR) return out;

    for (auto* a = first; a; a = a->Next) {
        if (a->IfIndex != index && a->Ipv6IfIndex != index) continue;
        out.adapter = narrow(a->FriendlyName);
        out.linkMbps = int(a->TransmitLinkSpeed / 1000000ull);
        std::string guid = a->AdapterName ? a->AdapterName : "";
        if (a->IfType == IF_TYPE_IEEE80211) {
            out.kind = probe::LinkKind::Wifi;
            queryWifi(guid, out);
        } else if (a->IfType == IF_TYPE_ETHERNET_CSMACD) {
            out.kind = probe::LinkKind::Wired;
        } else {
            out.kind = probe::LinkKind::Other;
        }
        out.powerSaving = powerSaving(guid);
        break;
    }
    return out;
}

}
