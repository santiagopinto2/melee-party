/* Melee Party: Order Up, Mario Party 4's m431, in a Melee match.
 *
 * The minigame is MP4's own code (mp4/m431), run by the MP4 runtime (mp4/mp4_party.c): the two
 * tables, each with a ring of ten dishes the players circle, the chef and his order bubble, the
 * spinning of the rings, the rounds, the timer, CPUs, counters and banners, and its one camera.
 * Two against two: MP4 tells the pairs apart by GWPlayerCfg.group, which its board decides; the
 * party pairs the players at random (as Cheep Cheep Sweep does) and hands the groups to the
 * runtime (mp4_match_groups). The four players are MP4's player objects, each driving a hidden MP4
 * character model; the Melee fighters stand where those models stand, through party_arena.c.
 * Nobody walks by the stick in this game: MP4 turns the player a few degrees a frame round its
 * table, so the fighters only follow, in Melee's walk and run for MP4's, a taunt for MP4's cheer
 * and win, a crouch for the shrug after a wrong dish and for the loss, and a jab as a player
 * grabs a dish. The grab itself, the dish in the hand and the ring are MP4's. The match ends when
 * the minigame returns to MP4's boot overlay; MP4 gives each member of the winning pair 10 coins,
 * so that pair shares first place and the other pair is last. When it ends in a draw MP4 gives
 * nobody coins and everyone is last. */
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

/* m431's player states (mp4_m431_player) */
enum { ST_IDLE = 0, ST_WALK = 1, ST_RUN = 2, ST_GRAB = 3, ST_CHEER = 4, ST_SHRUG = 5, ST_WIN = 6, ST_LOSE = 7 };

int mp4_m431_player(int player, s32* state, s32* team);

static struct {
    int ended;
    s8 groups[PARTY_PLAYERS];
    int grabbed[PARTY_PLAYERS];   /* the fighter jabbed for this grab */
} ou;

static void ou_start(void)
{
    ifAll_802F3394();   /* no damage percents or stocks */
    mp4_match_groups(ou.groups);
    party_arena_begin(mp4_overlay_m431, WORLD_X, 0.0f, 0.0f);
}

static void ou_frame(void)
{
    party_arena_frame();
    /* over when MP4 returns to its boot overlay; at once without a disc (MELEE_PARTY_MINIGAME
     * can still name this minigame then: the lists do not) */
    if (!ou.ended && (mp4_match_over() || !mp4_available())) {
        int i;
        ou.ended = 1;
        for (i = 0; i < PARTY_PLAYERS; i++) {
            party_log("m431: P%d pair %d coins %d", i + 1, ou.groups[i], mp4_player_coins(i));
        }
        gm_8016B328();
    }
}

static void ou_setup(StartMeleeData* start)
{
    int order[PARTY_PLAYERS] = { 0, 1, 2, 3 };
    int i;
    memset(&ou, 0, sizeof ou);
    /* random pairs, as Cheep Cheep Sweep pairs its teams: any two and two, in any slots */
    for (i = PARTY_PLAYERS - 1; i > 0; i--) {
        int j = party_rand(i + 1);
        int t = order[i];
        order[i] = order[j];
        order[j] = t;
    }
    for (i = 0; i < PARTY_PLAYERS; i++) {
        ou.groups[order[i]] = (s8) (i >> 1);
    }
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.x4_0 = false;    /* no pause camera: the minigame keeps its own */
    start->rules.x1_2 = true;     /* no "Ready... GO!": MP4 shows its own start */
    start->rules.x30 = 0.0f;      /* nobody is hurt */
    start->rules.on_match_start = ou_start;
    start->rules.on_frame_start = ou_frame;
}

/* Fighter_procInput: the fighter follows its player (MP4 moves the player round the table), with
 * a jab as the player starts to grab and a crouch while it shrugs or has lost. */
static void ou_fighter_input(Fighter* fp)
{
    int slot = fp->player_idx;
    s32 state, team;
    if (!party_arena_input_begin(fp) || ou.ended) {
        return;
    }
    if (!mp4_m431_player(slot, &state, &team)) {
        return;
    }
    party_arena_follow(fp, 0);
    if (state == ST_GRAB) {
        if (!ou.grabbed[slot]) {
            ou.grabbed[slot] = 1;
            fp->input.held_buttons[0] |= HSD_PAD_A;
        }
    } else {
        ou.grabbed[slot] = 0;
    }
    if (state == ST_SHRUG || state == ST_LOSE) {
        fp->input.lstick[0].y = -1.0f;
    }
}

static float ou_knockback(Fighter* fp, float kb)
{
    (void) fp;
    (void) kb;
    return 0.0f;
}

/* MP4 gives each member of the winning pair 10 coins and the others nothing; nobody in a draw. */
static void ou_result(s8 place[PARTY_PLAYERS])
{
    int i;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        place[i] = (s8) (mp4_player_coins(i) > 0 ? 0 : 3);
    }
}

const PartyMinigame mg_mp4_m431 = {
    "Order Up", "order-up", ou_setup, ou_fighter_input, ou_result, 0, NULL,
    St_Kind_Last, -1, party_arena_map, party_arena_drawn, ou_knockback, 1, NULL, NULL, 1,
};
