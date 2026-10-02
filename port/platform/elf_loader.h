// Loads the game library built as an ELF (sourceport/cmake/linux-elf-*.cmake) at its link address.
//
// The game keeps addresses in 32-bit slots (disc pointers), so its image has to sit where it was
// linked, below 4 GB (0x82800000), which no system loader guarantees: Android's linker refuses to
// place a library at a fixed address, glibc's does not try. This loader maps every PT_LOAD segment
// at its own address (MAP_FIXED_NOREPLACE, failing cleanly when something else is there), applies the
// relocations and binds the few C library calls the game makes to the host process's own.
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <functional>
#include <string>

namespace platform {

struct ElfImage {
  uintptr_t base = 0;        // lowest mapped address (the link address of the first segment)
  uintptr_t end = 0;         // one past the highest mapped address
  uintptr_t rw_start = 0;    // the writable segment: initialised data, then bss
  uintptr_t rw_data_end = 0;
  uintptr_t rw_end = 0;
  std::string error;         // set when load() returns false
  void* symbol(const char* name) const;   // a defined dynamic symbol, or nullptr
  // internal
  const uint8_t* symtab = nullptr;
  const char* strtab = nullptr;
  uint32_t nsyms = 0;
};

// resolve(name) supplies the address of every symbol the image imports; nullptr leaves the import
// unbound (the loader then points it at a function that reports the name and aborts).
using ElfResolver = std::function<void*(const char* name)>;

bool load_elf_image(const std::string& path, const ElfResolver& resolve, ElfImage* out);

}  // namespace platform
