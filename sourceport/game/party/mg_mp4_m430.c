/* Melee Party: Pair-a-sailing, Mario Party 4's m430, in a Melee match.
 *
 * The minigame is MP4's own code (mp4/m430), run by the MP4 runtime (mp4/mp4_party.c): the sea,
 * the two speedboats, the coins, the rocks, its rules, CPUs, counters and banners, and its two
 * Hu3D cameras, a half of the screen each while the pairs sail (the start and the beach are one
 * camera on the whole screen). Two against two: one of a pair drives the boat (the stick steers,
 * A and B shift its three speeds) and the other hangs from a parasail behind it (the stick
 * steers) and catches the coins that float at two heights; the faster the boat, the higher the
 * parasail. MP4 seats the players by GWPlayerCfg.group: its instruction screen turns the board's
 * two pairs into slots 0 to 3 (slot / 2 the pair, slot % 2 the seat, both at random), and m430
 * reads those; the party pairs and seats the players at random and hands the slots to the runtime
 * (mp4_match_groups). The four players are MP4's player objects, each driving a hidden MP4
 * character model; the Melee fighters stand where those models stand, through party_arena.c.
 * Nobody walks in this game, so the fighters only follow: the driver standing in its boat, the
 * one on the parasail in the air at its height, everyone on the beach at the end. The match ends
 * when the minigame returns to MP4's boot overlay, after 45 seconds of sailing; MP4 gives each
 * player its pair's catch (coin_collect), so the pair with more coins shares first place and the
 * other last; equal catches share first, and nothing caught at all puts everyone last. */
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

/* m430's seats and phases (mp4_m430_player) */
enum { SEAT_DRIVER = 0, SEAT_SAIL = 1 };
enum { PHASE_BEACH = 4 };
/* the parasail flies at 200 to 550 (MP4's units); the boat and the beach are below 40 */
#define SAIL_HEIGHT 100.0f

int mp4_m430_player(int player, s32* seat, s32* pair, s32* state, float* height, s32* phase, s32* coins);
int mp4_m430_coins(int player);

static struct {
    int ended;
    s8 slots[PARTY_PLAYERS];   /* MP4's GWPlayerCfg.group for m430: slot / 2 the pair, slot % 2 the seat */
} ps;

static void ps_start(void)
{
    ifAll_802F3394();   /* no damage percents or stocks */
    mp4_match_groups(ps.slots);
    party_arena_begin(mp4_overlay_m430, WORLD_X, 0.0f, 0.0f);
}

static void ps_frame(void)
{
    party_arena_frame();
    /* over when MP4 returns to its boot overlay; at once without a disc (MELEE_PARTY_MINIGAME
     * can still name this minigame then: the lists do not) */
    if (!ps.ended && (mp4_match_over() || !mp4_available())) {
        int i;
        ps.ended = 1;
        for (i = 0; i < PARTY_PLAYERS; i++) {
            party_log("m430: P%d pair %d seat %d coins %d", i + 1, ps.slots[i] >> 1, ps.slots[i] & 1,
                      mp4_m430_coins(i));
        }
        gm_8016B328();
    }
}

static void ps_setup(StartMeleeData* start)
{
    int order[PARTY_PLAYERS] = { 0, 1, 2, 3 };
    int i;
    memset(&ps, 0, sizeof ps);
    /* random pairs and seats, as MP4's instruction screen deals them out of the board's pairs */
    for (i = PARTY_PLAYERS - 1; i > 0; i--) {
        int j = party_rand(i + 1);
        int t = order[i];
        order[i] = order[j];
        order[j] = t;
    }
    for (i = 0; i < PARTY_PLAYERS; i++) {
        ps.slots[order[i]] = (s8) i;
    }
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.x4_0 = false;    /* no pause camera: the minigame keeps its own */
    start->rules.x1_2 = true;     /* no "Ready... GO!": MP4 shows its own start */
    start->rules.x30 = 0.0f;      /* nobody is hurt */
    start->rules.on_match_start = ps_start;
    start->rules.on_frame_start = ps_frame;
}

/* Fighter_procInput: the fighter follows its player throughout, in the air while it hangs from
 * the parasail. */
static void ps_fighter_input(Fighter* fp)
{
    s32 seat, pair, state, phase, coins;
    float height;
    if (!party_arena_input_begin(fp) || ps.ended) {
        return;
    }
    if (!mp4_m430_player(fp->player_idx, &seat, &pair, &state, &height, &phase, &coins)) {
        return;
    }
    party_arena_follow(fp, seat == SEAT_SAIL && phase != PHASE_BEACH && height > SAIL_HEIGHT);
}

static float ps_knockback(Fighter* fp, float kb)
{
    (void) fp;
    (void) kb;
    return 0.0f;
}

/* MP4 gives both of a pair the pair's catch (coin_collect, on the beach) and no winner's coins:
 * the pair that caught more shares first place and the other last; equal catches share first,
 * and nobody catching anything puts everyone last. */
static void ps_result(s8 place[PARTY_PLAYERS])
{
    int pair_coins[2] = { 0, 0 };
    int i, best;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        int c = mp4_m430_coins(i);
        if (c > pair_coins[ps.slots[i] >> 1]) {
            pair_coins[ps.slots[i] >> 1] = c;
        }
    }
    best = pair_coins[0] > pair_coins[1] ? pair_coins[0] : pair_coins[1];
    for (i = 0; i < PARTY_PLAYERS; i++) {
        place[i] = (s8) (best > 0 && pair_coins[ps.slots[i] >> 1] == best ? 0 : 3);
    }
}

const PartyMinigame mg_mp4_m430 = {
    "Pair-a-sailing", "pair-a-sailing", ps_setup, ps_fighter_input, ps_result, 0, NULL,
    St_Kind_Last, -1, party_arena_map, party_arena_drawn, ps_knockback, 1,
    party_arena_camera_views, party_arena_camera_view, 1,
};
