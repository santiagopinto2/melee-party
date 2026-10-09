# Melee Party on arhum's PC (fleet jobs)

Since 2026-10-08 nothing that reads the Melee or Mario Party 4 discs runs on Santi's VM: the
headless games, the rendered captures and the test build's smoke party run on arhum's PC as
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
- `job.sh`: what the job runs. Builds the game library and the host, sets the Wine prefix up
  (DXVK on Vulkan, so the Radeon renders; `FLEET_GPU=0` for Wine's OpenGL path), runs the
  port's GPU test, then each MP4 minigame named on the command line (default: all) headless to
  the end and captured every 50 frames. `fleet-out/report.md` sums it up; the montages and
  logs sit beside it.
- `request.json`: the request, with `COMMIT` (the full hash) to fill in: `fleet-job submit
  request.json`. Its `private_files` mounts Santi's discs read-only at `/private/melee.iso` and
  `/private/mp4.iso` (sent once with `fleet-job private-upload`); nothing under `/private` may
  come back, so the outputs are the checkout's `fleet-out` and the built binaries.

A job only starts while arhum is away from his PC, so results come in the evening or at night.
