#pragma once

#include "Probe.hpp"

#include <format>
#include <string>
#include <vector>

namespace quality {

enum class Grade { Unknown, Good, Fair, Poor };

struct Limits {
    float pingFair = 60.f;
    float pingPoor = 150.f;
    float jitterFair = 6.f;
    float jitterPoor = 20.f;
    float lossPoor = 3.f;
};

struct Verdict {
    Grade grade = Grade::Unknown;
    std::string label = i18n::tr("No measurement");
    std::string reason;
    std::vector<std::string> tips;
};

inline Verdict judge(const probe::Snapshot& s, const Limits& l) {
    Verdict v;
    if (!s.running || !s.resolved || s.received == 0) {
        v.reason = s.sent > 0 ? i18n::tr("No reply from the server. Is the firewall blocking ICMP?") : "";
        return v;
    }

    bool wifi = s.link.kind == probe::LinkKind::Wifi;
    bool weakWifi = wifi && s.link.signal >= 0 && s.link.signal < 50;
    bool lossy = s.loss >= l.lossPoor;
    bool jittery = s.jitter >= l.jitterPoor;
    bool slow = s.avg >= l.pingPoor;

    if (lossy || jittery || slow) v.grade = Grade::Poor;
    else if (s.avg >= l.pingFair || s.jitter >= l.jitterFair || s.loss >= 0.5f) v.grade = Grade::Fair;
    else v.grade = Grade::Good;
    v.label = i18n::tr(v.grade == Grade::Good ? "Stable" : v.grade == Grade::Fair ? "Shaky" : "Poor");

    if (v.grade != Grade::Good) {
        if (weakWifi) v.reason = i18n::fmt("Weak Wi-Fi signal ({} %)", s.link.signal);
        else if (lossy && wifi) v.reason = i18n::fmt("{:.1f} % packet loss, probably radio interference", s.loss);
        else if (lossy) v.reason = i18n::fmt("{:.1f} % packet loss on the route", s.loss);
        else if (jittery && wifi) v.reason = i18n::tr("High jitter, probably radio interference");
        else if (jittery) v.reason = i18n::tr("High jitter, the line fluctuates");
        else if (slow) v.reason = i18n::fmt("High ping ({:.0f} ms), server far away or poor route", s.avg);
        else v.reason = i18n::tr("Slight fluctuations");
    }
    if (s.spikePeriod > 0.f)
        v.reason = i18n::fmt("Ping spikes every {:.0f} s, probably a Wi-Fi scan in the background", s.spikePeriod);

    if (wifi) {
        if (s.link.band == i18n::tr("2.4 GHz")) v.tips.push_back(i18n::tr("Switch to the 5 GHz band, 2.4 GHz is crowded"));
        if (weakWifi) v.tips.push_back(i18n::tr("Move closer to the router or use a LAN cable"));
        if (v.grade != Grade::Good) v.tips.push_back(i18n::tr("A LAN cable is the surest fix for jitter"));
        if (s.link.powerSaving == 1) v.tips.push_back(i18n::tr("Turn off power saving of the Wi-Fi adapter in the Device Manager"));
        if (s.spikePeriod > 0.f) v.tips.push_back(i18n::tr("Turn off the driver's automatic Wi-Fi scan"));
    } else if (v.grade != Grade::Good) {
        v.tips.push_back(i18n::tr("Pause background downloads and cloud sync"));
        if (lossy) v.tips.push_back(i18n::tr("Check cable and router, enable router QoS for games"));
    }
    return v;
}

}
