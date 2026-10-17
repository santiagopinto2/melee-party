/* Melee Party: Toad's Quick Draw, Mario Party 4's m409, in a Melee match.
 *
 * The minigame is MP4's own code (mp4/m409), run by the MP4 runtime (mp4/mp4_party.c): the train
 * that brings Toad in, the balloons with pictures on them, Toad's sign, the cursors, the shots,
 * its rules, CPUs, hit counters and banners. Each round Toad holds up one picture and the players
 * aim their cursors with the stick and shoot with A; whoever pops the balloon with that picture
 * scores, and three wins. The four players are MP4's player objects, each driving a hidden MP4
 * character model that stands in its place and turns to aim; the Melee fighters stand where
 * those models stand, through party_arena.c. Nobody walks in this game, so the fighters only
 * follow their players (party_arena_follow), with a jab for each shot and a taunt for the win.
 * The popgun is MP4's model, hooked to the hidden model's hand, so it stays beside the fighter.
 * The match ends when the minigame returns to MP4's boot overlay. MP4 gives the winner 10 coins
 * and the others nothing; the winner is first and the others are placed by their hits (MP4's own
 * counters), ties sharing, a player with none last. After twelve rounds without three hits MP4
 * names no winner, and the hits alone place everyone. */
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

/* m409's player states (mp4_m409_player) */
enum { ST_FIRE = 3 };

int mp4_m409_player(int player, s16* state, int* hits);

static struct {
    int ended;
    s16 state[PARTY_PLAYERS];   /* the state seen last frame */
    int hits[PARTY_PLAYERS];    /* MP4's hit counters, kept for the placements */
} qd;

static void qd_start(void)
{
    int i;
    ifAll_802F3394();   /* no damage percents or stocks */
    memset(&qd, 0, sizeof qd);
    for (i = 0; i < PARTY_PLAYERS; i++) {
        qd.state[i] = -1;
    }
    party_arena_begin(mp4_overlay_m409, WORLD_X, 0.0f, 0.0f);
}

static void qd_frame(void)
{
    party_arena_frame();
    /* over when MP4 returns to its boot overlay; at once without a disc (MELEE_PARTY_MINIGAME
     * can still name this minigame then: the lists do not) */
    if (!qd.ended && (mp4_match_over() || !mp4_available())) {
        int i;
        qd.ended = 1;
        for (i = 0; i < PARTY_PLAYERS; i++) {
            party_log("m409: P%d coins %d hits %d", i + 1, mp4_player_coins(i), qd.hits[i]);
        }
        gm_8016B328();
    }
}

static void qd_setup(StartMeleeData* start)
{
    memset(&qd, 0, sizeof qd);
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.x4_0 = false;    /* no pause camera: the minigame keeps its own */
    start->rules.x1_2 = true;     /* no "Ready... GO!": MP4 shows its own start */
    start->rules.x30 = 0.0f;      /* nobody is hurt */
    start->rules.on_match_start = qd_start;
    start->rules.on_frame_start = qd_frame;
}

/* Fighter_procInput: the fighter follows its player throughout (nobody walks), and jabs as the
 * player fires. */
static void qd_fighter_input(Fighter* fp)
{
    int slot = fp->player_idx;
    s16 state;
    int hits;
    if (!party_arena_input_begin(fp) || qd.ended) {
        return;
    }
    if (!mp4_m409_player(slot, &state, &hits)) {
        return;
    }
    qd.hits[slot] = hits;
    party_arena_follow(fp, 0);
    if (state == ST_FIRE && qd.state[slot] != ST_FIRE) {
        fp->input.held_buttons[0] |= HSD_PAD_A;
    }
    qd.state[slot] = state;
}

static float qd_knockback(Fighter* fp, float kb)
{
    (void) fp;
    (void) kb;
    return 0.0f;
}

/* MP4 gives the winner 10 coins: first. The others by their hits, ties sharing, after the winner;
 * a player with no hit last. */
static void qd_result(s8 place[PARTY_PLAYERS])
{
    int i, j;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        int p = 0;
        if (mp4_player_coins(i) > 0) {
            place[i] = 0;
            continue;
        }
        if (qd.hits[i] <= 0) {
            place[i] = 3;
            continue;
        }
        for (j = 0; j < PARTY_PLAYERS; j++) {
            if (mp4_player_coins(j) > 0 || qd.hits[j] > qd.hits[i]) {
                p++;
            }
        }
        place[i] = (s8) (p > 3 ? 3 : p);
    }
}

const PartyMinigame mg_mp4_m409 = {
    "Toad's Quick Draw", "toads-quick-draw", qd_setup, qd_fighter_input, qd_result, 0, NULL,
    St_Kind_Last, -1, party_arena_map, party_arena_drawn, qd_knockback, 1, NULL, NULL, 1,
};
