/* Melee Party: Paths of Peril, Mario Party 4's m439, in a Melee match.
 *
 * The minigame is MP4's own code (mp4/m439), run by the MP4 runtime (mp4/mp4_party.c): the four
 * narrow paths over the drop, the pits and the breaking planks, its rules, CPUs and banners, on
 * a four-way split screen: once the game starts it draws everything once a quarter, one Hu3D
 * camera a player, and the runtime's views follow (party_arena_camera_views). Two against two:
 * MP4 tells the pairs apart by GWPlayerCfg.group, which its board decides; the party pairs the
 * players at random (as Dungeon Duos does) and hands the groups to the runtime (mp4_match_groups).
 * The four players are MP4's player objects, each driving a hidden MP4 character model; the
 * Melee fighters stand where those models stand, through party_arena.c. While a player walks by
 * the stick, its fighter walks by Melee's physics and the player moves at that speed along MP4's
 * heading (one line in m439's player code); when the game moves the player (a fall, the way back
 * onto the path, the end) the fighter follows. The match ends when the minigame returns to MP4's
 * boot overlay; the pair of the first player to reach the end shares first place, the other
 * last; nobody loses if nobody got there. */
#include <math.h>
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

/* m439's player states (mp4_m439_player) */
enum { ST_PATH = 0, ST_BACK = 1, ST_FALLEN = 2 };

int mp4_m439_player(int player, s32* state, s16* stick_x, s16* stick_y, int* finished, s32* pair,
                    int* order, int* playing);

static struct {
    int ended;
    s8 groups[PARTY_PLAYERS];
    int order[PARTY_PLAYERS];   /* how many reached the end before each (-1: not there) */
} pp;

static void pp_start(void)
{
    ifAll_802F3394();   /* no damage percents or stocks */
    mp4_match_groups(pp.groups);
    party_arena_begin(mp4_overlay_m439, WORLD_X, 0.0f, 0.0f);
}

static void pp_frame(void)
{
    party_arena_frame();
    /* over when MP4 returns to its boot overlay; at once without a disc (MELEE_PARTY_MINIGAME
     * can still name this minigame then: the lists do not) */
    if (!pp.ended && (mp4_match_over() || !mp4_available())) {
        int i;
        pp.ended = 1;
        for (i = 0; i < PARTY_PLAYERS; i++) {
            party_log("m439: P%d pair %d order %d", i + 1, pp.groups[i], pp.order[i]);
        }
        gm_8016B328();
    }
}

static void pp_setup(StartMeleeData* start)
{
    int order[PARTY_PLAYERS] = { 0, 1, 2, 3 };
    int i;
    memset(&pp, 0, sizeof pp);
    /* random pairs, as Dungeon Duos pairs its teams */
    for (i = PARTY_PLAYERS - 1; i > 0; i--) {
        int j = party_rand(i + 1);
        int t = order[i];
        order[i] = order[j];
        order[j] = t;
    }
    for (i = 0; i < PARTY_PLAYERS; i++) {
        pp.groups[order[i]] = (s8) (i >> 1);
    }
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.x4_0 = false;    /* no pause camera: the minigame keeps its own */
    start->rules.x1_2 = true;     /* no "Ready... GO!": MP4 shows its own start */
    start->rules.x30 = 0.0f;      /* nobody is hurt */
    start->rules.on_match_start = pp_start;
    start->rules.on_frame_start = pp_frame;
}

/* Fighter_procInput: walking by the stick while the player is on its path and the game is in
 * play, following it the rest of the time (in the air while fallen). */
static void pp_fighter_input(Fighter* fp)
{
    int slot = fp->player_idx;
    s32 state, pair;
    s16 sx, sy;
    int finished, order, playing;
    if (!party_arena_input_begin(fp) || pp.ended) {
        return;
    }
    if (!mp4_m439_player(slot, &state, &sx, &sy, &finished, &pair, &order, &playing)) {
        return;
    }
    pp.order[slot] = order;
    if (playing && state == ST_PATH && !finished) {
        party_arena_walk(fp, sx, sy);
        return;
    }
    party_arena_follow(fp, state == ST_FALLEN);
}

static float pp_knockback(Fighter* fp, float kb)
{
    (void) fp;
    (void) kb;
    return 0.0f;
}

/* MP4 gives each winner 10 coins and the others nothing; nobody when it is a draw. */
/* The pair of the first player to reach the end wins (MP4 pays the finishers); nobody loses if
 * nobody got there. */
static void pp_result(s8 place[PARTY_PLAYERS])
{
    int i, first = -1;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        if (pp.order[i] == 0) {
            first = i;
        }
    }
    for (i = 0; i < PARTY_PLAYERS; i++) {
        place[i] = (s8) (first < 0 || pp.groups[i] == pp.groups[first] ? 0 : 3);
    }
}

const PartyMinigame mg_mp4_m439 = {
    "Paths of Peril", "paths-of-peril", pp_setup, pp_fighter_input, pp_result, 0, NULL,
    St_Kind_Last, -1, party_arena_map, party_arena_drawn, pp_knockback, 1,
    party_arena_camera_views, party_arena_camera_view, 1,
};
