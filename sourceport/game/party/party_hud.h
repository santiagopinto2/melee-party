/* Melee Party: text over a match (party_hud.c). */
#ifndef MU_PARTY_HUD_H
#define MU_PARTY_HUD_H

#include <dolphin/gx.h>
#include <sysdolphin/baselib/forward.h>

#define PARTY_WHITE ((GXColor) { 255, 255, 255, 255 })
#define PARTY_GOLD ((GXColor) { 255, 210, 60, 255 })
#define PARTY_RED ((GXColor) { 255, 80, 80, 255 })
#define PARTY_BLUE ((GXColor) { 90, 150, 255, 255 })

void party_hud_init(void);            /* once per match, from on_match_start */
HSD_Text* party_hud_text(void);       /* NULL if the HUD could not be created */
int party_hud_line(HSD_Text* t, float x, float y, float scale, GXColor color);
void party_hud_set(HSD_Text* t, int idx, const char* fmt, ...);
void party_hud_color(HSD_Text* t, int idx, GXColor color);

#endif
