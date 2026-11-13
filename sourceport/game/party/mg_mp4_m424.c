/* Melee Party: Blame it on the Crane, Mario Party 4's m424, in a Melee match.
 *
 * The minigame is MP4's own code (mp4/m424), run by the MP4 runtime (mp4/mp4_party.c): one
 * against three. The one works a crane on its tower with A alone (hold to swing the arm, let go
 * to drop the claw); the three each roll inside a clear ball on a spinning turntable, among six
 * balls with other creatures, and steer clear of the claw. A ball the claw grabs with a runner
 * inside goes down the chute. The one wins by dropping all three within 60 seconds, otherwise
 * the three win. MP4 tells the sides apart by GWPlayerCfg.group, which its board decides; the
 * party picks the one at random and hands the groups to the runtime (mp4_match_groups). On
 * party_arena.c: while a runner rolls its ball by the stick, its fighter walks by Melee's physics
 * and the ball rolls at that speed; in the claw, down the chute and at the end, the fighter
 * follows. The crane's fighter stands on the tower and follows it. The match ends when the
 * minigame returns to MP4's boot overlay; MP4 gives each winner 10 coins, so the winning side
 * shares first place and the other last. */
#include <string.h>

#include <melee/ft/fighter.h>
#include <melee/ft/types.h>
#include <melee/gm/gmvs.h>
#include <melee/gr/forward.h>
#include <melee/if/ifall.h>

#include "mp4/mp4.h"
#include "party.h"
#include "party_arena.h"

#define WORLD_X 600.0f

/* m424's phase and ball states (mp4_m424_runner) */
enum { PHASE_PLAY = 4 };
enum { BALL_ROLLING = 1, BALL_AIR = 2, BALL_CLAW = 3, BALL_CHUTE = 4, BALL_BLINK = 5, BALL_GONE = 6 };

int mp4_m424_runner(int player, s32* ball, s32* out, s16* stick_x, s16* stick_y, float* y, s32* phase);
int mp4_m424_crane(int player, s32* state, s32* airborne);

static struct {
    int ended;
    int solo;   /* the one at the crane */
} bc;

static void bc_start(void)
{
    s8 groups[PARTY_PLAYERS];
    int i;
    ifAll_802F3394();   /* no damage percents or stocks */
    for (i = 0; i < PARTY_PLAYERS; i++) {
        groups[i] = (s8) (i == bc.solo ? 0 : 1);
    }
    mp4_match_groups(groups);
    party_arena_begin(mp4_overlay_m424, WORLD_X, 0.0f, 0.0f);
}

static void bc_frame(void)
{
    party_arena_frame();
    /* over when MP4 returns to its boot overlay; at once without a disc (MELEE_PARTY_MINIGAME
     * can still name this minigame then: the lists do not) */
    if (!bc.ended && (mp4_match_over() || !mp4_available())) {
        int i;
        bc.ended = 1;
        for (i = 0; i < PARTY_PLAYERS; i++) {
            party_log("m424: P%d coins %d%s", i + 1, mp4_player_coins(i), i == bc.solo ? " (the one)" : "");
        }
        gm_8016B328();
    }
}

static void bc_setup(StartMeleeData* start)
{
    memset(&bc, 0, sizeof bc);
    bc.solo = party_rand(PARTY_PLAYERS);
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.x4_0 = false;    /* no pause camera: the minigame keeps its own */
    start->rules.x1_2 = true;     /* no "Ready... GO!": MP4 shows its own start */
    start->rules.x30 = 0.0f;      /* nobody is hurt */
    start->rules.on_match_start = bc_start;
    start->rules.on_frame_start = bc_frame;
}

/* Fighter_procInput: a runner walks by the stick while its ball rolls on the turntable in play,
 * and follows it the rest of the time (in the air while the ball is up, in the claw or falling
 * down the chute); the crane's player follows the crane, in the air on its hops after a win. */
static void bc_fighter_input(Fighter* fp)
{
    int slot = fp->player_idx;
    s32 ball, out, phase, state, airborne;
    s16 sx, sy;
    float y;
    if (!party_arena_input_begin(fp) || bc.ended) {
        return;
    }
    if (mp4_m424_crane(slot, &state, &airborne)) {
        party_arena_follow(fp, airborne);
        return;
    }
    if (!mp4_m424_runner(slot, &ball, &out, &sx, &sy, &y, &phase)) {
        return;
    }
    if (phase == PHASE_PLAY && !out && ball == BALL_ROLLING) {
        party_arena_walk(fp, sx, sy);
        return;
    }
    party_arena_follow(fp, ball == BALL_AIR || ball == BALL_CLAW || ball == BALL_CHUTE ||
                               (ball == BALL_GONE && y > 50.0f) || (ball == BALL_BLINK && y > 200.0f));
}

static float bc_knockback(Fighter* fp, float kb)
{
    (void) fp;
    (void) kb;
    return 0.0f;
}

/* MP4 gives each winner 10 coins and the others nothing. */
static void bc_result(s8 place[PARTY_PLAYERS])
{
    int i;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        place[i] = (s8) (mp4_player_coins(i) > 0 ? 0 : 3);
    }
}

const PartyMinigame mg_mp4_m424 = {
    "Blame it on the Crane", "blame-it-on-the-crane", bc_setup, bc_fighter_input, bc_result, 0, NULL,
    St_Kind_Last, -1, party_arena_map, party_arena_drawn, bc_knockback, 1, NULL, NULL, 1,
};
