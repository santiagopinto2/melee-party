/* Melee Party: Butterfly Blitz, Mario Party 4's m441, in a Melee match.
 *
 * The minigame is MP4's own code (mp4/m441), run by the MP4 runtime (mp4/mp4_party.c): the round
 * meadow, the butterflies, the nets, its rules, timer, CPUs, score counters and banners. The four
 * players are MP4's player objects, each driving a hidden MP4 character model; the Melee fighters
 * stand where those models stand, through party_arena.c. While a player is free to walk, its
 * fighter walks by Melee's physics and the player moves at the fighter's speed; a swing of the
 * net (A high, B low) is MP4's, on the hidden model, and the fighter jabs with it; the rest of
 * the time (the start, the end) the fighter follows. The net itself is MP4's model, hooked to
 * the hidden model's hand, so it swings beside the fighter. The match ends when the minigame
 * returns to MP4's boot overlay; the placements are MP4's ranks by catches, written to the
 * players' coin_win (0 first; 3 for everyone when nobody caught anything). */
#include <string.h>

#include <melee/ft/fighter.h>
#include <melee/ft/types.h>
#include <melee/gm/gmvs.h>
#include <melee/gr/forward.h>
#include <melee/if/ifall.h>
#include <sysdolphin/baselib/controller.h>

#include "mp4/mp4.h"
#include "party.h"
#include "party_arena.h"

#define WORLD_X 600.0f

/* m441's player modes (mp4_m441_player) */
enum { MODE_FREE = 2004, MODE_SWING = 2005, MODE_SWING_END = 2006 };

int mp4_m441_player(int player, s32* mode, int* playing, s16* stick_x, s16* stick_y, int* caught);

static struct {
    int ended;
    int swung[PARTY_PLAYERS];   /* the fighter jabbed for this swing */
} bf;

static void bf_start(void)
{
    ifAll_802F3394();   /* no damage percents or stocks */
    memset(&bf, 0, sizeof bf);
    party_arena_begin(mp4_overlay_m441, WORLD_X, 0.0f, 0.0f);
}

static void bf_frame(void)
{
    party_arena_frame();
    /* over when MP4 returns to its boot overlay; at once without a disc (MELEE_PARTY_MINIGAME
     * can still name this minigame then: the lists do not) */
    if (!bf.ended && (mp4_match_over() || !mp4_available())) {
        int i;
        bf.ended = 1;
        for (i = 0; i < PARTY_PLAYERS; i++) {
            party_log("m441: P%d place %d", i + 1, mp4_player_coins(i));
        }
        gm_8016B328();
    }
}

static void bf_setup(StartMeleeData* start)
{
    memset(&bf, 0, sizeof bf);
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.x4_0 = false;    /* no pause camera: the minigame keeps its own */
    start->rules.x1_2 = true;     /* no "Ready... GO!": MP4 shows its own start */
    start->rules.x30 = 0.0f;      /* nobody is hurt */
    start->rules.on_match_start = bf_start;
    start->rules.on_frame_start = bf_frame;
}

/* Fighter_procInput: walking by the stick while the player is free, a jab as the swing begins,
 * following the player the rest of the time. */
static void bf_fighter_input(Fighter* fp)
{
    int slot = fp->player_idx;
    s32 mode;
    int playing, caught;
    s16 sx, sy;
    if (!party_arena_input_begin(fp) || bf.ended) {
        return;
    }
    if (!mp4_m441_player(slot, &mode, &playing, &sx, &sy, &caught)) {
        return;
    }
    if (playing && mode == MODE_FREE) {
        bf.swung[slot] = 0;
        party_arena_walk(fp, sx, sy);
        return;
    }
    party_arena_follow(fp, 0);
    if (playing && mode == MODE_SWING && !bf.swung[slot]) {
        bf.swung[slot] = 1;
        fp->input.held_buttons[0] |= HSD_PAD_A;
    }
}

static float bf_knockback(Fighter* fp, float kb)
{
    (void) fp;
    (void) kb;
    return 0.0f;
}

static void bf_result(s8 place[PARTY_PLAYERS])
{
    int i;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        int p = mp4_player_coins(i);
        place[i] = (s8) (p < 0 ? 0 : p > 3 ? 3 : p);
    }
}

const PartyMinigame mg_mp4_m441 = {
    "Butterfly Blitz", "butterfly-blitz", bf_setup, bf_fighter_input, bf_result, 0, NULL,
    St_Kind_Last, -1, party_arena_map, party_arena_drawn, bf_knockback, 1, NULL, NULL, 1,
};
