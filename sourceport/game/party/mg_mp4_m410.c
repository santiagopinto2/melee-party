/* Melee Party: Three Throw, Mario Party 4's m410, in a Melee match.
 *
 * The minigame is MP4's own code (mp4/m410), run by the MP4 runtime (mp4/mp4_party.c): the wall of
 * three target rows sliding past, the balls tossed in from behind, their flight and bounces, the
 * splats, its rules, timer, CPUs, score counters and banners. The four players are MP4's player
 * objects, each driving a hidden MP4 character model; the Melee fighters stand where those models
 * stand, through party_arena.c. Nobody walks in this game: each player stays on its spot, catches
 * the ball tossed to it, and throws it at the targets, from the ground (B) or from a jump (A, then
 * B in the air). MP4 does all of that, so the fighters only follow: in the air while the player
 * jumps, a jab or an aerial as it throws, a taunt when it wins. The ball is MP4's model, held in
 * the hidden model's hand beside the fighter. The match ends when the minigame returns to MP4's
 * boot overlay; MP4 gives 10 coins to each player with the top score (nobody, when nobody hit a
 * target), so they share first place, and the others are placed by their scores, ties sharing.
 *
 * The Melee side: Final Destination, with each fighter colliding on a lane of its own and the MP4
 * world drawn 600 units to the side. */
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

/* m410's player motions (mp4_m410_player): indices in its motion table */
enum { MOT_GROUND_THROW = 3, MOT_AIR_THROW = 5, MOT_WON = 8 };

int mp4_m410_player(int player, s32* motion, int* airborne, s32* score);

static struct {
    int ended;
    int scored;                    /* the scores below were read at least once */
    s32 score[PARTY_PLAYERS];      /* the last score MP4 showed, kept for the result */
    s32 motion[PARTY_PLAYERS];     /* the motion seen last frame */
} tt;

static void tt_start(void)
{
    int i;
    ifAll_802F3394();   /* no damage percents or stocks */
    memset(&tt, 0, sizeof tt);
    for (i = 0; i < PARTY_PLAYERS; i++) {
        tt.motion[i] = -1;
    }
    party_arena_begin(mp4_overlay_m410, WORLD_X, 0.0f, 0.0f);
}

static void tt_frame(void)
{
    party_arena_frame();
    /* over when MP4 returns to its boot overlay; at once without a disc (MELEE_PARTY_MINIGAME
     * can still name this minigame then: the lists do not) */
    if (!tt.ended && (mp4_match_over() || !mp4_available())) {
        int i;
        tt.ended = 1;
        for (i = 0; i < PARTY_PLAYERS; i++) {
            party_log("m410: P%d score %d coins %d", i + 1, (int) tt.score[i], mp4_player_coins(i));
        }
        gm_8016B328();
    }
}

static void tt_setup(StartMeleeData* start)
{
    memset(&tt, 0, sizeof tt);
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.x4_0 = false;    /* no pause camera: the minigame keeps its own */
    start->rules.x1_2 = true;     /* no "Ready... GO!": MP4 shows its own start */
    start->rules.x30 = 0.0f;      /* nobody is hurt */
    start->rules.on_match_start = tt_start;
    start->rules.on_frame_start = tt_frame;
}

/* Fighter_procInput: always following the player, which MP4 keeps on its spot. The arena's own
 * reading of the MP4 motion is dropped: m410's motions are its own files, whose numbers the arena
 * would take for a walk or a taunt. The fighter jabs (on the ground) or attacks in the air as a
 * throw begins, and taunts as its win begins. */
static void tt_fighter_input(Fighter* fp)
{
    int slot = fp->player_idx;
    s32 motion, score;
    int airborne, began;
    if (!party_arena_input_begin(fp) || tt.ended) {
        return;
    }
    if (!mp4_m410_player(slot, &motion, &airborne, &score)) {
        return;
    }
    tt.score[slot] = score;
    tt.scored = 1;
    began = motion != tt.motion[slot];
    tt.motion[slot] = motion;
    party_arena_follow(fp, airborne);
    fp->input.lstick[0].x = 0.0f;
    fp->input.held_buttons[0] = 0;
    if (!began) {
        return;
    }
    if (motion == MOT_GROUND_THROW || motion == MOT_AIR_THROW) {
        fp->input.held_buttons[0] = HSD_PAD_A;
    } else if (motion == MOT_WON) {
        fp->input.held_buttons[0] = HSD_PAD_DPADUP;
    }
}

static float tt_knockback(Fighter* fp, float kb)
{
    (void) fp;
    (void) kb;
    return 0.0f;
}

/* MP4 gives each player with the top score 10 coins: first place, shared. The others are placed
 * after them by their scores (ties share). Nobody hit a target: nobody won, everyone last. */
static void tt_result(s8 place[PARTY_PLAYERS])
{
    int i, j, winners = 0;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        if (mp4_player_coins(i) > 0) {
            winners++;
        }
    }
    for (i = 0; i < PARTY_PLAYERS; i++) {
        int p = 1;
        if (winners == 0) {
            place[i] = 3;
            continue;
        }
        if (mp4_player_coins(i) > 0) {
            place[i] = 0;
            continue;
        }
        if (!tt.scored) {
            place[i] = 3;
            continue;
        }
        for (j = 0; j < PARTY_PLAYERS; j++) {
            if (mp4_player_coins(j) <= 0 && tt.score[j] > tt.score[i]) {
                p++;
            }
        }
        place[i] = (s8) (p > 3 ? 3 : p);
    }
}

const PartyMinigame mg_mp4_m410 = {
    "Three Throw", "three-throw", tt_setup, tt_fighter_input, tt_result, 0, NULL,
    St_Kind_Last, -1, party_arena_map, party_arena_drawn, tt_knockback, 1, NULL, NULL, 1,
};
