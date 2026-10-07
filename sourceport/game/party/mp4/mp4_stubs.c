/* Melee Party, Mario Party 4 runtime: the parts of MP4 the model layer calls into that are not
 * ported yet, each standing in as "nothing there". They come in milestone 1b:
 * - sprites (sprman.c, sprput.c): no sprite is drawn, and HuSprAnimRead has no texture to read;
 * - texture animations and particles (hsfanim.c): models draw with their first frame;
 * - processes (process.c): HuPrcVSleep returns at once;
 * - performance meters (perf.c). */
#include <dolphin/types.h>
#include <string.h>

#include "game/hu3d.h"
#include "game/init.h"
#include "game/sprite.h"

HU3DTEXANIM Hu3DTexAnimData[HU3D_TEXANIM_MAX];
HU3DTEXSCROLL Hu3DTexScrData[HU3D_TEXSCROLL_MAX];

ANIMDATA *HuSprAnimRead(void *data)
{
    (void) data;
    return NULL;
}

void HuSprAnimKill(ANIMDATA *anim) { (void) anim; }

void HuSprTexLoad(ANIMDATA *anim, s16 bmp, s16 slot, GXTexWrapMode wrap_s, GXTexWrapMode wrap_t, GXTexFilter filter)
{
    (void) anim;
    (void) bmp;
    (void) slot;
    (void) wrap_s;
    (void) wrap_t;
    (void) filter;
}

void HuSprBegin(void) {}
void HuSprDispInit(void) {}
void HuSprExec(s16 draw_no) { (void) draw_no; }
void HuSprFinish(void) {}

void Hu3DAnimInit(void)
{
    memset(Hu3DTexAnimData, 0, sizeof Hu3DTexAnimData);
    memset(Hu3DTexScrData, 0, sizeof Hu3DTexScrData);
}

void Hu3DAnimExec(void) {}
void Hu3DAnimAllKill(void) {}
void Hu3DAnimModelKill(HU3DMODELID modelId) { (void) modelId; }

s32 Hu3DAnimSet(HU3DMODEL *modelP, HSFATTRIBUTE *attrP, s16 texSlotNo)
{
    (void) modelP;
    (void) attrP;
    (void) texSlotNo;
    return 0;
}

void Hu3DParManInit(void) {}
void Hu3DParManAllKill(void) {}

void HuPrcVSleep(void) {}

void HuPerfBegin(s32 arg0) { (void) arg0; }
void HuPerfEnd(s32 arg0) { (void) arg0; }


/* The render mode the cameras size their viewports by: Melee's. */
extern GXRenderModeObj GXNtsc480IntDf;
GXRenderModeObj *RenderMode = &GXNtsc480IntDf;
float minimumVcountf = 1.0f;
