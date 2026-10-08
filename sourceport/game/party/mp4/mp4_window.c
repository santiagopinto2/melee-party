/* Melee Party, Mario Party 4 runtime: MP4's message windows (window.c, with its fonts and message
 * data) are not ported. The minigame banners use them only for the pause menu, which Melee's own
 * pause replaces; every call here makes no window. */
#include <dolphin/types.h>

#include "game/armem.h"
#include "game/data.h"
#include "game/esprite.h"
#include "game/window.h"

void HuWinInit(s32 mess_data_no) { (void) mess_data_no; }
void HuWinKill(s16 window) { (void) window; }
void HuWinAllKill(void) {}
void HuWinPosSet(s16 window, float x, float y) { (void) window; (void) x; (void) y; }
void HuWinPriSet(s16 window, s16 prio) { (void) window; (void) prio; }
void HuWinAttrSet(s16 window, u32 attr) { (void) window; (void) attr; }
void HuWinMesSpeedSet(s16 window, s16 speed) { (void) window; (void) speed; }
void HuWinMesSet(s16 window, u32 mess) { (void) window; (void) mess; }
void HuWinInsertMesSet(s16 window, u32 mess, s16 index) { (void) window; (void) mess; (void) index; }
void HuWinMesWait(s16 window) { (void) window; }
void HuWinDispOn(s16 window) { (void) window; }
s16 HuWinExCreateStyled(float x, float y, s16 w, s16 h, s16 portrait, s16 frame)
{
    (void) x; (void) y; (void) w; (void) h; (void) portrait; (void) frame;
    return -1;
}
void HuWinExAnimIn(s16 window) { (void) window; }

/* ARAM (armem.c): nothing is ever moved there, so no transfer is ever in progress; a file "from
 * ARAM" is read from the disc. The sound fade (audio.c) is never in progress either. */
s32 HuARDMACheck(void) { return 0; }
AMEM_PTR HuAR_DVDtoARAM(u32 dir) { (void) dir; return 0; }
void *HuAR_ARAMtoMRAMFileRead(u32 dir, u32 num, HeapID heap)
{
    return HuDataSelHeapReadNum((s32) dir, (s32) num, heap);
}
u8 fadeStat;

/* Extended sprites (esprite.c): none. Booksquirm's one-player record counter asks for them
 * (lbl_1_bss_2, never set in the four-player game). */
void espInit(void) {}
s16 espEntry(unsigned int dataNum, s16 prio, s16 bank)
{
    (void) dataNum;
    (void) prio;
    (void) bank;
    return -1;
}
void espPosSet(s16 espId, float posX, float posY)
{
    (void) espId;
    (void) posX;
    (void) posY;
}
void espTPLvlSet(s16 espId, float tpLvl)
{
    (void) espId;
    (void) tpLvl;
}
void espColorSet(s16 espId, u8 r, u8 g, u8 b)
{
    (void) espId;
    (void) r;
    (void) g;
    (void) b;
}
void espBankSet(s16 espId, s16 bank)
{
    (void) espId;
    (void) bank;
}
