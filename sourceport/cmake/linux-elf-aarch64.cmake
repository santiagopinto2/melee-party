# The game library as an aarch64 ELF (Android arm64 and Linux arm64 hosts; tested under qemu-aarch64).
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc-15)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++-15)
set(CMAKE_OBJCOPY aarch64-linux-gnu-objcopy)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(MELEE_ELF_TARGET ON)
