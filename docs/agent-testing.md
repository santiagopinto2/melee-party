# Building and testing on Linux (agents)

How an agent on a Linux machine with no GPU and no display (for example a small agent VM: Debian 13, 4 vCPU,
4 GB RAM, Intel Xeon E5-2650L) builds Melee Party, runs its tests, and plays a scripted party it
can see. Windows builds are unchanged; see [build-source-port.md](build-source-port.md).

The path: the game library (`melee_game.dll`) cross-compiles with Debian's MinGW-w64 GCC, the host
(`melee_source.exe`) with clang-cl against the MSVC CRT and Windows SDK that
[xwin](https://github.com/Jake-Shadle/xwin) downloads, and the game runs under Wine on Xvfb, with
D3D11 going through wined3d to Mesa's software OpenGL (llvmpipe). Nothing is emulated: it is the
real Windows build of the game.

**Never commit or push anything from the ISO**: not the ISO, not extracted files, not
screenshots or frame dumps of the game. Keep them out of the repository (the commands below write
to `/tmp` and `~/.local/share/melee/`).

## One-time setup

```sh
sudo apt-get install -y gcc-mingw-w64-x86-64-posix g++-mingw-w64-x86-64-posix cmake ninja-build \
  clang lld llvm wine wine64 xvfb mesa-utils libgl1-mesa-dri cabextract imagemagick
git submodule update --init --recursive
python3 tools/prepare_native_sources.py

# MSVC CRT + Windows SDK for clang-cl (about 1.1 GB download, 630 MB kept)
curl -sL https://github.com/Jake-Shadle/xwin/releases/download/0.10.0/xwin-0.10.0-x86_64-unknown-linux-musl.tar.gz | tar xz
./xwin-0.10.0-x86_64-unknown-linux-musl/xwin --accept-license --cache-dir ~/.cache/xwin-dl --arch x86_64 splat --output ~/.cache/xwin
rm -rf ~/.cache/xwin-dl xwin-0.10.0-*

# DXC for Linux (configure needs it; only melee_port's ray-tracing shader uses it)
mkdir -p ~/.cache/dxc && curl -sL https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.9.2609/linux_dxc_2026_09_28.x86_x64.tar.gz | tar xz -C ~/.cache/dxc

# A Wine prefix with Microsoft's HLSL compiler: Wine's own d3dcompiler_47 cannot compile the
# D3D11 renderer's pixel shaders ("E5017: not yet implemented"), and every draw is lost.
curl -sL -o ~/.local/bin/winetricks https://raw.githubusercontent.com/Winetricks/winetricks/master/src/winetricks
chmod +x ~/.local/bin/winetricks
export WINEPREFIX=~/.cache/wine-melee
xvfb-run -a wineboot -i
xvfb-run -a winetricks -q d3dcompiler_47
```

The ISO: Melee NTSC 1.02, plain uncompressed `.iso` (GALE01 rev 2, 1,459,978,240 bytes, MD5
`0e63d4223b01d9aba596259dc155a174`). On the agent VM it is at `~/.local/share/melee/melee.iso`.

## CPU baseline: build for AVX

Releases are built for AVX2. A CPU without AVX2 (the agent VM's Sandy Bridge Xeon has AVX but no
AVX2 or FMA) refuses to start them, or crashes with an illegal instruction. Check
`grep -o -w 'avx2\|fma' /proc/cpuinfo | sort -u`; with neither, configure both builds with
`-DMELEE_CPU_BASELINE=AVX`. An AVX build fuses no multiply-adds, so its online matches are not
bit-identical to an AVX2 build's; that does not matter for local tests.

## Build

One build at a time, at most two jobs: 4 GB of RAM holds that and nothing bigger.

```sh
# Game library: about 25 minutes from scratch on 2 jobs, 6.5 MB DLL
cmake -S sourceport/game -B build-sourceport-gcc \
  -DCMAKE_TOOLCHAIN_FILE=$PWD/sourceport/cmake/mingw-w64-x86_64-linux.cmake \
  -DMELEE_CPU_BASELINE=AVX -DMELEE_PYTHON=python3
cmake --build build-sourceport-gcc --target melee_game -j2

# Host: the headers it needs come from the in-repo symbol and HLE lists (no DOL, no translation)
python3 port/recomp/recomp.py --host-only --out build-clangcl/generated-host
cmake -S . -B build-clangcl -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$PWD/port/cmake/clang-cl-xwin.cmake \
  -DMELEE_BUILD_EXPERIMENTAL_PORT=ON -DMELEE_CPU_BASELINE=AVX \
  -DPORT_GEN=$PWD/build-clangcl/generated-host -DMELEE_DXC_EXECUTABLE=$HOME/.cache/dxc/bin/dxc
ninja -C build-clangcl -j2 melee_source          # about 10 minutes
cp build-sourceport-gcc/melee_game.dll build-sourceport-gcc/melee_game.snapexcl build-clangcl/port/
```

After changing party code (`sourceport/game/party/`) only the game library needs rebuilding:
rebuild `melee_game` and copy the two files again. `melee_port` (Static Recomp) and the targets
that link the translated game (`port_mm_probe`, `port_settings_load_test`, `port_native_parity`)
need the full translation from the DOL and are not built here.

## Tests

```sh
# Plain C++ tests at the root, built natively (seconds)
cmake -S . -B build-linux && cmake --build build-linux -j2 && ctest --test-dir build-linux

# The game library's tests: Windows executables, run by ctest under Wine
export WINEPREFIX=~/.cache/wine-melee WINEDEBUG=-all
cmake --build build-sourceport-gcc -j2 && ctest --test-dir build-sourceport-gcc

# The host's tests (port/tests and the root ones, as Windows executables): build, then ctest under Wine
ninja -C build-clangcl -j2 native_practice_model_test packed_animation_test native_animation_test \
  $(ninja -C build-clangcl -t targets all | grep -o '^port/port_[a-z0-9_]*\.exe' \
    | grep -v -e mm_probe -e settings_load -e native_parity -e dht_probe | sort -u)
xvfb-run -a ctest --test-dir build-clangcl -E 'port_mm|port_settings_load|native_parity' --timeout 600
```

`port_native_savestate` takes about 3 minutes under Wine, hence the long timeout.

The test status as of this writing is under [Status on the agent VM](#status-on-the-agent-vm).

## Running a scripted party

`tools/wine_party.sh` runs `build-clangcl/port/melee_source.exe` under Wine on Xvfb with the D3D11
renderer, the ISO from `$MELEE_ISO` (default `~/.local/share/melee/melee.iso`) and sound off.
Linux paths in its arguments become `Z:\...` paths for Wine. The game's log is
`build-clangcl/port/melee_port.log`; the party logs every roll, landing, minigame placement and
final score there as `[game] [party] ...` lines.

`port/scripts/party_boot.txt` presses A through the two memory card notices of a fresh card
(`MELEE_FRESH_CARD=1` starts from one, which keeps runs repeatable). The `MELEE_PARTY_*` knobs are
in [melee-party.md](melee-party.md#testing-knobs).

`port/scripts/party_minigame_menu.txt` is the player's own path, without `MELEE_PARTY_BOOT`: the
menus to VS Mode, Party Minigames, its sixth row (Bowser's Bigger Blast with an MP4 disc), the
character select with two humans picking, START. Run it with `MELEE_FRESH_CARD=1` and 3200 frames
(headless, about 70 seconds) when a change touches the menu, the character select or how a match
starts: the knobs skip all of that. Without an MP4 disc the sixth row does not exist and the
cursor stops on the fifth. `party_minigame_menu_last.txt` is the same path to the list's last row
(one press up from the first: the list wraps), the newest MP4 minigame with a disc and Dungeon Duos
without one.

### Logic only: headless

No renderer, about 23 game frames a second: a whole one-turn party with CPUs, board, minigame,
podium and back to the menu in about 4 minutes.

```sh
MELEE_PARTY_BOOT=1 MELEE_PARTY_SKIP_CSS=1 MELEE_PARTY_ALL_CPU=1 MELEE_PARTY_TURNS=1 \
MELEE_PARTY_SEED=1 MELEE_PARTY_AUTO_ROLL=1 MELEE_FRESH_CARD=1 \
  tools/wine_party.sh --headless --frames 6000 --script "$PWD/port/scripts/party_boot.txt"
grep '\[party\]' build-clangcl/port/melee_port.log | grep -v reaches
```

With a fixed `MELEE_PARTY_SEED` the log is the same on every run, so it can be diffed before and
after a change. `MELEE_PARTY_MINIGAME=<id>` plays one minigame over and over.

### Seeing it: frame captures

The same run with the renderer, writing a picture every N game frames:

```sh
mkdir -p /tmp/mpcap
MELEE_PARTY_BOOT=1 MELEE_PARTY_SKIP_CSS=1 MELEE_PARTY_ALL_CPU=1 MELEE_PARTY_TURNS=1 \
MELEE_PARTY_SEED=1 MELEE_PARTY_AUTO_ROLL=1 MELEE_FRESH_CARD=1 \
  tools/wine_party.sh --hidden --fast --window 640x480 --scale 1 --frames 2200 \
    --script "$PWD/port/scripts/party_boot.txt" --capture /tmp/mpcap/f.ppm --capture-every 10
cd /tmp/mpcap && for f in *.ppm; do convert "$f" "${f%.ppm}.png"; done
montage $(ls *.png | tail -16) -tile 4x -geometry 320x240+2+2 sheet.png   # one image to look at
```

Then look at the PNGs (an agent can read them as images). `--capture-frame N --capture out.ppm`
writes one presented frame, `--capture-sim-frame N` the first one at or after game frame N.

`--hidden` matters twice. A hidden (automated) run renders on the game thread and presents every
game frame, so `--capture-every N` means every N game frames and two runs give the same pictures.
Without it the game takes the player's path: a render thread that presents 2 to 3 pictures a
second on llvmpipe and drops the rest (and with `--fast`, nearly all of them). And without it a
crash opens a dialog box that nobody can close, so the run hangs until its timeout. `--fast`
drops the 60 Hz pacing, which only matters for menus and headless runs.

Software rendering is slow, so keep rendered runs short and aim them with the knobs:
`MELEE_PARTY_MINIGAME=volley` above (boot plus 30 seconds of Volleyball, 2200 frames, 21 pictures)
took 7.5 minutes at 640x480, about 5 game frames a second. The board draws much more and runs at
about 2 game frames a second at 1280x960.

## Test builds for Santi

Santi tries a change on his Windows PC before it ships. The flow:

1. He asks for a change. Make it on a branch, build, run the tests and a CPU party, and look at a
   screenshot of what changed (sections above).
2. `tools/test_build.sh <name> "<what changed, one line>"` builds incrementally (the game library
   and host trees stay cached; a party-only change takes a few minutes), plays a headless one-turn
   CPU party as a smoke test, packages the zip and prints its link.
3. Send him the link, `http://santi:8090/builds/MeleeParty-test-<name>-<date>.zip`. The page
   `http://santi:8090/` lists the last five builds, newest first.
4. "Ship it" means open the PR and merge it once checks are green, and add a line to
   `docs/history/RELEASE_NOTES_UNRELEASED.md` if players would notice. "Change X" means a new
   build and a new link.

The zip (`tools/package_test_build.py`) is the runnable build output: `melee_source.exe` (static
CRT, so no Visual C++ runtime is needed), `melee_game.dll` and `melee_game.snapexcl`. It also
carries what a release has beside the game (Slippi's `Sys`, `ui_sources`, `licenses`), plus
`PlayTest.bat` and a `README.txt`. He unzips it into its own folder and drags his ISO onto
`PlayTest.bat` once; it remembers the path. The folder keeps its own settings, saves and replays,
so his normal install is not touched. The packager refuses disc images and game file types outside
`Sys/`, and anything over 64 MB.

The server is the user unit `test-builds.service` on this machine (`~/services/test-builds/`): a
static file server bound to the tailnet address only, port 8090. It is reachable from Santi's
devices and nowhere else. Test builds never go to GitHub (the repository is public) or to a
public site.

The test build is built for AVX like the rest of this machine's builds, not for AVX2 like a
release. It runs on any PC that runs the release, but its floating point is not bit-identical to
a release build's, so it can only play online against the same test build.

## Gotchas

- `melee_port.log` beside the exe is the first thing to read. A crash in the game library logs
  `game crash ... at melee_game.dll+0x...` and a stack; symbolize with
  `x86_64-w64-mingw32-addr2line -f -i -e build-sourceport-gcc/melee_game.dbg 0x<0x82800000 + offset>`.
- Wine prints ALSA and `X connection ... broken` noise on exit; it means nothing.
- Wine's stack sits at high addresses (`0x7FFF...`), where Windows' usually sits low. A bug that
  passes an `int` where the game reads a 64-bit value can show up only under Wine (see the
  variadic `NULL` terminators fixed in `melee-native.patch`). The reverse happens too: a stale
  stack slot can be zero under Wine and not on Windows, so a path that works here can crash on a
  PC. The title screen's attract demo and the menu path above are cheap checks that the knobs skip.
- `pkill -x melee_source.exe` never matches: process names are cut at 15 characters. Stop a run
  with `WINEPREFIX=~/.cache/wine-melee wineserver -k`.
- Don't run two copies at once in the same exe folder: they share `melee_port.log` and `User/`.

## Status on the agent VM

Verified on 2026-10-05 (main at 75fcb17 plus this change):

| What | Result |
|---|---|
| Root tests (native Linux) | 3/3 pass |
| Game library tests (Wine) | 10/12 pass. `native_disc_offsets` hard-codes a Windows toolchain path (`tools/disc_offset_audit.py`); `native_disc_layout` (not in `all`) returns 1 |
| Host tests (Wine, `xvfb-run`) | 54/56 pass. Failing under Wine: `port_launch_process` and `port_cosmetic_mods` (ZIP/vault import). `port_frame_queue` has a 200 ms timing assertion that can fail on a loaded machine |
| `melee_source.exe` under Wine | boots, menus and party render; one-turn CPU party plays board, Domination and podium headless in 4 minutes |
| Rendered board and Volleyball | captured at 1280x960 and 640x480 (slow, see above) |
| Online pair (`tools/online_pair.py`) | not tried |

What cannot be done here: real GPU timing or D3D12 (there is no GPU; `--backend d3d12` would go
through vkd3d to lavapipe and is untested), DLSS, controllers, audio output, Slippi online against
other players, and `melee_port` (Static Recomp, which needs the full translation and several GB of
objects).
