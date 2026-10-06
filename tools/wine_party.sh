#!/usr/bin/env bash
# Runs the Linux cross-built Source Port (melee_source.exe) under Wine on a virtual display.
# See docs/agent-testing.md for the build this expects and for the MELEE_PARTY_* knobs.
#
#   tools/wine_party.sh [melee_source options...]
#
# Environment:
#   MELEE_ISO      the NTSC 1.02 ISO (default ~/.local/share/melee/melee.iso)
#   MELEE_EXE_DIR  folder holding melee_source.exe and melee_game.dll (default build-clangcl/port)
#   WINEPREFIX     default ~/.cache/wine-melee (needs the native d3dcompiler_47, see the doc)
#   MELEE_FRESH_CARD=1  delete the memory card first (User/ beside the exe)
# Paths in the options may be Linux paths: anything starting with / is passed as Z:\... to Wine.
set -euo pipefail
repo="$(cd "$(dirname "$0")/.." && pwd)"
exe_dir="${MELEE_EXE_DIR:-$repo/build-clangcl/port}"
iso="${MELEE_ISO:-$HOME/.local/share/melee/melee.iso}"
export WINEPREFIX="${WINEPREFIX:-$HOME/.cache/wine-melee}"
export WINEDEBUG="${WINEDEBUG:--all}"
export WINEDLLOVERRIDES="${WINEDLLOVERRIDES:-d3dcompiler_47=n}"

winpath() { case "$1" in /*) printf 'Z:%s' "${1//\//\\}" ;; *) printf '%s' "$1" ;; esac; }

[ -f "$exe_dir/melee_source.exe" ] || { echo "no $exe_dir/melee_source.exe (build it first)" >&2; exit 2; }
[ -f "$exe_dir/melee_game.dll" ] || { echo "no $exe_dir/melee_game.dll (copy it from build-sourceport-gcc)" >&2; exit 2; }
[ -f "$iso" ] || { echo "no ISO at $iso (set MELEE_ISO)" >&2; exit 2; }
[ "${MELEE_FRESH_CARD:-0}" = 1 ] && rm -rf "$exe_dir/User"

args=()
for a in "$@"; do args+=("$(winpath "$a")"); done
cd "$exe_dir"
rm -f melee_port.log
exec xvfb-run -a -s "-screen 0 1280x1024x24" \
  wine melee_source.exe --iso "$(winpath "$iso")" --backend d3d11 --slippi-menus off --volume 0 "${args[@]}"
