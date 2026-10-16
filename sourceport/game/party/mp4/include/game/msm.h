/* Melee Party, Mario Party 4 runtime: the music and sound manager's sound-effect parameters
 * (include/game/msm.h in the MP4 decompilation, github.com/mariopartyrd/partyboard, CC0), the
 * part MP4's minigames call. MP4 sound is stubbed here (mp4_audio.c). */
#ifndef MP4_GAME_MSM_H
#define MP4_GAME_MSM_H

#include "dolphin.h"

#define MSM_SEPARAM_NONE 0
#define MSM_SEPARAM_VOL (1 << 0)
#define MSM_SEPARAM_PAN (1 << 1)
#define MSM_SEPARAM_PITCH (1 << 2)
#define MSM_SEPARAM_SPAN (1 << 3)
#define MSM_SEPARAM_AUXVOLA (1 << 4)
#define MSM_SEPARAM_AUXVOLB (1 << 5)
#define MSM_SEPARAM_POS (1 << 6)
#define MSM_SEPARAM_PAD (1 << 7)

typedef struct msmSeParam_s {
    s32 flag;
    s8 vol;
    s8 pan;
    s16 pitch;
    u8 span;
    s8 auxAVol;
    s8 auxBVol;
    s32 pad;
    Vec pos;
} MSM_SEPARAM;

s32 msmSeSetParam(int seNo, MSM_SEPARAM* param);
int msmSePlay(int seId, MSM_SEPARAM* param);
s32 msmSeStop(int seNo, s32 speed);
s32 msmSeGetStatus(int seNo);

#endif
