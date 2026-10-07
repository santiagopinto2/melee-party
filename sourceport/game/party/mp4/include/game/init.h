/* MP4's video setup (partyboard include/game/init.h, CC0): only what the model layer reads, the
 * render mode (Melee's own here) and the frame length (mp4_stubs.c). */
#ifndef _GAME_INIT_H
#define _GAME_INIT_H

#include <dolphin/gx.h>

extern GXRenderModeObj *RenderMode;
/* MP4 ran its game logic at 60 Hz: one retrace a frame. */
extern float minimumVcountf;

#endif
