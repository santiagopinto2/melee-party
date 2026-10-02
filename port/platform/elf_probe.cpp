// elf_probe: loads the ELF game library with the platform loader and checks it.
//
//   elf_probe <melee_game.so> [iterations]
//
// 1. Loads the image at its link address, binds its C library imports (platform::game_imports),
//    calls mu_game_entry with a minimal host, and prints the layout.
// 2. A first determinism check between CPUs: runs the game's own maths (MSL's trig and powf, the
//    decomp's vector routines, the spline helper, all compiled with -ffp-contract=on) over a fixed
//    sequence of inputs and prints a hash of every result's bits. The x86_64 build and the aarch64
//    build (under qemu-aarch64) must print the same hash; online play between a PC and an Android
//    device needs exactly that, everywhere in the game.
// SPDX-License-Identifier: GPL-3.0-or-later
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "elf_loader.h"
#include "game_imports.h"
#include "mu_host.h"

namespace {

struct Vec3 { float x, y, z; };

void probe_log(const char* text) { std::printf("[game] %s\n", text); }

uint64_t mix(uint64_t h, float f) {
  uint32_t bits;
  std::memcpy(&bits, &f, sizeof bits);
  h ^= bits;
  return h * 0x100000001b3ull;
}

// A fixed input stream, the same on every CPU (integers only).
struct Lcg {
  uint32_t s = 12345;
  float next(float lo, float hi) {
    s = s * 1664525u + 1013904223u;
    return lo + (hi - lo) * static_cast<float>(s >> 8) * (1.0f / 16777216.0f);
  }
};

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: elf_probe <melee_game.so> [iterations]\n");
    return 2;
  }
  const int iterations = argc > 2 ? std::atoi(argv[2]) : 200000;
  platform::ElfImage image;
  if (!platform::load_elf_image(argv[1], platform::game_imports, &image)) {
    std::fprintf(stderr, "load failed: %s\n", image.error.c_str());
    return 1;
  }
  std::printf("loaded %#lx-%#lx, writable %#lx-%#lx (data to %#lx)\n", (unsigned long)image.base,
              (unsigned long)image.end, (unsigned long)image.rw_start, (unsigned long)image.rw_end,
              (unsigned long)image.rw_data_end);

  using Entry = int32_t (*)(const MuHostApi*, MuGameApi*);
  auto entry = reinterpret_cast<Entry>(image.symbol("mu_game_entry"));
  if (!entry) { std::fprintf(stderr, "no mu_game_entry\n"); return 1; }
  static MuHostApi host{};
  static MuGameApi game{};
  host.version = MU_HOST_API_VERSION;
  host.log = probe_log;
  const int32_t rc = entry(&host, &game);
  std::printf("mu_game_entry -> %d, game API version %u\n", rc, game.version);

  using F1 = float (*)(float);
  using F2 = float (*)(float, float);
  using V1 = float (*)(Vec3*);
  using V2 = float (*)(Vec3*, Vec3*);
  using V3 = Vec3* (*)(Vec3*, Vec3*, Vec3*);
  using H6 = float (*)(float, float, float, float, float, float);
  auto sinf_ = reinterpret_cast<F1>(image.symbol("sinf"));
  auto cosf_ = reinterpret_cast<F1>(image.symbol("cosf"));
  auto tanf_ = reinterpret_cast<F1>(image.symbol("tanf"));
  auto atanf_ = reinterpret_cast<F1>(image.symbol("atanf"));
  auto acosf_ = reinterpret_cast<F1>(image.symbol("acosf"));
  auto atan2f_ = reinterpret_cast<F2>(image.symbol("atan2f"));
  auto powf_ = reinterpret_cast<F2>(image.symbol("powf"));
  auto norm = reinterpret_cast<V1>(image.symbol("lbVector_Normalize"));
  auto angle = reinterpret_cast<V2>(image.symbol("lbVector_Angle"));
  auto cross = reinterpret_cast<V3>(image.symbol("lbVector_CrossprodNormalized"));
  auto helmite = reinterpret_cast<H6>(image.symbol("splGetHelmite"));
  if (!sinf_ || !cosf_ || !tanf_ || !atanf_ || !acosf_ || !atan2f_ || !powf_ || !norm || !angle ||
      !cross || !helmite) {
    std::fprintf(stderr, "a maths routine is missing from the image\n");
    return 1;
  }

  // One hash per routine, so a difference between CPUs names the routine.
  enum { SIN, COS, TAN, ATAN, ACOS, ATAN2, POW, ANGLE, CROSS, NORM, HELMITE, N };
  static const char* const names[N] = {"sinf", "cosf", "tanf", "atanf", "acosf", "atan2f", "powf",
                                       "lbVector_Angle", "lbVector_CrossprodNormalized",
                                       "lbVector_Normalize", "splGetHelmite"};
  uint64_t h[N];
  for (auto& x : h) x = 0xcbf29ce484222325ull;
  Lcg in;
  int first_diff_dump = std::getenv("PROBE_DUMP") ? std::atoi(std::getenv("PROBE_DUMP")) : -1;
  for (int i = 0; i < iterations; ++i) {
    float a = in.next(-10.0f, 10.0f), b = in.next(-10.0f, 10.0f), c = in.next(-1.0f, 1.0f);
    float r[16];
    int k = 0;
    h[SIN] = mix(h[SIN], r[k++] = sinf_(a));
    h[COS] = mix(h[COS], r[k++] = cosf_(b));
    h[TAN] = mix(h[TAN], r[k++] = tanf_(c));
    h[ATAN] = mix(h[ATAN], r[k++] = atanf_(a));
    h[ACOS] = mix(h[ACOS], r[k++] = acosf_(c));
    h[ATAN2] = mix(h[ATAN2], r[k++] = atan2f_(a, b));
    h[POW] = mix(h[POW], r[k++] = powf_(0.1f + (c < 0 ? -c : c), b * 0.2f));
    Vec3 u{a, b, c}, v{b, c, a}, w{};
    h[ANGLE] = mix(h[ANGLE], r[k++] = angle(&u, &v));
    cross(&u, &v, &w);
    h[CROSS] = mix(mix(mix(h[CROSS], w.x), w.y), w.z);
    h[NORM] = mix(h[NORM], r[k++] = norm(&u));
    h[NORM] = mix(mix(mix(h[NORM], u.x), u.y), u.z);
    // Drawn into variables first: argument evaluation order differs between compilers.
    const float p1 = in.next(-5, 5), d0 = in.next(-5, 5), d1 = in.next(-5, 5);
    h[HELMITE] = mix(h[HELMITE], r[k++] = helmite(c, a, b, p1, d0, d1));
    if (i == first_diff_dump) {
      std::printf("iteration %d inputs a=%a b=%a c=%a\n", i, a, b, c);
      for (int j = 0; j < k; ++j) std::printf("  r%d = %a\n", j, r[j]);
      std::printf("  cross = %a %a %a\n", w.x, w.y, w.z);
    }
  }
  for (int j = 0; j < N; ++j) std::printf("%-30s %016llx\n", names[j], (unsigned long long)h[j]);
  return 0;
}
