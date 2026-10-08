# Melee Party toolchain for fleet jobs on arhum's PC (see tools/fleet/README.md).
#
# Debian 13, like the VM the project is built on: the MinGW-w64 GCC that builds the game library,
# clang-cl and lld-link (LLVM 19) for the host, Wine with Mesa's Vulkan and OpenGL drivers for
# running the game on the PC's Radeon, Xvfb for the display, ImageMagick for the capture montages.
# The MSVC CRT and Windows SDK (xwin), the DirectX shader compiler, Microsoft's d3dcompiler_47 and
# DXVK are not in the image: they are Microsoft's or large, and tools/fleet/job.sh fetches them
# when a job starts.
#
#   docker build -t melee-party-tools:latest -f tools/fleet/Dockerfile tools/fleet
FROM debian:trixie-slim
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
      ca-certificates curl git xz-utils unzip cabextract \
      python3 cmake ninja-build make \
      gcc-mingw-w64-x86-64-posix g++-mingw-w64-x86-64-posix binutils-mingw-w64-x86-64 \
      clang-19 clang-tools-19 lld-19 llvm-19 \
      wine wine64 \
      mesa-vulkan-drivers libgl1-mesa-dri libglx-mesa0 libegl-mesa0 libvulkan1 mesa-utils vulkan-tools \
      xvfb xauth \
      imagemagick \
    && rm -rf /var/lib/apt/lists/*
# Wine's 64-bit tools without the 32-bit half
ENV WINEARCH=win64 WINEDEBUG=-all
WORKDIR /work
