#!/usr/bin/env bash
# builds tools/cosmetics/native/render against the client's cosmetics code
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
dll="$here/../../../dll"
g++ -std=c++20 -O2 -DIMGUI_DEFINE_MATH_OPERATORS -I"$here/stub" -I"$dll/src" -I"$dll/lib/imgui" -I"$dll/lib/json" -I"$dll/lib/stb" \
    "$here/render.cpp" "$dll/src/cosmetics/Cosmetics.cpp" "$dll/src/cosmetics/Preview.cpp" \
    "$dll/lib/imgui/imgui.cpp" "$dll/lib/imgui/imgui_draw.cpp" "$dll/lib/imgui/imgui_tables.cpp" "$dll/lib/imgui/imgui_widgets.cpp" \
    -o "${1:-$here/render}"
