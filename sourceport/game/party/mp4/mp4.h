/* Melee Party, Mario Party 4 runtime (sourceport/game/party/mp4): MP4's own code, ported from the
 * MP4 decompilation (github.com/mariopartyrd/partyboard, CC0), loading MP4's own data from the
 * player's disc and drawing it in a Melee match.
 *
 * Everything here stays off unless the host found a Mario Party 4 (USA) disc whose header checks
 * out: without one, mp4_available() is 0 and nothing else is called. */
#ifndef MU_PARTY_MP4_H
#define MU_PARTY_MP4_H

#include <dolphin/types.h>

/* Opens the disc and the heaps once (the first call); 1 when MP4 data can be loaded. */
int mp4_available(void);
int mp4_mem_ready(void);
int mp4_mem_fits(void);                 /* the heaps are where GX can read them */

/* An archive's data number (DATADIR_* << 16) by its name ("m440", "data/m440.bin"), or -1. */
s32 mp4_data_dir(const char* name);

/* A party match is set up: MELEE_PARTY_MP4_MODEL draws an MP4 model in it (mp4_party.c). */
struct StartMeleeData;
void mp4_debug_setup(struct StartMeleeData* start);

/* ---- an MP4 minigame in a Melee match (mp4_party.c), for its wrapper (mg_mp4_*.c) ---- */

#define MP4_SCALE 0.1f   /* MP4's units into Melee's */

/* From the wrapper's on_match_start: MP4's runtime is up, and the overlay (an OMOVL, game/object.h)
 * starts next frame. offset: where MP4's origin sits in Melee's world, clear of the stage. */
void mp4_match_begin(int overlay, float offset_x, float offset_y, float offset_z);
/* From the wrapper's on_frame_start: MP4's processes run a frame (the minigame's objects, its
 * banners), and MP4's camera goes to the Melee match camera. */
void mp4_frame(void);
/* 1 once the overlay returned (omOvlReturnEx): the minigame is over. */
int mp4_match_over(void);
/* A human's pad this frame, as MP4 reads it (from the wrapper's fighter_input). */
void mp4_pad(int pad, u32 held, float stick_x, float stick_y, float substick_x, float substick_y,
             float trigger);
/* Where a player's MP4 model stands, in Melee's world (with the offset), and its yaw in radians;
 * 0 if the player has no model yet. */
int mp4_player_pose(int player, float* x, float* y, float* z, float* yaw);
/* The player's current MP4 motion, a Hu3D motion id (-1: none). */
s32 mp4_player_motion(int player);
/* The coins MP4 awarded the player (GWPlayerCoinWinSet). */
int mp4_player_coins(int player);
/* MP4's camera (the first Hu3D camera) in Melee's world; 0 if there is none yet. */
int mp4_camera(float eye[3], float look[3], float* fov);

/* mp4_ovl.c: the overlay numbers (OMOVL) of the minigames linked in. */
extern const int mp4_overlay_m440;

/* mp4_char.c: the player's hidden model, a Hu3D model id or -1. */
s16 mp4_char_model(int charNo);
/* mp4_ovl.c: the boot overlay, the one a minigame returns to. */
int mp4_boot_reached(void);
void mp4_boot_reset(void);

#endif
