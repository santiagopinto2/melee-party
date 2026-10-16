/* Melee Party: Team Treasure Trek, Mario Party 4's m429, in a Melee match.
 *
 * The minigame is MP4's own code (mp4/m429), run by the MP4 runtime (mp4/mp4_party.c): a
 * four-way split screen, one Hu3D camera a view (party_arena_camera_views), the maze, its
 * walls (MapWall), the keys and chests, the map on X or Y, its rules, CPUs and banners. Two
 * against two: MP4 tells the pairs apart by GWPlayerCfg.group, which its board decides; the party
 * pairs the players at random (as Dungeon Duos does) and hands the groups to the runtime
 * (mp4_match_groups). The four players are MP4's player objects, each driving a hidden MP4
 * character model; the Melee fighters stand where those models stand, through party_arena.c.
 * While a player walks by the stick, its fighter walks by Melee's physics and the player moves
 * at the fighter's speed; with the map open, and at the start and the end, the fighter follows.
 * The match ends when the minigame returns to MP4's boot overlay; MP4 gives each winner 10
 * coins, so the winning pair shares first place and the other last. */
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

/* m429's player modes (mp4_m429_player) */
enum { MODE_PLAY = 2001 };

int mp4_m429_player(int player, s32* mode, int* held, s16* stick_x, s16* stick_y, s32* team, s32* phase);

static struct {
    int ended;
    s8 groups[PARTY_PLAYERS];
} tt;

static void tt_start(void)
{
    ifAll_802F3394();   /* no damage percents or stocks */
    mp4_match_groups(tt.groups);
    party_arena_begin(mp4_overlay_m429, WORLD_X, 0.0f, 0.0f);
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
            party_log("m429: P%d pair %d coins %d", i + 1, tt.groups[i], mp4_player_coins(i));
        }
        gm_8016B328();
    }
}

static void tt_setup(StartMeleeData* start)
{
    int order[PARTY_PLAYERS] = { 0, 1, 2, 3 };
    int i;
    memset(&tt, 0, sizeof tt);
    /* random pairs, as Dungeon Duos pairs its teams */
    for (i = PARTY_PLAYERS - 1; i > 0; i--) {
        int j = party_rand(i + 1);
        int t = order[i];
        order[i] = order[j];
        order[j] = t;
    }
    for (i = 0; i < PARTY_PLAYERS; i++) {
        tt.groups[order[i]] = (s8) (i >> 1);
    }
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.x4_0 = false;    /* no pause camera: the minigame keeps its own */
    start->rules.x1_2 = true;     /* no "Ready... GO!": MP4 shows its own start */
    start->rules.x30 = 0.0f;      /* nobody is hurt */
    start->rules.on_match_start = tt_start;
    start->rules.on_frame_start = tt_frame;
}

/* Fighter_procInput: walking by the stick while the player is in play and its map is closed,
 * following it the rest of the time. */
static void tt_fighter_input(Fighter* fp)
{
    s32 mode, team, phase;
    int held;
    s16 sx, sy;
    if (!party_arena_input_begin(fp) || tt.ended) {
        return;
    }
    if (!mp4_m429_player(fp->player_idx, &mode, &held, &sx, &sy, &team, &phase)) {
        return;
    }
    if (mode == MODE_PLAY && !held && phase < 1007) {
        party_arena_walk(fp, sx, sy);
    } else {
        party_arena_follow(fp, 0);
    }
}

static float tt_knockback(Fighter* fp, float kb)
{
    (void) fp;
    (void) kb;
    return 0.0f;
}

/* MP4 gives each winner 10 coins and the others nothing; nobody when it is a draw. */
static void tt_result(s8 place[PARTY_PLAYERS])
{
    int i, any = 0;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        any |= mp4_player_coins(i) > 0;
    }
    for (i = 0; i < PARTY_PLAYERS; i++) {
        place[i] = (s8) (!any || mp4_player_coins(i) > 0 ? 0 : 3);
    }
}

const PartyMinigame mg_mp4_m429 = {
    "Team Treasure Trek", "team-treasure-trek", tt_setup, tt_fighter_input, tt_result, 0, NULL,
    St_Kind_Last, -1, party_arena_map, party_arena_drawn, tt_knockback, 1,
    party_arena_camera_views, party_arena_camera_view, 1,
};
