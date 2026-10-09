#!/bin/sh
# The toolchain for a fleet job on a plain Debian 13 image (the fleet's default, docker.io/
# library/debian:13): what tools/fleet/Dockerfile bakes into the image, installed at the start
# of the job instead, so no image has to be published. A few minutes a job. Idempotent: does
# nothing when the cross compiler is already there (the image from the Dockerfile). With
# FLEET_CACHE set (job.sh: /keep/melee-party), the downloaded packages stay there for the next job.
set -eu
if command -v x86_64-w64-mingw32-gcc > /dev/null 2>&1 && [ -x /usr/lib/llvm-19/bin/clang-cl ]; then
  echo "toolchain present"; exit 0
fi
export DEBIAN_FRONTEND=noninteractive
keep=""
if [ -n "${FLEET_CACHE:-}" ]; then
  mkdir -p "$FLEET_CACHE/apt/archives/partial"
  keep="-o Dir::Cache::Archives=$FLEET_CACHE/apt/archives -o APT::Keep-Downloaded-Packages=true"
fi
apt-get update
# shellcheck disable=SC2086
apt-get install -y --no-install-recommends $keep \
  ca-certificates curl git xz-utils unzip cabextract \
  python3 cmake ninja-build make ccache \
  gcc-mingw-w64-x86-64-posix g++-mingw-w64-x86-64-posix binutils-mingw-w64-x86-64 \
  clang-19 clang-tools-19 lld-19 llvm-19 \
  wine wine64 \
  mesa-vulkan-drivers libgl1-mesa-dri libglx-mesa0 libegl-mesa0 libvulkan1 mesa-utils vulkan-tools \
  xvfb xauth \
  imagemagick
rm -rf /var/lib/apt/lists/*
echo "toolchain installed"
