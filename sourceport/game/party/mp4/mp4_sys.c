/* Melee Party, Mario Party 4 runtime: MP4's global state that its minigames read, filled by the
 * match instead of by MP4's own systems:
 * - the pads (pad.c): HuPad*, from the Melee fighters' inputs each frame (mp4_party.c);
 * - the game work (gamework.c): who plays, with which pad, human or CPU and at what level;
 * - the save flags (flag.c): none set, so no practice mode and no tutorial;
 * - what main.c keeps: the frame counter, the 8-bit random numbers, the vsync wait;
 * - the character sound effects (chrman.c) and the debug print (printfunc.c): nothing. */
#include <dolphin/types.h>

#include <string.h>

#include "game/chrman.h"
#include "game/flag.h"
#include "game/gamework_data.h"
#include "game/pad.h"
#include "game/printfunc.h"

u16 HuPadBtn[4];
u16 HuPadBtnDown[4];
u16 HuPadBtnRep[4];
s8 HuPadStkX[4];
s8 HuPadStkY[4];
s8 HuPadSubStkX[4];
s8 HuPadSubStkY[4];
u8 HuPadTrigL[4];
u8 HuPadTrigR[4];
u8 HuPadDStk[4];
u8 HuPadDStkRep[4];
s8 HuPadErr[4];

/* Melee Party: nothing held from the last match (a slot that was human then stays still). */
void mp4_pad_reset(void)
{
    memset(HuPadBtn, 0, sizeof HuPadBtn);
    memset(HuPadBtnDown, 0, sizeof HuPadBtnDown);
    memset(HuPadBtnRep, 0, sizeof HuPadBtnRep);
    memset(HuPadStkX, 0, sizeof HuPadStkX);
    memset(HuPadStkY, 0, sizeof HuPadStkY);
    memset(HuPadSubStkX, 0, sizeof HuPadSubStkX);
    memset(HuPadSubStkY, 0, sizeof HuPadSubStkY);
    memset(HuPadTrigL, 0, sizeof HuPadTrigL);
    memset(HuPadTrigR, 0, sizeof HuPadTrigR);
    memset(HuPadDStk, 0, sizeof HuPadDStk);
    memset(HuPadDStkRep, 0, sizeof HuPadDStkRep);
}

PlayerConfig GWPlayerCfg[4];
PlayerState GWPlayer[4];
SystemState GWSystem;
GameStat GWGameStat;

/* The minigame records (gamework.c), kept for the session: a one-player Booksquirm compares its
 * page count with them. */
void GWMGRecordSet(s32 index, u32 value)
{
    GWGameStat.mg_record[index] = value;
}

u32 GWMGRecordGet(s32 index)
{
    return GWGameStat.mg_record[index];
}

u32 GlobalCounter;
extern u32 minimumVcount;

static s32 rnd_seed = 0x0000D9ED;

s32 rand8(void)
{
    rnd_seed = (rnd_seed * 0x41C64E6D) + 0x3039;
    return (u8) (((rnd_seed + 1) >> 16) & 0xFF);
}

s16 HuSysVWaitGet(s16 old)
{
    (void) old;
    return (s16) minimumVcount;
}

s16 HuPadStatGet(s16 pad)
{
    return HuPadErr[pad];
}

void HuPadRumbleSet(s16 pad, s16 duration, s16 off, s16 on)
{
    (void) pad; (void) duration; (void) off; (void) on;
}

s32 CharFXPlay(s16 charNo, s16 seId)
{
    (void) charNo; (void) seId;
    return -1;
}

int fontcolor;
s16 print8(s16 x, s16 y, float scale, char *str, ...)
{
    (void) x; (void) y; (void) scale; (void) str;
    return 0;
}
s16 printWin(s16 x, s16 y, s16 w, s16 h, GXColor *color)
{
    (void) x; (void) y; (void) w; (void) h; (void) color;
    return 0;
}

s32 _CheckFlag(u32 flag)
{
    (void) flag;
    return 0;
}

void HuPadRumbleAllStop(void) {}
