#!/usr/bin/env bash
# Melee Party as a fleet job on arhum's PC (see tools/fleet/README.md): builds the checkout,
# runs what does not need the discs, then the Mario Party 4 minigames headless and captured if
# the discs are mounted. Results go to fleet-out/: report.md, the logs and the capture montages.
#
#   tools/fleet/job.sh [game id ...]        (default: every MP4 minigame in the list below)
#
# Environment:
#   MELEE_ISO_DIR   where the discs are (melee.iso and mp4.iso; /private on the PC, mounted read-only
#                   by the request's "private_files"); without it, no game runs. Nothing under it
#                   may come back: the outputs are the checkout's fleet-out alone
#   FLEET_JOBS      build parallelism (default: nproc)
#   FLEET_FRAMES    frames per headless game (default 9000) and per capture (default 1300)
#   FLEET_CAPTURE_EVERY  capture every N game frames (default 50); FLEET_NO_CAPTURE=1 skips them
#   FLEET_GPU=0     keep Wine's own D3D11 on OpenGL instead of DXVK on Vulkan
set -uo pipefail
repo="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$repo"
out="$repo/fleet-out"; mkdir -p "$out"
report="$out/report.md"
jobs="${FLEET_JOBS:-$(nproc)}"
iso_dir="${MELEE_ISO_DIR:-}"
games=("$@")
[ ${#games[@]} -gt 0 ] || games=(bigger-blast chomp-fever blizzard-brigade booksquirm butterfly-blitz trace-race candlelight-flight money-belts hop-or-pop cheep-cheep-sweep team-treasure-trek challenge-booksquirm paths-of-peril stamp-out)
t0=$(date +%s)
# on the fleet's plain Debian image: the toolchain first (tools/fleet/setup.sh, a no-op on the built image)
if ! command -v x86_64-w64-mingw32-gcc > /dev/null 2>&1; then
  "$repo/tools/fleet/setup.sh" > "$out/setup.log" 2>&1 || { echo "toolchain setup failed (fleet-out/setup.log)"; tail -20 "$out/setup.log"; exit 1; }
fi
say() { printf '%s\n' "$*" | tee -a "$report"; }
step() { say ""; say "## $* ($(( $(date +%s) - t0 )) s)"; }
fail=0

: > "$report"
say "# Melee Party fleet job: $(git rev-parse --short HEAD) ($(git log -1 --format=%s | cut -c1-80))"
say "host: $(nproc) cores, $(free -g | awk '/Mem:/{print $2}') GB, $(uname -r); GPU: $(ls /dev/dri 2>/dev/null | tr '\n' ' ')"

step "downloads"
mkdir -p "$HOME/.cache"
if [ ! -d "$HOME/.cache/xwin/crt" ]; then
  ( cd /tmp && curl -sL https://github.com/Jake-Shadle/xwin/releases/download/0.10.0/xwin-0.10.0-x86_64-unknown-linux-musl.tar.gz | tar xz \
    && ./xwin-0.10.0-x86_64-unknown-linux-musl/xwin --accept-license --cache-dir /tmp/xwin-dl --arch x86_64 splat --output "$HOME/.cache/xwin" > "$out/xwin.log" 2>&1 \
    && rm -rf /tmp/xwin-dl /tmp/xwin-0.10.0-* ) || { say "xwin failed (see xwin.log)"; fail=1; }
fi
if [ ! -x "$HOME/.cache/dxc/bin/dxc" ]; then
  mkdir -p "$HOME/.cache/dxc" && curl -sL https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.9.2609/linux_dxc_2026_09_28.x86_x64.tar.gz | tar xz -C "$HOME/.cache/dxc" || { say "dxc download failed"; fail=1; }
fi
say "xwin: $(du -sh "$HOME/.cache/xwin" 2>/dev/null | cut -f1), dxc: $("$HOME/.cache/dxc/bin/dxc" --version 2>/dev/null | head -1)"

step "game library (MinGW GCC, -j$jobs)"
python3 tools/prepare_native_sources.py > "$out/prepare.log" 2>&1 || { say "prepare_native_sources failed"; fail=1; }
cmake -S sourceport/game -B build-sourceport-gcc -DCMAKE_TOOLCHAIN_FILE="$PWD/sourceport/cmake/mingw-w64-x86_64-linux.cmake" \
  -DMELEE_CPU_BASELINE=AVX -DMELEE_PYTHON=python3 > "$out/configure-game.log" 2>&1 || { say "configure failed (configure-game.log)"; fail=1; }
if cmake --build build-sourceport-gcc --target melee_game -j"$jobs" > "$out/build-game.log" 2>&1; then
  say "melee_game.dll: $(stat -c %s build-sourceport-gcc/melee_game.dll) bytes"
else
  say "BUILD FAILED (build-game.log):"; grep -E ' error|undefined reference' "$out/build-game.log" | head -20 | tee -a "$report"; fail=1
fi

step "host (clang-cl, -j$jobs)"
python3 port/recomp/recomp.py --host-only --out build-clangcl/generated-host > "$out/recomp.log" 2>&1 || { say "recomp failed"; fail=1; }
cmake -S . -B build-clangcl -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="$PWD/port/cmake/clang-cl-xwin.cmake" \
  -DMELEE_BUILD_EXPERIMENTAL_PORT=ON -DMELEE_CPU_BASELINE=AVX -DPORT_GEN="$PWD/build-clangcl/generated-host" \
  -DMELEE_DXC_EXECUTABLE="$HOME/.cache/dxc/bin/dxc" > "$out/configure-host.log" 2>&1 || { say "host configure failed"; fail=1; }
if ninja -C build-clangcl -j"$jobs" melee_source > "$out/build-host.log" 2>&1; then
  say "melee_source.exe: $(stat -c %s build-clangcl/port/melee_source.exe) bytes"
else
  say "HOST BUILD FAILED (build-host.log):"; grep -E 'error' "$out/build-host.log" | head -20 | tee -a "$report"; fail=1
fi
cp build-sourceport-gcc/melee_game.dll build-sourceport-gcc/melee_game.snapexcl build-clangcl/port/ 2>/dev/null
[ $fail = 0 ] || { say ""; say "stopped after the build failures"; exit 1; }

step "wine prefix"
export WINEPREFIX="$HOME/.cache/wine-melee" WINEDEBUG=-all WINEARCH=win64
export WINEDLLOVERRIDES="d3dcompiler_47=n"
xvfb-run -a wineboot -i > "$out/wine.log" 2>&1
curl -sL -o /tmp/winetricks https://raw.githubusercontent.com/Winetricks/winetricks/master/src/winetricks && chmod +x /tmp/winetricks
xvfb-run -a /tmp/winetricks -q d3dcompiler_47 >> "$out/wine.log" 2>&1 || say "winetricks d3dcompiler_47 failed (wine.log)"
overrides="d3dcompiler_47=n"
if [ "${FLEET_GPU:-1}" != 0 ]; then
  # DXVK: D3D11 on Vulkan, so the Radeon renders (Wine's own D3D11 would go through Mesa's OpenGL)
  url=$(curl -s https://api.github.com/repos/doitsujin/dxvk/releases/latest | grep -o 'https://[^"]*dxvk-[0-9.]*\.tar\.gz' | head -1)
  if [ -n "$url" ] && curl -sL "$url" | tar xz -C /tmp; then
    cp /tmp/dxvk-*/x64/{d3d11,dxgi,d3d10core}.dll "$WINEPREFIX/drive_c/windows/system32/" && overrides="$overrides;d3d11,dxgi,d3d10core=n"
    say "dxvk: $(basename "$url")"
  else
    say "dxvk download failed: Wine's D3D11 on OpenGL"
  fi
fi
export WINEDLLOVERRIDES="$overrides"
say "vulkan: $(xvfb-run -a vulkaninfo --summary 2>/dev/null | grep -m1 deviceName || echo 'no device')"

step "tests without the discs"
xvfb-run -a ninja -C build-clangcl -j"$jobs" port_gpu_resource_test > /dev/null 2>&1 || true
( cd build-clangcl && xvfb-run -a ctest -R 'port_gpu' --timeout 300 > "$out/ctest-gpu.log" 2>&1 ) && say "GPU resource test: passed" || say "GPU resource test: failed or absent (ctest-gpu.log)"

if [ -z "$iso_dir" ] || [ ! -f "$iso_dir/melee.iso" ] || [ ! -f "$iso_dir/mp4.iso" ]; then
  say ""; say "no discs at '${iso_dir:-MELEE_ISO_DIR unset}': no game runs"; exit $fail
fi
export MELEE_ISO="$iso_dir/melee.iso"
run_game() { # id mode frames extra...
  local id=$1 mode=$2 frames=$3; shift 3
  MELEE_PARTY_BOOT=1 MELEE_PARTY_SKIP_CSS=1 MELEE_PARTY_ALL_CPU=1 MELEE_PARTY_TURNS=1 MELEE_PARTY_SEED=1 MELEE_PARTY_AUTO_ROLL=1 \
  MELEE_FRESH_CARD=1 MELEE_PARTY_MINIGAME="$id" MELEE_PARTY_MP4_LOG=1 \
    timeout 1500 tools/wine_party.sh $mode --frames "$frames" --script "$PWD/port/scripts/party_boot.txt" --mp4-iso "$iso_dir/mp4.iso" "$@" > /dev/null 2>&1
  WINEPREFIX="$WINEPREFIX" wineserver -k 2>/dev/null; sleep 2
}
for g in "${games[@]}"; do
  step "$g headless"
  run_game "$g" "--headless --fast" "${FLEET_FRAMES:-9000}"
  log=build-clangcl/port/melee_port.log; cp "$log" "$out/$g-headless.log"
  grep -E 'places|game crash|panic|CRASH|heaps leave' "$log" | cut -c1-160 | tee -a "$report"
  grep -q 'places' "$log" || { say "$g: no game to the end"; fail=1; }
  [ "${FLEET_NO_CAPTURE:-0}" = 1 ] && continue
  step "$g capture"
  mkdir -p "$out/cap-$g"; rm -f "$out/cap-$g"/*
  run_game "$g" "--hidden --fast --window 640x480 --scale 1" "${FLEET_CAPTURE_FRAMES:-1300}" --capture "$out/cap-$g/f.ppm" --capture-every "${FLEET_CAPTURE_EVERY:-50}"
  cp build-clangcl/port/melee_port.log "$out/$g-capture.log"
  n=$(ls "$out/cap-$g"/f_*.ppm 2>/dev/null | wc -l)
  if [ "$n" -gt 0 ]; then
    ( cd "$out/cap-$g" && montage -label '%t' $(ls f_*.ppm | tail -16) -tile 4x -geometry 320x240+4+4 "../$g-montage.png" && rm -f f_*.ppm )
    say "$g: $n frames, montage $g-montage.png"
  else
    say "$g: no frames captured"; fail=1
  fi
done
say ""; say "done in $(( $(date +%s) - t0 )) s, ${fail:+failures: }$fail"
exit $fail
