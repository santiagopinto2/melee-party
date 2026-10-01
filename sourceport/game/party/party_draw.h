/* Melee Party: flat shapes in the match world (party_draw.c). */
#ifndef MU_PARTY_DRAW_H
#define MU_PARTY_DRAW_H

#include <dolphin/gx.h>

/* Once per match: cb runs every frame inside the match camera's pass. */
void party_draw_init(void (*cb)(void));
void party_draw_quad(GXColor color, float x0, float y0, float x1, float y1, float z);
void party_draw_disc(GXColor color, float x, float y, float rx, float rz);

#endif
