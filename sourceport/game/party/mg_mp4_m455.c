/* Melee Party: Rumble Fishing, Mario Party 4's m455, in a Melee match.
 *
 * The minigame is MP4's own code (mp4/m455), run by the MP4 runtime (mp4/mp4_party.c): the pier,
 * the sea and its reflections, the fish, the rods and lines, its rules, timer, CPUs and banners.
 * The four players sit on the pier with a rod each; a "!" over their heads says a fish bit, and
 * the first three to pull with A after it land a fish (the first the biggest). Pulling before the
 * bite catches nothing. MP4 shows the "!" only when the pads can't rumble, and the runtime's pads
 * never do, so it always shows. Nobody walks in this game, so the Melee fighters follow their
 * players throughout (party_arena_follow): they stand where the hidden MP4 models stand and face
 * their way, jab as their player pulls, taunt when they win and crouch when they lose. The rod is
 * MP4's model on the hidden model's hand. The match ends when the minigame returns to MP4's boot
 * overlay; the placements are MP4's, written to the players' coin_win: the order of the pulls
 * after the bite (0 first), everyone who pulled too early or not at all sharing the place after
 * the last pull, and 3 for everyone when nobody landed a fish. */
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

/* m455's player states and motions (mp4_m455_player) */
enum { ST_WAIT = 0, ST_PULL = 1 };
enum { MOTION_LOSE = 4 };

int mp4_m455_player(int player, s32* state, s32* motion, s32* phase, s32* order, s32* caught);

static struct {
    int ended;
    int pulled[PARTY_PLAYERS];   /* the fighter jabbed for this pull */
} rf;

static void rf_start(void)
{
    ifAll_802F3394();   /* no damage percents or stocks */
    memset(&rf, 0, sizeof rf);
    party_arena_begin(mp4_overlay_m455, WORLD_X, 0.0f, 0.0f);
}

static void rf_frame(void)
{
    party_arena_frame();
    /* over when MP4 returns to its boot overlay; at once without a disc (MELEE_PARTY_MINIGAME
     * can still name this minigame then: the lists do not) */
    if (!rf.ended && (mp4_match_over() || !mp4_available())) {
        int i;
        rf.ended = 1;
        for (i = 0; i < PARTY_PLAYERS; i++) {
            party_log("m455: P%d place %d", i + 1, mp4_player_coins(i));
        }
        gm_8016B328();
    }
}

static void rf_setup(StartMeleeData* start)
{
    memset(&rf, 0, sizeof rf);
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.x4_0 = false;    /* no pause camera: the minigame keeps its own */
    start->rules.x1_2 = true;     /* no "Ready... GO!": MP4 shows its own start */
    start->rules.x30 = 0.0f;      /* nobody is hurt */
    start->rules.on_match_start = rf_start;
    start->rules.on_frame_start = rf_frame;
}

/* Fighter_procInput: the fighter follows its player, jabs once as the player pulls the rod, and
 * crouches through MP4's loss pose. */
static void rf_fighter_input(Fighter* fp)
{
    int slot = fp->player_idx;
    s32 state, motion, phase, order, caught;
    if (!party_arena_input_begin(fp) || rf.ended) {
        return;
    }
    if (!mp4_m455_player(slot, &state, &motion, &phase, &order, &caught)) {
        return;
    }
    party_arena_follow(fp, 0);
    if (state == ST_PULL) {
        if (!rf.pulled[slot]) {
            rf.pulled[slot] = 1;
            fp->input.held_buttons[0] |= HSD_PAD_A;
        }
    } else if (state == ST_WAIT) {
        rf.pulled[slot] = 0;
    }
    if (motion == MOTION_LOSE) {
        fp->input.lstick[0].y = -1.0f;
    }
}

static float rf_knockback(Fighter* fp, float kb)
{
    (void) fp;
    (void) kb;
    return 0.0f;
}

/* m455 writes each player's place to coin_win: 0 for the first pull after the bite, then 1 and
 * 2 (the three fish) and 3; the early and the idle share the place after the last pull; 3 for
 * everyone when nobody landed a fish. */
static void rf_result(s8 place[PARTY_PLAYERS])
{
    int i;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        int p = mp4_player_coins(i);
        place[i] = (s8) (p < 0 ? 0 : p > 3 ? 3 : p);
    }
}

const PartyMinigame mg_mp4_m455 = {
    "Rumble Fishing", "rumble-fishing", rf_setup, rf_fighter_input, rf_result, 0, NULL,
    St_Kind_Last, -1, party_arena_map, party_arena_drawn, rf_knockback, 1, NULL, NULL, 1,
};
