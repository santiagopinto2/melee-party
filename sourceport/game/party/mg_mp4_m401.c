/* Melee Party: Manta Rings, Mario Party 4's m401, in a Melee match.
 *
 * The minigame is MP4's own code (mp4/m401), run by the MP4 runtime (mp4/mp4_party.c): the jump
 * off the boat, the swim through the deep behind the manta, the bubble rings (one each, the ones
 * the manta leaves three), the sea creatures that cost rings when they hit, its CPUs, counters
 * and banners, and the ending at the surface. The four players are MP4's player objects, each
 * driving a hidden MP4 character model; the Melee fighters are drawn where those models are,
 * through party_arena.c. Nobody walks in this game: the stick steers a swimmer the camera carries along
 * and A kicks it forward, all MP4's own, so the fighters only follow (party_arena_follow). They
 * are in the air for the jump off the boat and the dive, and standing the rest of the time: a
 * swimmer is drawn where MP4 has it, at any depth, while its fighter stays on its lane of Final
 * Destination (a fighter held in the air at a swimmer's depth would cross the stage's floor). The
 * match ends when the minigame returns to MP4's boot overlay. MP4 gives 10 coins to everyone with
 * the most rings when the course ends, and nothing to anyone when nobody has a ring; the winners
 * share first place and the others place by their rings at the finish (ties share), or all last
 * when nobody won. */
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

/* m401's player states (mp4_m401_player) */
enum { ST_JUMP = 1, ST_DIVE = 2 };

int mp4_m401_player(int player, s16* state, s16* rings, int* counting);

static struct {
    int ended;
    s16 rings[PARTY_PLAYERS];   /* as the race ended: MP4 picks its winners from these */
} mr;

static void mr_start(void)
{
    ifAll_802F3394();   /* no damage percents or stocks */
    memset(&mr, 0, sizeof mr);
    party_arena_begin(mp4_overlay_m401, WORLD_X, 0.0f, 0.0f);
}

static void mr_frame(void)
{
    int i;
    party_arena_frame();
    /* over when MP4 returns to its boot overlay; at once without a disc (MELEE_PARTY_MINIGAME
     * can still name this minigame then: the lists do not) */
    if (!mr.ended && (mp4_match_over() || !mp4_available())) {
        mr.ended = 1;
        for (i = 0; i < PARTY_PLAYERS; i++) {
            party_log("m401: P%d rings %d coins %d", i + 1, (int) mr.rings[i], mp4_player_coins(i));
        }
        gm_8016B328();
    }
    if (mr.ended) {
        return;
    }
    /* the rings, while they still count (the players are gone once the overlay returned) */
    for (i = 0; i < PARTY_PLAYERS; i++) {
        s16 state, rings;
        int counting;
        if (mp4_m401_player(i, &state, &rings, &counting) && counting) {
            mr.rings[i] = rings;
        }
    }
}

static void mr_setup(StartMeleeData* start)
{
    memset(&mr, 0, sizeof mr);
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.x4_0 = false;    /* no pause camera: the minigame keeps its own */
    start->rules.x1_2 = true;     /* no "Ready... GO!": MP4 shows its own start */
    start->rules.x30 = 0.0f;      /* nobody is hurt */
    start->rules.on_match_start = mr_start;
    start->rules.on_frame_start = mr_frame;
}

/* Fighter_procInput: the fighter follows its player, in the air while it jumps off the boat and
 * dives in. */
static void mr_fighter_input(Fighter* fp)
{
    s16 state, rings;
    int counting;
    if (!party_arena_input_begin(fp) || mr.ended) {
        return;
    }
    if (!mp4_m401_player(fp->player_idx, &state, &rings, &counting)) {
        return;
    }
    party_arena_follow(fp, state == ST_JUMP || state == ST_DIVE);
}

static float mr_knockback(Fighter* fp, float kb)
{
    (void) fp;
    (void) kb;
    return 0.0f;
}

/* MP4 gives each player with the most rings 10 coins (none when nobody has a ring): they share
 * first place, and the others follow by their rings at the finish. Nobody won: all last. */
static void mr_result(s8 place[PARTY_PLAYERS])
{
    int i, j, won = 0;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        won |= mp4_player_coins(i) > 0;
    }
    for (i = 0; i < PARTY_PLAYERS; i++) {
        int ahead = 0;
        if (!won) {
            place[i] = 3;
            continue;
        }
        if (mp4_player_coins(i) > 0) {
            place[i] = 0;
            continue;
        }
        for (j = 0; j < PARTY_PLAYERS; j++) {
            if (j != i && (mp4_player_coins(j) > 0 || mr.rings[j] > mr.rings[i])) {
                ahead++;
            }
        }
        place[i] = (s8) ahead;
    }
}

const PartyMinigame mg_mp4_m401 = {
    "Manta Rings", "manta-rings", mr_setup, mr_fighter_input, mr_result, 0, NULL,
    St_Kind_Last, -1, party_arena_map, party_arena_drawn, mr_knockback, 1, NULL, NULL, 1,
};
