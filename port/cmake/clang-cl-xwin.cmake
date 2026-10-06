# The host (melee_source and the port tests) cross-compiled from Linux with clang-cl and lld-link
# against the MSVC CRT and Windows SDK that xwin downloads (https://github.com/Jake-Shadle/xwin).
# Windows builds keep using Visual Studio; this is for agents building on Linux and running under
# Wine. See docs/agent-testing.md.
#
#   xwin --accept-license --arch x86_64 splat --output ~/.cache/xwin
#   cmake -S . -B build-clangcl -G Ninja -DCMAKE_TOOLCHAIN_FILE=port/cmake/clang-cl-xwin.cmake ...
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)
set(MELEE_XWIN_ROOT "$ENV{HOME}/.cache/xwin" CACHE PATH "xwin splat output (crt/ and sdk/)")
set(MELEE_LLVM_BIN "/usr/lib/llvm-19/bin" CACHE PATH "Folder holding clang-cl, lld-link, llvm-lib, llvm-rc")
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES MELEE_XWIN_ROOT MELEE_LLVM_BIN)

set(CMAKE_C_COMPILER "${MELEE_LLVM_BIN}/clang-cl")
set(CMAKE_CXX_COMPILER "${MELEE_LLVM_BIN}/clang-cl")
set(CMAKE_LINKER "${MELEE_LLVM_BIN}/lld-link" CACHE FILEPATH "")
set(CMAKE_AR "${MELEE_LLVM_BIN}/llvm-lib" CACHE FILEPATH "")
set(CMAKE_RC_COMPILER "${MELEE_LLVM_BIN}/llvm-rc" CACHE FILEPATH "")
set(CMAKE_MT "${MELEE_LLVM_BIN}/llvm-mt" CACHE FILEPATH "")
set(CMAKE_C_COMPILER_TARGET x86_64-pc-windows-msvc)
set(CMAKE_CXX_COMPILER_TARGET x86_64-pc-windows-msvc)

set(_xwin_includes "${MELEE_XWIN_ROOT}/crt/include" "${MELEE_XWIN_ROOT}/sdk/include/ucrt"
  "${MELEE_XWIN_ROOT}/sdk/include/um" "${MELEE_XWIN_ROOT}/sdk/include/shared" "${MELEE_XWIN_ROOT}/sdk/include/winrt")
set(_xwin_flags "")
set(_xwin_rc_flags "")
foreach(dir ${_xwin_includes})
  string(APPEND _xwin_flags " /imsvc \"${dir}\"")
  string(APPEND _xwin_rc_flags " /I \"${dir}\"")
endforeach()
# MSVC accepts these; clang-cl warns or errors by default.
string(APPEND _xwin_flags " -Wno-unused-command-line-argument -Wno-microsoft-include -Wno-c++11-narrowing -Wno-invalid-offsetof")
set(CMAKE_C_FLAGS_INIT "${_xwin_flags}")
set(CMAKE_CXX_FLAGS_INIT "${_xwin_flags}")
set(CMAKE_RC_FLAGS_INIT "${_xwin_rc_flags}")
set(_xwin_libs "/libpath:\"${MELEE_XWIN_ROOT}/crt/lib/x86_64\" /libpath:\"${MELEE_XWIN_ROOT}/sdk/lib/um/x86_64\" /libpath:\"${MELEE_XWIN_ROOT}/sdk/lib/ucrt/x86_64\"")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${_xwin_libs}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${_xwin_libs}")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "${_xwin_libs}")

# xwin leaves out the debug CRT (msvcrtd.lib), and a test zip must run on a PC without the VC++
# redistributable: the static release CRT in every configuration.
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded)
set(CMAKE_TRY_COMPILE_CONFIGURATION Release)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# Tests are Windows executables: ctest runs them under Wine.
set(CMAKE_CROSSCOMPILING_EMULATOR wine)
