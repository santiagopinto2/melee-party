#!/bin/sh
# The toolchain for a fleet job on a plain Debian 13 image (the fleet's default, docker.io/
# library/debian:13): what tools/fleet/Dockerfile bakes into the image, installed at the start
# of the job instead, so no image has to be published. A few minutes a job. Idempotent: does
# nothing when the cross compiler is already there (the image from the Dockerfile).
set -eu
if command -v x86_64-w64-mingw32-gcc > /dev/null 2>&1 && [ -x /usr/lib/llvm-19/bin/clang-cl ]; then
  echo "toolchain present"; exit 0
fi
export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y --no-install-recommends \
  ca-certificates curl git xz-utils unzip cabextract \
  python3 cmake ninja-build make \
  gcc-mingw-w64-x86-64-posix g++-mingw-w64-x86-64-posix binutils-mingw-w64-x86-64 \
  clang-19 clang-tools-19 lld-19 llvm-19 \
  wine wine64 \
  mesa-vulkan-drivers libgl1-mesa-dri libglx-mesa0 libegl-mesa0 libvulkan1 mesa-utils vulkan-tools \
  xvfb xauth \
  imagemagick
rm -rf /var/lib/apt/lists/*
echo "toolchain installed"
