/* Melee Party, Mario Party 4 runtime: sprite animation structures (ANIMDATA and what it points
 * at) from their file form (big-endian, 32-bit offsets) into the sprite system's 64-bit ones. A C
 * port of the sprite half of partyboard's src/port/byteswap.cpp (github.com/mariopartyrd/partyboard,
 * CC0), in the manner of hsf_byteswap.c: the file structure is swapped in place and copied, its
 * offsets left as numbers for HuSprAnimRead to resolve. */
#include <string.h>
#include "port/byteswap.h"

#define PTR(T, v) ((T) (uintptr_t) (v))

static void sw16(void* p)
{
    u8* b = p;
    u8 t = b[0];
    b[0] = b[1];
    b[1] = t;
}

static void sw32(void* p)
{
    u8* b = p;
    u8 t = b[0];
    b[0] = b[3];
    b[3] = t;
    t = b[1];
    b[1] = b[2];
    b[2] = t;
}

void byteswap_animdata(void* src, ANIMDATA* dest)
{
    AnimData32b* a = src;
    sw16(&a->bankNum);
    sw16(&a->patNum);
    sw16(&a->bmpNum);
    sw16(&a->useNum);
    sw32(&a->bank);
    sw32(&a->pat);
    sw32(&a->bmp);
    memset(dest, 0, sizeof *dest);
    dest->bankNum = a->bankNum;
    dest->patNum = a->patNum;
    dest->bmpNum = a->bmpNum;
    dest->useNum = a->useNum;
    dest->bank = PTR(ANIMBANK*, a->bank);
    dest->pat = PTR(ANIMPAT*, a->pat);
    dest->bmp = PTR(ANIMBMP*, a->bmp);
}

void byteswap_animbankdata(AnimBankData32b* src, ANIMBANK* dest)
{
    sw16(&src->timeNum);
    sw16(&src->unk);
    sw32(&src->frame);
    dest->timeNum = src->timeNum;
    dest->unk = src->unk;
    dest->frame = PTR(ANIMFRAME*, src->frame);
}

void byteswap_animpatdata(AnimPatData32b* src, ANIMPAT* dest)
{
    sw16(&src->layerNum);
    sw16(&src->centerX);
    sw16(&src->centerY);
    sw16(&src->sizeX);
    sw16(&src->sizeY);
    sw32(&src->layer);
    dest->layerNum = src->layerNum;
    dest->centerX = src->centerX;
    dest->centerY = src->centerY;
    dest->sizeX = src->sizeX;
    dest->sizeY = src->sizeY;
    dest->layer = PTR(ANIMLAYER*, src->layer);
}

void byteswap_animbmpdata(AnimBmpData32b* src, ANIMBMP* dest)
{
    sw16(&src->palNum);
    sw16(&src->sizeX);
    sw16(&src->sizeY);
    sw32(&src->dataSize);
    sw32(&src->palData);
    sw32(&src->data);
    memset(dest, 0, sizeof *dest);
    dest->pixSize = src->pixSize;
    dest->dataFmt = src->dataFmt;
    dest->palNum = src->palNum;
    dest->sizeX = src->sizeX;
    dest->sizeY = src->sizeY;
    dest->dataSize = src->dataSize;
    dest->palData = PTR(void*, src->palData);
    dest->data = PTR(void*, src->data);
}

void byteswap_animframedata(ANIMFRAME* src)
{
    sw16(&src->pat);
    sw16(&src->time);
    sw16(&src->shiftX);
    sw16(&src->shiftY);
    sw16(&src->flip);
    sw16(&src->pad);
}

void byteswap_animlayerdata(ANIMLAYER* src)
{
    s32 i;
    sw16(&src->bmpNo);
    sw16(&src->startX);
    sw16(&src->startY);
    sw16(&src->sizeX);
    sw16(&src->sizeY);
    sw16(&src->shiftX);
    sw16(&src->shiftY);
    for (i = 0; i < 8; i++) {
        sw16(&src->vtx[i]);
    }
}
