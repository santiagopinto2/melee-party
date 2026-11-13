/* Melee Party: Chain Chomp Fever, Mario Party 4's m438, in a Melee match.
 *
 * The minigame is MP4's own code (mp4/m438), run by the MP4 runtime (mp4/mp4_party.c): the round
 * arena with its fire, the Chain Chomps charging across it, its rules, timer, CPUs and banners. The
 * four players are MP4's player objects, each driving a hidden MP4 character model; the Melee
 * fighters stand where those models stand, through party_arena.c. While a player walks by the
 * stick, its fighter walks by Melee's physics and the player moves at the fighter's speed; when
 * the game moves the player (dropping in at the start, flung by a Chomp or off the edge, the win
 * and the loss at the end) the fighter follows. Nobody jumps: a Chomp hits whatever it runs
 * through, as in MP4. The match ends when the minigame returns to MP4's boot overlay; the
 * placements are MP4's, written to the players' coin_win (0 for the survivors, up to 3 for the
 * first one out, 3 for everyone when nobody survived). */
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

/* m438's player states (mp4_m438_player) */
enum { ST_IDLE = 0, ST_WALK = 1, ST_RUN = 2, ST_HIT = 6 };

int mp4_m438_player(int player, s16* state, s16* stick_x, s16* stick_y, int* grounded, int* out);

static struct {
    int ended;
    int hit[PARTY_PLAYERS];   /* the fighter was sent flying for this hit */
} cc;

static void cc_start(void)
{
    ifAll_802F3394();   /* no damage percents or stocks */
    memset(&cc, 0, sizeof cc);
    party_arena_begin(mp4_overlay_m438, WORLD_X, 0.0f, 0.0f);
}

static void cc_frame(void)
{
    party_arena_frame();
    /* over when MP4 returns to its boot overlay; at once without a disc (MELEE_PARTY_MINIGAME
     * can still name this minigame then: the lists do not) */
    if (!cc.ended && (mp4_match_over() || !mp4_available())) {
        int i;
        cc.ended = 1;
        for (i = 0; i < PARTY_PLAYERS; i++) {
            party_log("m438: P%d place %d", i + 1, mp4_player_coins(i));
        }
        gm_8016B328();
    }
}

static void cc_setup(StartMeleeData* start)
{
    memset(&cc, 0, sizeof cc);
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.x4_0 = false;    /* no pause camera: the minigame keeps its own */
    start->rules.x1_2 = true;     /* no "Ready... GO!": MP4 shows its own start */
    start->rules.x30 = 0.0f;      /* nobody is hurt */
    start->rules.on_match_start = cc_start;
    start->rules.on_frame_start = cc_frame;
}

/* Fighter_procInput: walking by the stick while the game lets the player walk, following it the
 * rest of the time. */
static void cc_fighter_input(Fighter* fp)
{
    int slot = fp->player_idx;
    s16 state, sx, sy;
    int grounded, out;
    if (!party_arena_input_begin(fp) || cc.ended) {
        return;
    }
    if (!mp4_m438_player(slot, &state, &sx, &sy, &grounded, &out)) {
        return;
    }
    if (state == ST_HIT) {
        if (!cc.hit[slot]) {
            cc.hit[slot] = 1;
            party_arena_hit(fp);
        } else {
            party_arena_follow(fp, 1);
        }
        return;
    }
    cc.hit[slot] = 0;
    if (state <= ST_RUN && grounded) {
        party_arena_walk(fp, sx, sy);
    } else {
        party_arena_follow(fp, !grounded);
    }
}

static float cc_knockback(Fighter* fp, float kb)
{
    (void) fp;
    (void) kb;
    return 0.0f;
}

static void cc_result(s8 place[PARTY_PLAYERS])
{
    int i;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        int p = mp4_player_coins(i);
        place[i] = (s8) (p < 0 ? 0 : p > 3 ? 3 : p);
    }
}

const PartyMinigame mg_mp4_m438 = {
    "Chain Chomp Fever", "chomp-fever", cc_setup, cc_fighter_input, cc_result, 0, NULL,
    St_Kind_Last, -1, party_arena_map, party_arena_drawn, cc_knockback, 1, NULL, NULL, 1,
};
