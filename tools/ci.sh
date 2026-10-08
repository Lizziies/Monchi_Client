#!/usr/bin/env bash
# Build, self-test and menu screenshots under Wine, used by .github/workflows/check.yml.
# usage: tools/ci.sh [outdir]
set -uo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
out="${1:-$root/ci-out}"
build="${MONCHI_BUILD:-/tmp/monchi-build}"
export MONCHI_BUILD="$build"
export PATH="$PATH:/usr/lib/wine"
export WINEPREFIX="${WINEPREFIX:-$HOME/.wine-monchi}" WINEDEBUG=-all WINEARCH=win64
mkdir -p "$out" "$WINEPREFIX"

if ! "$root/tools/cross.sh" build > "$out/build.log" 2>&1; then
    grep -E "error|Error" "$out/build.log" | head -40 > "$out/errors.txt"
    echo "build failed"
    exit 1
fi

mkdir -p "$out/bin"
cp "$build/dll/Monchi.dll" "$build/launcher/MonchiLauncher.exe" "$out/bin/" 2>/dev/null || true

(Xvfb :77 -screen 0 1280x720x24 >/dev/null 2>&1 &)
sleep 2
export DISPLAY=:77
timeout 180 wine64 wineboot -i >/dev/null 2>&1 || true
sleep 3

data() { ls -d "$WINEPREFIX"/drive_c/users/*/AppData/Local/Monchi 2>/dev/null | head -1; }

cd "$build"
if [ -n "${MONCHI_CI_SELFTEST:-}" ]; then
    MONCHI_SELFTEST=1 timeout 420 wine64 testhost.exe dll/Monchi.dll 360 > "$out/selftest-host.txt" 2>&1 || true
    cp "$(data)/logs/latest.log" "$out/selftest.log" 2>/dev/null || true
    grep -E "selftest|\[error\]" "$out/selftest.log" 2>/dev/null | tail -60 > "$out/selftest-summary.txt" || true
fi

# each step: seconds:kind:a,b  (k = key, c = click, w = wheel, t = char)
script="${MONCHI_CI_SCRIPT:-}"
shots="${MONCHI_CI_SHOTS:-4 7 10 13 16 19 22 25}"
rm -rf "$(data)/configs" "$(data)/logs/latest.log"
(TESTHOST_MANUAL=1 TESTHOST_SCRIPT="$script" timeout 120 wine64 testhost.exe dll/Monchi.dll 40 > "$out/menu-host.txt" 2>&1 &)
# the host's script clock starts when the process does, which is about when the client logs "ready"
for i in $(seq 1 600); do
    grep -q "\] \[info\] ready" "$(data)/logs/latest.log" 2>/dev/null && break
    sleep 0.1
done
start=$(date +%s)
for t in $shots; do
    while [ $(( $(date +%s) - start )) -lt "$t" ]; do sleep 0.2; done
    import -window root "$out/menu-$t.png"
done
sleep 6
cp "$(data)/logs/latest.log" "$out/menu.log" 2>/dev/null || true
wineserver -k 2>/dev/null

# frame rate over time: the bare host, the client with the menu closed, the client with the menu open
TESTHOST_MANUAL=1 timeout 60 wine64 testhost.exe none.dll 30 > "$out/perf-bare.txt" 2>&1
TESTHOST_MANUAL=1 timeout 60 wine64 testhost.exe dll/Monchi.dll 30 > "$out/perf-client.txt" 2>&1
TESTHOST_MANUAL=1 TESTHOST_SCRIPT="3:k:161" timeout 60 wine64 testhost.exe dll/Monchi.dll 30 > "$out/perf-menu.txt" 2>&1
wineserver -k 2>/dev/null

# fresh starts: config creation, the first autosave and the background shader compile all fall into the first seconds
crashes=0
for i in 1 2 3 4 5; do
    rm -rf "$(data)/configs"
    TESTHOST_MANUAL=1 timeout 40 wine64 testhost.exe dll/Monchi.dll 12 > "$out/fresh-$i.txt" 2>&1
    grep -q "Unhandled" "$out/fresh-$i.txt" && crashes=$((crashes + 1))
    wineserver -k 2>/dev/null
done
echo "$crashes of 5 fresh starts crashed" > "$out/fresh.txt"

[ -n "${MONCHI_CI_TOUR:-}" ] && "$root/tools/tour.sh" "$out/tour"
echo "done"
