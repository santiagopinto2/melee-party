/* Melee Party, Mario Party 4 runtime: Fish n' Drips (m420), from the MP4 decompilation
 * (github.com/mariopartyrd/partyboard src/REL/m420dll/rand.c, CC0). */
#include "dolphin/types.h"

u32 lbl_1_bss_A48;

void fn_1_8934 (u32 seed) {
    lbl_1_bss_A48 = seed;
}

s32 fn_1_8944(void) {
    return (lbl_1_bss_A48 = (lbl_1_bss_A48 * 0x41C64E6D) + 0x3039) / 65536 & 0x7FFF;
}