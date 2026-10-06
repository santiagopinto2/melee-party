# The game library cross-compiled from Linux with the distribution's MinGW-w64 GCC (Debian/Ubuntu
# package g++-mingw-w64-x86-64-posix, GCC 14 or newer). Same output as mingw-w64-x86_64.cmake; see
# docs/agent-testing.md.
#
#   cmake -S sourceport/game -B build-sourceport-gcc -G Ninja
#         -DCMAKE_TOOLCHAIN_FILE=sourceport/cmake/mingw-w64-x86_64-linux.cmake
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(MELEE_MINGW_PREFIX "x86_64-w64-mingw32-" CACHE STRING "Prefix of the MinGW-w64 cross tools")
set(MELEE_MINGW_SUFFIX "-posix" CACHE STRING "Suffix of the MinGW-w64 compilers (-posix or empty)")
set(CMAKE_C_COMPILER "${MELEE_MINGW_PREFIX}gcc${MELEE_MINGW_SUFFIX}")
set(CMAKE_CXX_COMPILER "${MELEE_MINGW_PREFIX}g++${MELEE_MINGW_SUFFIX}")
set(CMAKE_AR "/usr/bin/${MELEE_MINGW_PREFIX}gcc-ar${MELEE_MINGW_SUFFIX}" CACHE FILEPATH "")
set(CMAKE_RANLIB "/usr/bin/${MELEE_MINGW_PREFIX}gcc-ranlib${MELEE_MINGW_SUFFIX}" CACHE FILEPATH "")
set(CMAKE_LINKER "/usr/bin/${MELEE_MINGW_PREFIX}ld" CACHE FILEPATH "")
set(CMAKE_NM "/usr/bin/${MELEE_MINGW_PREFIX}nm" CACHE FILEPATH "")
set(CMAKE_OBJCOPY "/usr/bin/${MELEE_MINGW_PREFIX}objcopy" CACHE FILEPATH "")
set(CMAKE_RC_COMPILER "${MELEE_MINGW_PREFIX}windres")
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
# The game's tests are Windows executables: ctest runs them under Wine.
set(CMAKE_CROSSCOMPILING_EMULATOR wine)
