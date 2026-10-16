/* Melee Party, Mario Party 4 runtime: the parts of MP4 the ported layers call into that are not
 * ported, each standing in as "nothing there":
 * - performance meters (perf.c);
 * - the particle manager's init and kill (hsfanim.c has the particles themselves). */
#include <dolphin/types.h>

#include "game/hu3d.h"
#include "game/init.h"

void HuPerfBegin(s32 arg0) { (void) arg0; }
void HuPerfEnd(s32 arg0) { (void) arg0; }

/* The render mode the cameras size their viewports by: Melee's. */
extern GXRenderModeObj GXNtsc480IntDf;
GXRenderModeObj *RenderMode = &GXNtsc480IntDf;
u32 minimumVcount = 1;
float minimumVcountf = 1.0f;
