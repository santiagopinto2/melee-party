# The game library as an ELF for the Linux and Android hosts (port/platform/elf_loader.cpp loads
# it at its fixed address). GCC only, for the same reason as mingw-w64-x86_64.cmake.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER gcc-15)
set(CMAKE_CXX_COMPILER g++-15)
set(MELEE_ELF_TARGET ON)
