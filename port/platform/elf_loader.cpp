// SPDX-License-Identifier: GPL-3.0-or-later
#include "elf_loader.h"

#include <elf.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

namespace platform {

namespace {

constexpr uintptr_t kPage = 0x10000;   // the image is linked with -z max-page-size=0x10000

uintptr_t page_down(uintptr_t v) { return v & ~(kPage - 1); }
uintptr_t page_up(uintptr_t v) { return (v + kPage - 1) & ~(kPage - 1); }

// An import nothing supplied: the game called something the host does not provide.
const char* g_unbound_names[256];
int g_unbound_count = 0;
[[noreturn]] void unbound_import() {
  std::fprintf(stderr, "game called an unbound import (one of:");
  for (int i = 0; i < g_unbound_count; ++i) std::fprintf(stderr, " %s", g_unbound_names[i]);
  std::fprintf(stderr, ")\n");
  std::abort();
}

#if defined(__x86_64__)
constexpr uint32_t kRelRelative = R_X86_64_RELATIVE, kRelAbs = R_X86_64_64,
                   kRelGlobDat = R_X86_64_GLOB_DAT, kRelJumpSlot = R_X86_64_JUMP_SLOT;
constexpr uint16_t kMachine = EM_X86_64;
#elif defined(__aarch64__)
constexpr uint32_t kRelRelative = R_AARCH64_RELATIVE, kRelAbs = R_AARCH64_ABS64,
                   kRelGlobDat = R_AARCH64_GLOB_DAT, kRelJumpSlot = R_AARCH64_JUMP_SLOT;
constexpr uint16_t kMachine = EM_AARCH64;
#else
#error "elf_loader: x86_64 and aarch64 only"
#endif

}  // namespace

void* ElfImage::symbol(const char* name) const {
  const auto* syms = reinterpret_cast<const Elf64_Sym*>(symtab);
  for (uint32_t i = 1; i < nsyms; ++i) {
    const Elf64_Sym& s = syms[i];
    if (s.st_shndx != SHN_UNDEF && std::strcmp(strtab + s.st_name, name) == 0)
      return reinterpret_cast<void*>(s.st_value);   // linked at its own address: no bias
  }
  return nullptr;
}

bool load_elf_image(const std::string& path, const ElfResolver& resolve, ElfImage* out) {
  *out = ElfImage{};
  int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
  if (fd < 0) { out->error = "cannot open " + path; return false; }
  struct stat st {};
  ::fstat(fd, &st);
  std::vector<uint8_t> file(static_cast<size_t>(st.st_size));
  size_t got = 0;
  while (got < file.size()) {
    ssize_t n = ::read(fd, file.data() + got, file.size() - got);
    if (n <= 0) break;
    got += static_cast<size_t>(n);
  }
  ::close(fd);
  if (got != file.size() || file.size() < sizeof(Elf64_Ehdr)) { out->error = "cannot read " + path; return false; }

  const auto* eh = reinterpret_cast<const Elf64_Ehdr*>(file.data());
  if (std::memcmp(eh->e_ident, ELFMAG, SELFMAG) != 0 || eh->e_ident[EI_CLASS] != ELFCLASS64 ||
      eh->e_machine != kMachine) {
    out->error = path + " is not a game library for this CPU";
    return false;
  }
  const auto* ph = reinterpret_cast<const Elf64_Phdr*>(file.data() + eh->e_phoff);

  // The span every segment lands in, at its link address.
  uintptr_t lo = UINTPTR_MAX, hi = 0;
  const Elf64_Phdr* dyn = nullptr;
  for (int i = 0; i < eh->e_phnum; ++i) {
    if (ph[i].p_type == PT_LOAD) {
      lo = std::min<uintptr_t>(lo, page_down(ph[i].p_vaddr));
      hi = std::max<uintptr_t>(hi, page_up(ph[i].p_vaddr + ph[i].p_memsz));
      if (ph[i].p_flags & PF_W) {
        out->rw_start = ph[i].p_vaddr;
        out->rw_data_end = ph[i].p_vaddr + ph[i].p_filesz;
        out->rw_end = ph[i].p_vaddr + ph[i].p_memsz;
      }
    } else if (ph[i].p_type == PT_DYNAMIC) {
      dyn = &ph[i];
    }
  }
  if (lo == UINTPTR_MAX || !dyn) { out->error = "no loadable segments"; return false; }
  void* at = ::mmap(reinterpret_cast<void*>(lo), hi - lo, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
  if (at == MAP_FAILED || reinterpret_cast<uintptr_t>(at) != lo) {
    char msg[160];
    std::snprintf(msg, sizeof msg, "the game's address range %#lx-%#lx is taken in this process",
                  static_cast<unsigned long>(lo), static_cast<unsigned long>(hi));
    out->error = msg;
    if (at != MAP_FAILED) ::munmap(at, hi - lo);
    return false;
  }
  out->base = lo;
  out->end = hi;
  for (int i = 0; i < eh->e_phnum; ++i) {
    if (ph[i].p_type != PT_LOAD) continue;
    std::memcpy(reinterpret_cast<void*>(ph[i].p_vaddr), file.data() + ph[i].p_offset, ph[i].p_filesz);
    // the anonymous mapping is zero already: that is the bss
  }

  // The dynamic section, now in place.
  const auto* d = reinterpret_cast<const Elf64_Dyn*>(dyn->p_vaddr);
  uintptr_t rela = 0, relasz = 0, jmprel = 0, pltrelsz = 0, symtab = 0, strtab = 0, hash = 0;
  uintptr_t init_array = 0, init_arraysz = 0;
  for (; d->d_tag != DT_NULL; ++d) {
    switch (d->d_tag) {
      case DT_RELA: rela = d->d_un.d_ptr; break;
      case DT_RELASZ: relasz = d->d_un.d_val; break;
      case DT_JMPREL: jmprel = d->d_un.d_ptr; break;
      case DT_PLTRELSZ: pltrelsz = d->d_un.d_val; break;
      case DT_SYMTAB: symtab = d->d_un.d_ptr; break;
      case DT_STRTAB: strtab = d->d_un.d_ptr; break;
      case DT_HASH: hash = d->d_un.d_ptr; break;
      case DT_INIT_ARRAY: init_array = d->d_un.d_ptr; break;
      case DT_INIT_ARRAYSZ: init_arraysz = d->d_un.d_val; break;
      default: break;
    }
  }
  if (!symtab || !strtab || !hash) { out->error = "no dynamic symbols (link with --hash-style=both)"; return false; }
  out->symtab = reinterpret_cast<const uint8_t*>(symtab);
  out->strtab = reinterpret_cast<const char*>(strtab);
  out->nsyms = reinterpret_cast<const uint32_t*>(hash)[1];   // DT_HASH: nbucket, nchain(=symbols)
  const auto* syms = reinterpret_cast<const Elf64_Sym*>(symtab);

  // The linker-defined bounds of the writable sections the game snapshots (PE linkers define them;
  // an ELF linker defines some of them on some targets): from the segment itself.
  auto special = [out](const char* name) -> void* {
    if (!std::strcmp(name, "__data_start__")) return reinterpret_cast<void*>(out->rw_start);
    if (!std::strcmp(name, "__data_end__") || !std::strcmp(name, "__bss_start__"))
      return reinterpret_cast<void*>(out->rw_data_end);
    if (!std::strcmp(name, "__bss_end__")) return reinterpret_cast<void*>(out->rw_end);
    return nullptr;
  };
  auto symbol_address = [&](uint32_t index) -> uintptr_t {
    const Elf64_Sym& s = syms[index];
    if (s.st_shndx != SHN_UNDEF) return s.st_value;
    const char* name = out->strtab + s.st_name;
    void* p = special(name);
    if (!p) p = resolve(name);
    if (!p) {
      if (g_unbound_count < 256) g_unbound_names[g_unbound_count++] = name;
      p = reinterpret_cast<void*>(&unbound_import);
    }
    return reinterpret_cast<uintptr_t>(p);
  };
  auto apply = [&](uintptr_t table, uintptr_t size) -> bool {
    const auto* r = reinterpret_cast<const Elf64_Rela*>(table);
    for (size_t i = 0; i < size / sizeof(Elf64_Rela); ++i) {
      auto* where = reinterpret_cast<uint64_t*>(r[i].r_offset);
      const uint32_t type = ELF64_R_TYPE(r[i].r_info);
      const uint32_t sym = ELF64_R_SYM(r[i].r_info);
      if (type == kRelRelative) {
        *where = static_cast<uint64_t>(r[i].r_addend);   // linked at its own address: B = 0
      } else if (type == kRelAbs || type == kRelGlobDat || type == kRelJumpSlot) {
        *where = symbol_address(sym) + (type == kRelAbs ? r[i].r_addend : 0);
      } else {
        char msg[96];
        std::snprintf(msg, sizeof msg, "unsupported relocation type %u", type);
        out->error = msg;
        return false;
      }
    }
    return true;
  };
  if ((rela && !apply(rela, relasz)) || (jmprel && !apply(jmprel, pltrelsz))) return false;

  // Final protections, segment by segment.
  for (int i = 0; i < eh->e_phnum; ++i) {
    if (ph[i].p_type != PT_LOAD) continue;
    int prot = 0;
    if (ph[i].p_flags & PF_R) prot |= PROT_READ;
    if (ph[i].p_flags & PF_W) prot |= PROT_WRITE;
    if (ph[i].p_flags & PF_X) prot |= PROT_EXEC;
    const uintptr_t a = page_down(ph[i].p_vaddr), e = page_up(ph[i].p_vaddr + ph[i].p_memsz);
    if (prot & PROT_EXEC) __builtin___clear_cache(reinterpret_cast<char*>(a), reinterpret_cast<char*>(e));
    ::mprotect(reinterpret_cast<void*>(a), e - a, prot);
  }
  // Constructors, if the image has any.
  if (init_array) {
    using Init = void (*)();
    auto* f = reinterpret_cast<Init*>(init_array);
    for (size_t i = 0; i < init_arraysz / sizeof(Init); ++i)
      if (f[i] && reinterpret_cast<uintptr_t>(f[i]) != ~uintptr_t(0)) f[i]();
  }
  return true;
}

}  // namespace platform
