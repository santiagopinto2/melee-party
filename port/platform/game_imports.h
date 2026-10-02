// The C library calls the ELF game library makes, bound to the host process's own functions.
//
// An explicit table rather than dlsym: on Android the game (built against glibc headers by GCC) and
// the host (bionic) agree on these calls' arguments, but not necessarily on every name a libc
// exports, and a table states exactly what the game may call. A call missing here fails loudly
// (platform::load_elf_image binds it to a function that names it and aborts).
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

extern "C" void* platform_game_import(const char* name);   // game_imports.c

namespace platform {
inline void* game_imports(const char* name) { return platform_game_import(name); }
}
