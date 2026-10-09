#!/usr/bin/env bash
# Melee Party as a fleet job (see tools/fleet/README.md): builds the checkout, then runs the
# Mario Party 4 minigames headless and captured if the discs are mounted. Results go to
# fleet-out/: report.md, the logs and the capture montages.
#
#   tools/fleet/job.sh [game id ...]        (default: every MP4 minigame in the list below)
#
# Environment:
#   MELEE_ISO_DIR   where the discs are (melee.iso and mp4.iso; /private on the fleet, mounted
#                   read-only by the request's "private_files"); without it, no game runs. Nothing
#                   under it may come back: the outputs are the checkout's fleet-out alone
#   FLEET_JOBS      build parallelism (default: the cores, at most one per GB of the job's memory);
#                   FLEET_HOST_JOBS for the C++ host, whose compilers need more (one per 1.5 GB)
#   FLEET_FRAMES    frames per headless game (default 9000)
#   FLEET_TIME_LIMIT  the request's time_limit_s (default 7200): the captures share what is left of
#                   it after the builds and the headless games, so every game gets a montage
#   FLEET_RENDER    how captures render: auto (default: the first that captures frames of DXVK,
#                   DXVK with Mesa's software present, and Wine's own D3D11 on OpenGL), dxvk,
#                   dxvk-sw or wined3d
#   FLEET_NO_CAPTURE=1  skips the captures
#   FLEET_BUILD_ONLY=1  stop after the two builds (a compile check on a CPU-only machine; see compile.json)
#
# /keep (the fleet's folder that stays on a machine between jobs) holds the downloads, the apt
# packages, the Wine prefix and a compiler cache, so a second job on the same machine skips most
# of the setup and the builds.
set -uo pipefail
repo="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$repo"
out="$repo/fleet-out"; mkdir -p "$out"
report="$out/report.md"
# a compiler per core, at most one per GB the job may use (the C++ host's at most one per 1.5 GB)
mem_gb=$(awk '{ print ($1 == "max") ? 64 : int($1 / 1073741824) }' /sys/fs/cgroup/memory.max 2>/dev/null || echo 64)
[ "${mem_gb:-0}" -ge 1 ] || mem_gb=1
jobs="${FLEET_JOBS:-$(( $(nproc) < mem_gb ? $(nproc) : mem_gb ))}"
host_jobs="${FLEET_HOST_JOBS:-$(( jobs < mem_gb * 2 / 3 ? jobs : (mem_gb * 2 / 3 > 0 ? mem_gb * 2 / 3 : 1) ))}"
iso_dir="${MELEE_ISO_DIR:-}"
games=("$@")
[ ${#games[@]} -gt 0 ] || games=(bigger-blast chomp-fever blizzard-brigade booksquirm butterfly-blitz trace-race candlelight-flight money-belts hop-or-pop cheep-cheep-sweep team-treasure-trek challenge-booksquirm paths-of-peril stamp-out)
t0=$(date +%s)
deadline=$(( t0 + ${FLEET_TIME_LIMIT:-7200} - 240 ))   # 4 minutes kept for packing the outputs
cache="$HOME/.cache"
if [ -d /keep ] && [ -w /keep ]; then cache=/keep/melee-party; fi
mkdir -p "$cache"
export FLEET_CACHE="$cache"
# on the fleet's plain Debian image: the toolchain first (tools/fleet/setup.sh, a no-op on the built image)
if ! command -v x86_64-w64-mingw32-gcc > /dev/null 2>&1; then
  "$repo/tools/fleet/setup.sh" > "$out/setup.log" 2>&1 || { echo "toolchain setup failed (fleet-out/setup.log)"; tail -20 "$out/setup.log"; exit 1; }
fi
say() { printf '%s\n' "$*" | tee -a "$report"; }
step() { say ""; say "## $* ($(( $(date +%s) - t0 )) s)"; }
fail=0

: > "$report"
say "# Melee Party fleet job: $(git rev-parse --short HEAD) ($(git log -1 --format=%s | cut -c1-80))"
say "host: ${FLEET_MACHINE:-?}, $(nproc) cores, $(free -g | awk '/Mem:/{print $2}') GB, $(uname -r); GPU: $(ls /dev/dri 2>/dev/null | tr '\n' ' '); cache $cache ($(du -sh "$cache" 2>/dev/null | cut -f1))"
say "setup: $(tail -1 "$out/setup.log" 2>/dev/null) ($(( $(date +%s) - t0 )) s)"

step "downloads"
# xwin moves its downloads into place, so they go on the same file system as the output; .done
# marks a whole splat (an interrupted one is started again)
if [ ! -f "$cache/xwin/.done" ]; then
  rm -rf "$cache/xwin" "$cache/xwin-dl"
  ( cd /tmp && curl -sL https://github.com/Jake-Shadle/xwin/releases/download/0.10.0/xwin-0.10.0-x86_64-unknown-linux-musl.tar.gz | tar xz \
    && ./xwin-0.10.0-x86_64-unknown-linux-musl/xwin --accept-license --cache-dir "$cache/xwin-dl" --arch x86_64 splat --output "$cache/xwin" > "$out/xwin.log" 2>&1 \
    && touch "$cache/xwin/.done" && rm -rf "$cache/xwin-dl" /tmp/xwin-0.10.0-* ) || { say "xwin failed (see xwin.log)"; fail=1; }
fi
if [ ! -x "$cache/dxc/bin/dxc" ]; then
  mkdir -p "$cache/dxc" && curl -sL https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.9.2609/linux_dxc_2026_09_28.x86_x64.tar.gz | tar xz -C "$cache/dxc" || { say "dxc download failed"; fail=1; }
fi
say "xwin: $(du -sh "$cache/xwin" 2>/dev/null | cut -f1), dxc: $("$cache/dxc/bin/dxc" --version 2>/dev/null | head -1)"

# a compiler cache in /keep: the checkout is new every job, so make and ninja rebuild everything,
# and ccache answers the files that did not change
launcher=()
if command -v ccache > /dev/null 2>&1; then
  export CCACHE_DIR="$cache/ccache" CCACHE_MAXSIZE=5G CCACHE_BASEDIR="$repo" CCACHE_NOHASHDIR=1
  launcher=(-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache)
  ccache -z > /dev/null 2>&1
fi

step "game library (MinGW GCC, -j$jobs)"
python3 tools/prepare_native_sources.py > "$out/prepare.log" 2>&1 || { say "prepare_native_sources failed"; fail=1; }
cmake -S sourceport/game -B build-sourceport-gcc -DCMAKE_TOOLCHAIN_FILE="$PWD/sourceport/cmake/mingw-w64-x86_64-linux.cmake" \
  -DMELEE_CPU_BASELINE=AVX -DMELEE_PYTHON=python3 "${launcher[@]}" > "$out/configure-game.log" 2>&1 || { say "configure failed (configure-game.log)"; fail=1; }
if cmake --build build-sourceport-gcc --target melee_game -j"$jobs" > "$out/build-game.log" 2>&1; then
  say "melee_game.dll: $(stat -c %s build-sourceport-gcc/melee_game.dll) bytes"
else
  say "BUILD FAILED (build-game.log):"; grep -E ' error|undefined reference' "$out/build-game.log" | head -20 | tee -a "$report"; fail=1
fi

step "host (clang-cl, -j$host_jobs)"
python3 port/recomp/recomp.py --host-only --out build-clangcl/generated-host > "$out/recomp.log" 2>&1 || { say "recomp failed"; fail=1; }
cmake -S . -B build-clangcl -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="$PWD/port/cmake/clang-cl-xwin.cmake" \
  -DMELEE_BUILD_EXPERIMENTAL_PORT=ON -DMELEE_CPU_BASELINE=AVX -DPORT_GEN="$PWD/build-clangcl/generated-host" \
  -DMELEE_DXC_EXECUTABLE="$cache/dxc/bin/dxc" -DMELEE_XWIN_ROOT="$cache/xwin" "${launcher[@]}" > "$out/configure-host.log" 2>&1 || { say "host configure failed"; fail=1; }
if ninja -C build-clangcl -j"$host_jobs" melee_source > "$out/build-host.log" 2>&1; then
  say "melee_source.exe: $(stat -c %s build-clangcl/port/melee_source.exe) bytes"
else
  say "HOST BUILD FAILED (build-host.log):"; grep -E 'error' "$out/build-host.log" | head -20 | tee -a "$report"; fail=1
fi
[ ${#launcher[@]} -gt 0 ] && say "ccache: $(ccache -s 2>/dev/null | grep -iE 'hits|misses' | tr -s ' ' | tr '\n' ';' | cut -c1-200)"
cp build-sourceport-gcc/melee_game.dll build-sourceport-gcc/melee_game.snapexcl build-clangcl/port/ 2>/dev/null
[ $fail = 0 ] || { say ""; say "stopped after the build failures"; exit 1; }
if [ "${FLEET_BUILD_ONLY:-0}" = 1 ]; then
  say ""; say "build only: done in $(( $(date +%s) - t0 )) s"; exit 0
fi

step "wine prefix"
export WINEPREFIX="$cache/wine-melee" WINEDEBUG=-all WINEARCH=win64
export WINEDLLOVERRIDES="d3dcompiler_47=n"
sys32="$WINEPREFIX/drive_c/windows/system32"
[ -d "$sys32" ] || xvfb-run -a wineboot -i > "$out/wine.log" 2>&1
if [ ! -f "$sys32/d3dcompiler_47.dll" ]; then
  # winetricks copies the DLL, then fails on a 32-bit registry step this 64-bit-only Wine cannot
  # run; the override comes from WINEDLLOVERRIDES anyway, so the DLL being there is what counts
  curl -sL -o /tmp/winetricks https://raw.githubusercontent.com/Winetricks/winetricks/master/src/winetricks && chmod +x /tmp/winetricks
  xvfb-run -a /tmp/winetricks -q d3dcompiler_47 >> "$out/wine.log" 2>&1
fi
[ -f "$sys32/d3dcompiler_47.dll" ] && say "d3dcompiler_47: in the prefix" || { say "d3dcompiler_47: missing (wine.log)"; fail=1; }
dxvk_dir=$(ls -d "$cache"/dxvk-*/ 2>/dev/null | tail -1)
if [ -z "$dxvk_dir" ]; then
  url=$(curl -s https://api.github.com/repos/doitsujin/dxvk/releases/latest | grep -o 'https://[^"]*dxvk-[0-9.]*\.tar\.gz' | head -1)
  [ -n "$url" ] && curl -sL "$url" | tar xz -C "$cache" && dxvk_dir=$(ls -d "$cache"/dxvk-*/ 2>/dev/null | tail -1)
fi
vulkan=$(xvfb-run -a vulkaninfo --summary 2>/dev/null | grep -m1 deviceName | sed 's/.*= *//')
say "dxvk: ${dxvk_dir:-none}; vulkan: ${vulkan:-no device}"
# Wine's own D3D11 or DXVK's, by copying DXVK's DLLs in or taking them out of the prefix
use_render() {
  unset MESA_VK_WSI_DEBUG
  case $1 in
    dxvk|dxvk-sw)
      cp "$dxvk_dir"/x64/{d3d11,dxgi,d3d10core}.dll "$sys32/"
      export WINEDLLOVERRIDES="d3dcompiler_47=n;d3d11,dxgi,d3d10core=n"
      [ "$1" = dxvk-sw ] && export MESA_VK_WSI_DEBUG=sw ;;
    *)
      # wineboot puts Wine's builtins back
      rm -f "$sys32"/{d3d11,dxgi,d3d10core}.dll; xvfb-run -a wineboot -u > /dev/null 2>&1
      export WINEDLLOVERRIDES="d3dcompiler_47=n;d3d11,dxgi,d3d10core=b" ;;
  esac
}

if [ -z "$iso_dir" ] || [ ! -f "$iso_dir/melee.iso" ] || [ ! -f "$iso_dir/mp4.iso" ]; then
  say ""; say "no discs at '${iso_dir:-MELEE_ISO_DIR unset}': no game runs"; exit $fail
fi
export MELEE_ISO="$iso_dir/melee.iso"
log=build-clangcl/port/melee_port.log
run_game() { # id mode frames seconds out extra...
  local id=$1 mode=$2 frames=$3 secs=$4 o=$5; shift 5
  MELEE_PARTY_BOOT=1 MELEE_PARTY_SKIP_CSS=1 MELEE_PARTY_ALL_CPU=1 MELEE_PARTY_TURNS=1 MELEE_PARTY_SEED=1 MELEE_PARTY_AUTO_ROLL=1 \
  MELEE_FRESH_CARD=1 MELEE_PARTY_MINIGAME="$id" MELEE_PARTY_MP4_LOG=1 \
    timeout "$secs" tools/wine_party.sh $mode --frames "$frames" --script "$PWD/port/scripts/party_boot.txt" --mp4-iso "$iso_dir/mp4.iso" "$@" > "$o" 2>&1
  local rc=$?
  WINEPREFIX="$WINEPREFIX" wineserver -k 2>/dev/null; sleep 2
  return $rc
}
# what a run's log says about the minigames (placements and their length, the MP4 heaps, crashes)
summary() {
  grep -aE 'places|game crash|panic|CRASH|out of memory|mp4: heaps at|leaves the data heap|data heap peak|Mario Party 4 disc|MP4 minigames are off|too many vertex arrays|bad free|Unhandled|exception' "$@" \
    | grep -v 'Mario Party 4 disc Z:' | awk '!seen[$0]++' | cut -c1-200 | head -40
}

# The headless games first: they are the proof that each minigame plays to the end.
declare -A first_end
for g in "${games[@]}"; do
  step "$g headless"
  run_game "$g" "--headless --fast" "${FLEET_FRAMES:-9000}" 600 "$out/$g-headless.out"; rc=$?
  cp "$log" "$out/$g-headless.log" 2>/dev/null
  summary "$out/$g-headless.log" "$out/$g-headless.out" | tee -a "$report"
  [ $rc = 0 ] || say "(exit $rc$([ $rc = 124 ] && echo ': timed out'))"
  n=$(grep -c 'minigame .*: places' "$out/$g-headless.log" 2>/dev/null)
  if [ "${n:-0}" -gt 0 ]; then
    # the frame of the first result: the last frame-statistics line before it
    first_end[$g]=$(awk '/^\[frame [0-9]+\]/ { f = $2 + 0 } /minigame .*: places/ { print f + 60; exit }' "$out/$g-headless.log" | tr -d ']')
    say "$g: $n games to the end; the first ends near frame ${first_end[$g]}"
  else
    say "$g: no game to the end"; fail=1
  fi
done

if [ "${FLEET_NO_CAPTURE:-0}" != 1 ]; then
  # Which way of rendering captures anything here: a short run of the first game with each.
  step "capture probe"
  probe=${games[0]}
  modes=(dxvk dxvk-sw wined3d)
  [ -n "$dxvk_dir" ] && [ -n "$vulkan" ] || modes=(wined3d)
  case "$vulkan" in *llvmpipe*) modes=(wined3d dxvk-sw) ;; esac   # no GPU Vulkan: Mesa's CPU renderers both
  [ "${FLEET_RENDER:-auto}" = auto ] || modes=("$FLEET_RENDER")
  render=""; spf=1
  for m in "${modes[@]}"; do
    use_render "$m"
    window=640x480; [ "$m" = wined3d ] && window=320x240
    mkdir -p "$out/probe-$m"; rm -f "$out/probe-$m"/*
    s=$(date +%s)
    run_game "$probe" "--hidden --fast --window $window --scale 1" 600 600 "$out/probe-$m.out" --capture "$out/probe-$m/f.ppm" --capture-every 100
    secs=$(( $(date +%s) - s ))
    cp "$log" "$out/probe-$m.log" 2>/dev/null
    n=$(ls "$out/probe-$m"/f_*.ppm 2>/dev/null | wc -l)
    say "$m ($window): $n frames captured of 600 in $secs s; log ends: $(grep -av '^\[frame' "$out/probe-$m.log" 2>/dev/null | tail -1 | cut -c1-120)"
    grep -aiE 'error|fail|crash|exception|err:' "$out/probe-$m.out" | head -5 | cut -c1-200 | sed 's/^/    /' | tee -a "$report"
    if [ "$n" -gt 0 ]; then
      render=$m; spf=$(awk -v s="$secs" 'BEGIN { v = (s - 10) / 600; print (v < 0.01 ? 0.01 : v) }')
      ( cd "$out/probe-$m" && montage -label '%t' f_*.ppm -tile 6x -geometry 320x240+4+4 "../probe-$m.png" ) && rm -f "$out/probe-$m"/f_*.ppm
      break
    fi
  done
  if [ -z "$render" ]; then
    say "no way of rendering captured a frame: no captures"; fail=1
  else
    say "captures render with $render, about $spf s a frame"
    i=0
    for g in "${games[@]}"; do
      left=$(( ${#games[@]} - i )); i=$((i + 1))
      step "$g capture"
      # the whole first game if the time allows; the time left shared by the games left
      want=$(( ${first_end[$g]:-2400} + 240 ))
      budget=$(( (deadline - $(date +%s)) / left - 30 ))
      frames=$(awk -v b="$budget" -v s="$spf" -v w="$want" 'BEGIN { f = int(b / s); if (f > w) f = w; print f }')
      if [ "$frames" -lt 300 ]; then say "$g: no time left for a capture"; fail=1; continue; fi
      every=$(( frames / 24 )); [ $every -ge 10 ] || every=10
      window=640x480; [ "$render" = wined3d ] && window=320x240
      mkdir -p "$out/cap-$g"; rm -f "$out/cap-$g"/*
      run_game "$g" "--hidden --fast --window $window --scale 1" "$frames" "$(( budget + 60 ))" "$out/$g-capture.out" \
        --capture "$out/cap-$g/f.ppm" --capture-every "$every"; rc=$?
      cp "$log" "$out/$g-capture.log" 2>/dev/null
      summary "$out/$g-capture.log" "$out/$g-capture.out" | grep -v 'mp4: heaps at' | tee -a "$report"
      n=$(ls "$out/cap-$g"/f_*.ppm 2>/dev/null | wc -l)
      if [ "$n" -gt 0 ]; then
        ( cd "$out/cap-$g" && montage -label '%t' f_*.ppm -tile 6x -geometry 320x240+4+4 "../$g-montage.png" && rm -f f_*.ppm )
        say "$g: $n frames of $frames (one every $every$([ $rc = 124 ] && echo ', stopped by the time limit')), montage $g-montage.png"
      else
        say "$g: no frames captured (exit $rc; $g-capture.out)"; fail=1
      fi
    done
  fi
fi

# a crash needs the library's symbols to read: they come back with the job when one happened
if grep -qaE 'game crash|CRASH|Unhandled' "$out"/*.log "$out"/*.out 2>/dev/null; then
  xz -T0 -c build-sourceport-gcc/melee_game.dbg > "$out/melee_game.dbg.xz" 2>/dev/null && say "" && say "a crash: melee_game.dbg.xz is in the outputs"
fi
say ""; say "done in $(( $(date +%s) - t0 )) s, failures: $fail"
exit $fail
