/* Melee Party: Hop or Pop, Mario Party 4's m421, in a Melee match.
 *
 * The minigame is MP4's own code (mp4/m421), run by the MP4 runtime (mp4/mp4_party.c): one
 * against three in a fenced ring, the one rolling in a spiked ball and the three hopping in
 * balloons it pops by touching them; the three win if one of them is left after 45 seconds (MP4
 * tells the sides apart by GWPlayerCfg.group, which its board decides; the party picks the one
 * at random and hands the groups to the runtime, mp4_match_groups). The stick moves and A hops
 * as MP4 reads them; the ball and the balloons are the game, so the fighters follow their
 * players throughout (party_arena_follow), in the air while the player is above its floor. The
 * match ends when the minigame returns to MP4's boot overlay; MP4 gives each winner 10 coins, so
 * the winning side shares first place and the other last. */
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

int mp4_m421_player(int player, s32* role, s32* state, float* height);

static struct {
    int ended;
    int solo;   /* the one against three */
} hp;

static void hp_start(void)
{
    s8 groups[PARTY_PLAYERS];
    int i;
    ifAll_802F3394();   /* no damage percents or stocks */
    for (i = 0; i < PARTY_PLAYERS; i++) {
        groups[i] = (s8) (i == hp.solo ? 0 : 1);
    }
    mp4_match_groups(groups);
    party_arena_begin(mp4_overlay_m421, WORLD_X, 0.0f, 0.0f);
}

static void hp_frame(void)
{
    party_arena_frame();
    /* over when MP4 returns to its boot overlay; at once without a disc (MELEE_PARTY_MINIGAME
     * can still name this minigame then: the lists do not) */
    if (!hp.ended && (mp4_match_over() || !mp4_available())) {
        int i;
        hp.ended = 1;
        for (i = 0; i < PARTY_PLAYERS; i++) {
            party_log("m421: P%d coins %d%s", i + 1, mp4_player_coins(i), i == hp.solo ? " (the one)" : "");
        }
        gm_8016B328();
    }
}

static void hp_setup(StartMeleeData* start)
{
    memset(&hp, 0, sizeof hp);
    hp.solo = party_rand(PARTY_PLAYERS);
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.x4_0 = false;    /* no pause camera: the minigame keeps its own */
    start->rules.x1_2 = true;     /* no "Ready... GO!": MP4 shows its own start */
    start->rules.x30 = 0.0f;      /* nobody is hurt */
    start->rules.on_match_start = hp_start;
    start->rules.on_frame_start = hp_frame;
}

/* Fighter_procInput: the fighter follows its player, in the air while the player hops. */
static void hp_fighter_input(Fighter* fp)
{
    s32 role, state;
    float height;
    if (!party_arena_input_begin(fp) || hp.ended) {
        return;
    }
    if (!mp4_m421_player(fp->player_idx, &role, &state, &height)) {
        return;
    }
    party_arena_follow(fp, height > 1.0f);
}

static float hp_knockback(Fighter* fp, float kb)
{
    (void) fp;
    (void) kb;
    return 0.0f;
}

/* MP4 gives each winner 10 coins and the others nothing. */
static void hp_result(s8 place[PARTY_PLAYERS])
{
    int i;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        place[i] = (s8) (mp4_player_coins(i) > 0 ? 0 : 3);
    }
}

const PartyMinigame mg_mp4_m421 = {
    "Hop or Pop", "hop-or-pop", hp_setup, hp_fighter_input, hp_result, 0, NULL,
    St_Kind_Last, -1, party_arena_map, party_arena_drawn, hp_knockback, 1,
    party_arena_camera_views, party_arena_camera_view, 1,
};
