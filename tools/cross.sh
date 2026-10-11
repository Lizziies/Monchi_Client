#!/usr/bin/env bash
# Cross-build the DLL/launcher with MinGW and run the DLL in the Wine test host.
# usage: tools/cross.sh setup | build | shots [outdir] | tour [outdir]
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
out="${MONCHI_BUILD:-/tmp/monchi-build}"
tc="$out/mingw.cmake"
export PATH="$PATH:/usr/lib/wine"
export WINEPREFIX="${WINEPREFIX:-/tmp/wineprefix}" WINEDEBUG=-all WINEARCH=win64

setup() {
    apt-get install -y --no-install-recommends cmake g++-mingw-w64-x86-64-posix wine64 xvfb imagemagick
}

toolchain() {
    mkdir -p "$out"
    cat > "$tc" <<T
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc-posix)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++-posix)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
T
}

build() {
    toolchain
    cmake -S "$root/dll" -B "$out/dll" -DCMAKE_TOOLCHAIN_FILE="$tc" -DCMAKE_BUILD_TYPE=Release >/dev/null
    cmake --build "$out/dll" -j"$(nproc)"
    cmake -S "$root/launcher" -B "$out/launcher" -DCMAKE_TOOLCHAIN_FILE="$tc" -DCMAKE_BUILD_TYPE=Release \
        -DMONCHI_DLL="$out/dll/Monchi.dll" -DMONCHI_COSMETICS="$root/cosmetics" >/dev/null
    cmake --build "$out/launcher" -j"$(nproc)"
    x86_64-w64-mingw32-g++-posix -std=c++20 -O1 -municode -static -static-libgcc -static-libstdc++ \
        "$root/tools/testhost/main.cpp" -o "$out/testhost.exe" -ld3d11 -ldxgi -luser32
}

shots() {
    local dir="${1:-$out/shots}"
    mkdir -p "$dir"
    (Xvfb :77 -screen 0 1280x720x24 >/dev/null 2>&1 &)
    sleep 2
    export DISPLAY=:77
    cd "$out"
    (wine64 testhost.exe dll/Monchi.dll 22 > "$dir/log.txt" 2>&1 &)
    for i in $(seq 1 14); do
        sleep 2
        import -window root "$dir/f$i.png"
    done
    cat "$dir/log.txt"
}

tour() {
    local dir="${1:-$out/tour}"
    mkdir -p "$dir"
    (Xvfb :77 -screen 0 1280x720x24 >/dev/null 2>&1 &)
    sleep 2
    export DISPLAY=:77
    cd "$out"
    local data
    data="$(ls -d "$WINEPREFIX"/drive_c/users/*/AppData/Local/Monchi | head -1)"
    rm -rf "$data/configs"
    wine64 testhost.exe dll/Monchi.dll 3 >/dev/null 2>&1 || true
    python3 - "$data" <<'PY'
import json, os, re, sys
data = sys.argv[1]
log = open(os.path.join(data, "logs", "latest.log")).read()
names = [n.strip() for k in ("usable", "locked") for n in re.search(r"\] \[info\] %s: (.*)" % k, log).group(1).split(";") if n.strip()]
os.makedirs(os.path.join(data, "configs"), exist_ok=True)
mods = {n: {"enabled": n != "Lua Scripts", "settings": {}} for n in names}
mods["Game Support"]["settings"]["demo"] = True
json.dump({"modules": mods}, open(os.path.join(data, "configs", "default.json"), "w"))
PY
    (TESTHOST_MANUAL=1 wine64 testhost.exe dll/Monchi.dll 16 > "$dir/log.txt" 2>&1 &)
    sleep 12
    import -window root "$dir/all.png"
    sleep 6
    grep -E "\[(error|warn)\]" "$data/logs/latest.log" | grep -v "power throttling" || echo "no errors in the log"
    rm -rf "$data/configs"
}

"${1:?usage: cross.sh setup|build|shots|tour}" "${@:2}"
