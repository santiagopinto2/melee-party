/* Melee Party, Mario Party 4 runtime (sourceport/game/party/mp4): MP4's own code, ported from the
 * MP4 decompilation (github.com/mariopartyrd/partyboard, CC0), loading MP4's own data from the
 * player's disc and drawing it in a Melee match.
 *
 * Everything here stays off unless the host found a Mario Party 4 (USA) disc whose header checks
 * out: without one, mp4_available() is 0 and nothing else is called. */
#ifndef MU_PARTY_MP4_H
#define MU_PARTY_MP4_H

#include <dolphin/types.h>

/* Opens the disc and the heaps once (the first call); 1 when MP4 data can be loaded. */
int mp4_available(void);
int mp4_mem_ready(void);
int mp4_mem_fits(void);                 /* the heaps are where GX can read them */

/* An archive's data number (DATADIR_* << 16) by its name ("m440", "data/m440.bin"), or -1. */
s32 mp4_data_dir(const char* name);

/* A party match is set up: MELEE_PARTY_MP4_MODEL draws an MP4 model in it (mp4_party.c). */
struct StartMeleeData;
void mp4_debug_setup(struct StartMeleeData* start);

#endif
