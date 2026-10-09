# Melee Party fleet jobs

Since 2026-10-08 nothing that reads the Melee or Mario Party 4 discs runs on Santi's VM: the
headless games, the rendered captures and the test build's smoke party run on arhum's machines as
fleet jobs (`/var/lib/fleet-jobs/README.md` on the VM). A job gets a public checkout of this
repository at one commit, the toolchain image and the internet; the discs are mounted there,
read-only, at a path arhum gives.

- `setup.sh`: the toolchain (Debian 13: MinGW-w64 GCC, LLVM 19 for clang-cl, Wine, Mesa Vulkan
  and OpenGL, Xvfb, ImageMagick) installed at the start of a job on the fleet's plain Debian
  image, a few minutes a job. `Dockerfile` bakes the same into an image; published public on
  ghcr.io and named in the request's `image`, it saves those minutes (`setup.sh` is then a
  no-op). Neither holds anything of Microsoft's: `job.sh` fetches the MSVC CRT and Windows SDK
  (xwin), the DirectX shader compiler, `d3dcompiler_47` (winetricks) and DXVK when the job
  starts.
- `job.sh`: what the job runs. Builds the game library and the host, sets the Wine prefix up,
  then runs each MP4 minigame named on the command line (default: all) headless to the end, and
  then captured. The captures first find a way of rendering that works on the machine (DXVK on
  Vulkan, DXVK with Mesa's software present, or Wine's own D3D11 on OpenGL, which on Xvfb is
  Mesa's CPU renderer) with a short run of the first game, then share the time left before the
  request's limit (`FLEET_TIME_LIMIT`) between the games, so each gets a montage: the whole first
  game when there is time, less when not. `fleet-out/report.md` sums it up: per game, the
  placements and length of each minigame played (`places ... coins ..., N frames`), the MP4
  heaps' sizes and the data heap's peak, crashes; the montages, logs and each run's Wine output
  (`*.out`) sit beside it. After a crash `melee_game.dbg.xz` comes back too, for the symbols.
  `/keep` (the fleet's folder that stays on each machine) holds the apt packages, xwin, dxc,
  DXVK, the Wine prefix and a ccache, so the next job on that machine is faster. Build
  parallelism follows the job's memory (one compiler per GB).
- `request.json`: the game job, with `COMMIT` (the full hash) to fill in: `fleet-job submit
  request.json`. Its `private_files` mounts Santi's discs read-only at `/private/melee.iso` and
  `/private/mp4.iso` (sent once with `fleet-job private-upload`); nothing under `/private` may
  come back, so the output is the checkout's `fleet-out`. It asks for `"gpu_kind": "opengl"`,
  `"mem": "3G"` and `"prefer": ["gpu-optiplex"]`, so the GPU OptiPlex takes it and arhum's PC
  only when the OptiPlex is busy (Santi, 2026-10-09: his PC last). Bigger Blast comes first in its
  list: it was verified on the VM, so it tells a fleet problem from a minigame's.
- `compile.json`: the same checkout built and nothing run (`FLEET_BUILD_ONLY=1`), as a CPU-only job
  with `"mem": "3G"` so Amal's PC and both OptiPlexes can take it (the fleet still prefers arhum's
  PC when it is free). Santi's choice (2026-10-09): his PC is the least reliable, so compile checks,
  symbol checks and disc-free tests go to the others; only the game runs need his PC, where the
  discs are. arhum's rule (2026-10-09): GPU jobs go to the three PCs with GPUs, CPU-only jobs to
  those and the SSD OptiPlex, and nothing at all compiles or runs on Santi's VM. The report says
  how long the builds took and `fleet-out/build-*.log` has the errors. If 3G is too little for the
  host link, raise `mem` to `8G` (Amal's PC).

A job only starts while arhum is away from his PC, so results come in the evening or at night.
