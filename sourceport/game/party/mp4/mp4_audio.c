/* Melee Party, Mario Party 4 runtime: MP4's sound (audio.c) is not played: the minigames' sound
 * effects, music and voices are MP4's own and are not part of this port. Every call plays nothing
 * and returns "no sound". */
#include <dolphin/types.h>

#include "game/audio.h"

void HuAudFadeOut(s32 speed) { (void) speed; }
int HuAudFXPlay(int seId) { (void) seId; return -1; }
int HuAudFXPlayVol(int seId, s16 vol) { (void) seId; (void) vol; return -1; }
int HuAudFXPlayVolPan(int seId, s16 vol, s16 pan) { (void) seId; (void) vol; (void) pan; return -1; }
void HuAudFXStop(int seNo) { (void) seNo; }
void HuAudFXAllStop(void) {}
void HuAudFXPanning(int seNo, s16 pan) { (void) seNo; (void) pan; }
void HuAudFXListnerSet(Vec *pos, Vec *heading, float sndDist, float sndSpeed)
{
    (void) pos; (void) heading; (void) sndDist; (void) sndSpeed;
}
void HuAudFXListnerUpdate(Vec *pos, Vec *heading) { (void) pos; (void) heading; }
void HuAudFXListnerKill(void) {}
void HuAudFXListnerSetEX(Vec *pos, Vec *heading, float sndDist, float sndSpeed, float startDis,
                         float frontSurDis, float backSurDis)
{
    (void) pos; (void) heading; (void) sndDist; (void) sndSpeed; (void) startDis; (void) frontSurDis;
    (void) backSurDis;
}
int HuAudFXEmiterPlay(int seId, Vec *pos) { (void) seId; (void) pos; return -1; }
void HuAudFXEmiterUpDate(int seNo, Vec *pos) { (void) seNo; (void) pos; }
void HuAudFXPauseAll(BOOL pauseF) { (void) pauseF; }
s32 HuAudFXVolSet(int seNo, s16 vol) { (void) seNo; (void) vol; return -1; }
s32 HuAudSeqPlay(s16 musId) { (void) musId; return -1; }
void HuAudSeqFadeOut(s32 musNo, s32 speed) { (void) musNo; (void) speed; }
void HuAudSeqAllFadeOut(s32 speed) { (void) speed; }
void HuAudSeqPauseAll(BOOL pause) { (void) pause; }
s32 HuAudSStreamPlay(s16 streamId) { (void) streamId; return -1; }
void HuAudDllSndGrpSet(u16 ovl) { (void) ovl; }
void HuAudSndGrpSet(s16 grpId) { (void) grpId; }
void HuAudSndCharGrpSet(s16 ovl) { (void) ovl; }
