#include "system/MusicState.hpp"
#include <cstdlib>

void check(bool ok) { if (!ok) std::abort(); }

int main() {
    using namespace music;
    TrackState state;
    Track first{"Amazon Music", "Artist - First", true, true};
    Track second{"Amazon Music", "Artist - Second", true, true};
    check(state.update(first, 100).title == first.title);
    check(state.update({"Amazon Music", "GID Amazon Music.exe", true, true}, 350).title == first.title);
    check(state.update({}, 600).title == first.title);
    check(state.update(second, 850).title == second.title);
    check(state.update({"Amazon Music", "", false, true}, 1100).playing == false);
    check(state.update({"Spotify", "", true, true}, 1200).player == "Spotify");
    check(state.update(first, 2000).title == first.title);
    check(state.update({"Amazon Music", "", true, true}, 3501).title.empty());
    check(state.update(second, 3750).title == second.title);
    check(state.update({}, 5251).found == false);
    check(!songTitle("Amazon Music.exe"));
    check(!songTitle("Default IME"));
    check(songTitle("Artist - Song.exe"));
}
