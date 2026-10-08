#!/usr/bin/env bash
# Screenshots of every menu page at 1080p with demo data, and of the launcher. Needs a build from cross.sh.
# usage: tools/tour.sh [outdir]
set -uo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
dir="${1:-$root/tour}"
build="${MONCHI_BUILD:-/tmp/monchi-build}"
export PATH="$PATH:/usr/lib/wine"
export WINEPREFIX="${WINEPREFIX:-$HOME/.wine-monchi}" WINEDEBUG=-all WINEARCH=win64
mkdir -p "$dir"
wineserver -k 2>/dev/null

# the test window sits at 0,0 with a 4 px frame and a 22 px title bar around its client area
(Xvfb :78 -screen 0 1936x1110x24 >/dev/null 2>&1 &)
sleep 2
export DISPLAY=:78

data() { ls -d "$WINEPREFIX"/drive_c/users/*/AppData/Local/Monchi 2>/dev/null | head -1; }
log() { echo "$(data)/logs/latest.log"; }

[ -n "$(data)" ] || { echo "no Monchi data folder, run the client once first"; exit 1; }
python3 - "$(data)" <<'PY'
import json, os, sys
data = sys.argv[1]
on = ["Keystrokes", "CPS", "FPS", "Armor HUD", "Potion HUD", "Coordinates", "Reach Counter", "Combo Counter",
      "Ping Counter", "Clock", "Toggle Sprint", "Low Latency", "Custom Crosshair", "Block Outline"]
mods = {n: {"enabled": True, "settings": {}} for n in on}
mods["Game Support"] = {"enabled": True, "settings": {"demo": True}}
mods["Client Settings"] = {"enabled": True, "settings": {"cosmetics": "sakura_wings,monchi_cape"}}
os.makedirs(os.path.join(data, "configs"), exist_ok=True)
json.dump({"modules": mods}, open(os.path.join(data, "configs", "default.json"), "w"))
PY
rm -rf "$(data)/cosmetics"
cp -r "$root/cosmetics" "$(data)/cosmetics"
touch "$build/dll/Monchi.root"
rm -f "$(log)"

cd "$build"
(TESTHOST_MANUAL=1 TESTHOST_SIZE=1920x1080 timeout 300 wine64 testhost.exe dll/Monchi.dll 240 > "$dir/host.txt" 2>&1 &)
for i in $(seq 1 600); do
    grep -q "\] \[info\] ready" "$(log)" 2>/dev/null && break
    sleep 0.1
done
sleep 4

shot() { import -window root -crop 1920x1080+4+22 +repage "$dir/$1.png"; }

# step name command...: hands the commands to the client and waits until it has run the last one
step() {
    local name="$1"
    shift
    local last="${*: -1}"
    local before
    before=$(grep -cF "dev command: $last" "$(log)")
    printf '%s\n' "$@" > "$(data)/dev.cmd"
    for i in $(seq 1 150); do
        [ "$(grep -cF "dev command: $last" "$(log)")" -gt "$before" ] && break
        sleep 0.1
    done
    sleep 1.6
    shot "$name"
}

shot 01-im-spiel
step 02-menue-offen "open"
step 03-pvp-modul "module Hitbox"
step 04-gruppe-combat-info "module Reach Counter"
step 05-gruppe-keystrokes "module CPS"
step 06-visual "module Zoom"
step 07-nuetzliches "module Auto GG"
step 08-leistung "module Frame Limiter"
step 09-server "module Hive Utils"
step 10-suche "search reach"
step 11-global-general "search" "settings 0"
step 12-global-chat "settings 1"
step 13-global-appearance "settings 2"
step 14-global-modules "settings 3"
step 15-global-profiles "settings 4"
step 16-global-about "settings 5"
step 17-cosmetics "page cosmetics"
step 18-hud-editor "hudedit"
step 19-zu "close"
if command -v xdotool >/dev/null; then
    # the fourth accent dot (purple) on the Appearance tab, in screen coordinates of the test window
    step 23-akzent-blau "settings 2"
    xdotool mousemove 906 294 click 1
    sleep 1.2
    shot 24-akzent-lila
    step 25-akzent-lila-liste "module Reach Counter"
fi
cp "$(log)" "$dir/client.log"
wineserver -k 2>/dev/null
rm -f "$build/dll/Monchi.root"
rm -rf "$(data)/configs"

# the launcher opens centred, 960x600 on a 1920x1080 screen
(Xvfb :79 -screen 0 1920x1080x24 >/dev/null 2>&1 &)
sleep 2
export DISPLAY=:79
exe="$(ls "$build"/launcher/MonchiLauncher.exe 2>/dev/null | head -1)"
[ -n "$exe" ] || exit 0
(timeout 60 wine64 "$exe" > "$dir/launcher.txt" 2>&1 &)
sleep 10
lshot() { import -window root -crop 960x600+480+240 +repage "$dir/$1.png"; }
lshot 20-launcher-start
if command -v xdotool >/dev/null; then
    xdotool mousemove 596 434 click 1
    sleep 1.5
    lshot 21-launcher-versionen
    xdotool mousemove 596 486 click 1
    sleep 1.5
    lshot 22-launcher-einstellungen
    xdotool mousemove 1264 451 click 1
    sleep 1
    lshot 22b-launcher-lila
    xdotool mousemove 596 382 click 1
    sleep 1.5
    lshot 22c-launcher-start-lila
fi
wineserver -k 2>/dev/null
echo "tour done"
