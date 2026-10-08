#include "MediaSession.hpp"

#if __has_include(<winrt/Windows.Media.Control.h>)

#include "core/Log.hpp"

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>

#include <optional>

namespace mediasession {

namespace {

using namespace winrt::Windows::Media::Control;

thread_local std::optional<GlobalSystemMediaTransportControlsSessionManager> manager;
thread_local bool reported = false;

}

bool list(std::vector<Session>& out) {
    try {
        if (!manager) manager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
        out.clear();
        auto active = manager->GetCurrentSession();
        for (auto s : manager->GetSessions()) {
            Session session;
            try {
                session.app = winrt::to_string(s.SourceAppUserModelId());
                session.current = active && s == active;
                auto info = s.GetPlaybackInfo();
                session.playing = info && info.PlaybackStatus() == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing;
                auto props = s.TryGetMediaPropertiesAsync().get();
                if (props) {
                    session.title = winrt::to_string(props.Title());
                    session.artist = winrt::to_string(props.Artist());
                }
            } catch (...) {
                // A session can disappear or have no properties between two tracks.
            }
            if (!session.app.empty()) out.push_back(std::move(session));
        }
        if (!reported) {
            reported = true;
            logger::info("music: windows media sessions ({} found)", out.size());
        }
        return true;
    } catch (...) {
        manager.reset();
        if (!reported) {
            reported = true;
            logger::warn("music: windows media sessions are not available, using window titles");
        }
        return false;
    }
}

void reset() { manager.reset(); }

}

#else

namespace mediasession {

bool list(std::vector<Session>&) { return false; }

void reset() {}

}

#endif
