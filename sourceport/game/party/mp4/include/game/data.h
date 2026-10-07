/* Mario Party 4's data archives (mp4_dir.c), from partyboard (github.com/mariopartyrd/partyboard
 * include/game/data.h, CC0). Every read is synchronous here: the asynchronous calls finish at
 * once. */
#ifndef _GAME_DATA_H
#define _GAME_DATA_H

#include <dolphin/types.h>

#include "datadir_enum.h"
#include "game/memory.h"

#define DATA_DECODE_NONE 0
#define DATA_DECODE_LZ 1
#define DATA_DECODE_SLIDE 2
#define DATA_DECODE_FSLIDE_ALT 3
#define DATA_DECODE_FSLIDE 4
#define DATA_DECODE_RLE 5

#define DATA_NUM_LISTEND -1U
#define HU_DATANUM_NONE -1
#define HU_DATA_STAT_NONE -1

s32 HuDataReadChk(s32 dataNum);
void *HuDataGetDirPtr(s32 dataNum);
s32 HuDataDirReadAsync(s32 dataNum);
s32 HuDataDirReadNumAsync(s32 dataNum, s32 num);
BOOL HuDataGetAsyncStat(s32 statId);
void *HuDataRead(s32 dataNum);
void *HuDataReadNum(s32 dataNum, s32 num);
void *HuDataSelHeapRead(s32 dataNum, HeapID heap);
void *HuDataSelHeapReadNum(s32 dataNum, s32 num, HeapID heap);
void **HuDataReadMulti(s32 *dataNum);
s32 HuDataGetSize(s32 dataNum);
void HuDataClose(void *ptr);
void HuDataCloseMulti(void **ptrs);
void HuDataDirClose(s32 dataNum);
void HuDataDirCloseNum(s32 num);
void *HuDataReadNumHeapShortForce(s32 dataNum, s32 num, HeapID heap);

/* A whole file of the disc by its path ("data/xxx.bin"), in the DVD heap: HuDvdDataClose. */
void *HuDvdDataRead(char *path);
void HuDvdDataClose(void *ptr);

#endif
