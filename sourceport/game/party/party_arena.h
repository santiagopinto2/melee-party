/* Melee Party: a Melee fighter as the player of a Mario Party 4 minigame (party_arena.c).
 *
 * The minigame's own player code runs as MP4 wrote it, on a hidden MP4 model: it reads the stick
 * (a human's pad, or its CPU), turns the player, walks it, keeps it inside the map, hits it and
 * puts it out. The fighter stands where that model stands and faces its way. What Melee owns is
 * the walking: while the minigame has the player walking by the stick, the fighter gets the
 * stick's strength as a push forward, Melee's physics give it its speed (each fighter's own walk,
 * dash and run, its skid when the stick lets go), and the minigame's player moves at that speed
 * (mp4_player_speed, which the minigame's walk code calls in place of its own speed). The rest of
 * the time (falling in, hit, out, won, lost) the minigame moves the player and the fighter
 * follows, with the nearest Melee animation.
 *
 * A wrapper (mg_mp4_<name>.c) calls these from its PartyMinigame hooks, in this order each frame:
 * party_arena_frame from on_frame_start, then for every fighter party_arena_input_begin and either
 * party_arena_walk or party_arena_follow from fighter_input, party_arena_map from fighter_map and
 * party_arena_drawn from fighter_drawn. */
#ifndef MU_PARTY_ARENA_H
#define MU_PARTY_ARENA_H

#include <dolphin/types.h>

struct Fighter;
struct HSD_CObj;

/* on_match_start: the arena forgets the last match's players and starts the minigame's overlay
 * (mp4_match_begin), with MP4's origin at the offset in Melee's world. */
void party_arena_begin(int overlay, float offset_x, float offset_y, float offset_z);
/* on_frame_start: MP4's frame, then its camera on the match camera. */
void party_arena_frame(void);

/* fighter_input, first: a human's pad goes to MP4, Melee's inputs are cleared, and the fighter's
 * model stands where it collides (Melee works the collision box out from the bones). 0 for a
 * fighter the arena does not drive (no slot, or a sub fighter). */
int party_arena_input_begin(struct Fighter* fp);
/* The player walks by the stick this frame: the stick as MP4 read it (-72..72 each way), as a
 * push forward for Melee. */
void party_arena_walk(struct Fighter* fp, s16 stick_x, s16 stick_y);
/* The minigame moves the player this frame (it is falling, hit, out, won or lost): the fighter
 * only follows, in the air when the player is (MP4's y), and in Melee's animation for the MP4
 * motion the player's model is in. */
void party_arena_follow(struct Fighter* fp, int airborne);
/* The player was hit: the fighter goes flying (once per hit). */
void party_arena_hit(struct Fighter* fp);

/* fighter_map: the fighter collides on its own lane of Final Destination; while the minigame
 * moves the player, at the player's height. Melee's forward speed is kept for mp4_player_speed. */
void party_arena_map(struct Fighter* fp);
/* fighter_drawn: drawn at the MP4 player's pose, plus whatever height Melee gave it; out of sight
 * once the game hid the player. */
void party_arena_drawn(struct Fighter* fp);

/* camera_views and camera_view, for a minigame that splits the screen: one view per Hu3D camera
 * the minigame uses, each with that camera's eye, fov and viewport. */
int party_arena_camera_views(void);
void party_arena_camera_view(int view, struct HSD_CObj* cobj);

#endif
