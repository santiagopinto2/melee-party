/* Melee Party: flat shapes in the match world (party_draw.c). */
#ifndef MU_PARTY_DRAW_H
#define MU_PARTY_DRAW_H

#include <dolphin/gx.h>

/* Once per match: cb runs every frame inside the match camera's pass. */
void party_draw_init(void (*cb)(void));
/* An upright rectangle facing the camera's default view, at depth z. */
void party_draw_quad(GXColor color, float x0, float y0, float x1, float y1, float z);
/* A flat disc lying on the floor at height y, centred on (x, z). */
void party_draw_disc(GXColor color, float x, float y, float z, float rx, float rz);
/* A flat disc of `segments` sides: discs too big for party_draw_disc's 24. */
void party_draw_disc_n(GXColor color, float x, float y, float z, float r, int segments);
/* An upright five-pointed star facing the camera's default view, centred on (x, y, z). */
void party_draw_star(GXColor color, float x, float y, float z, float r);
/* A flat four-cornered shape on the floor at height y, corners given as (x, z) pairs. */
void party_draw_floor_quad(GXColor color, float y, const float corners[8]);

#endif
