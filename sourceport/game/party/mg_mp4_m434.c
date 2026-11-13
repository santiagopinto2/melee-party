/* Melee Party: Cheep Cheep Sweep, Mario Party 4's m434, in a Melee match.
 *
 * The minigame is MP4's own code (mp4/m434), run by the MP4 runtime (mp4/mp4_party.c): the
 * pond, the Cheep Cheeps, the nets, its rules, CPUs, counters and banners, and its four Hu3D
 * cameras, one view and three render passes of MP4's own (the reflection cameras draw into a
 * corner of the screen for a copy; mp4_camera_set gives each its own eye and viewport). Two against two: MP4 tells the
 * pairs apart by GWPlayerCfg.group, which its board decides; the party pairs the players at
 * random (as Dungeon Duos does) and hands the groups to the runtime (mp4_match_groups). The four
 * players are MP4's player objects, each driving a hidden MP4 character model; the Melee fighters
 * stand where those models stand, through party_arena.c. While a player wades by the stick, its
 * fighter walks by Melee's physics and the player moves at the fighter's speed; a sweep of the
 * net (A) is MP4's, on the hidden model, and the fighter jabs with it; the rest of the time the
 * fighter follows. The match ends when the minigame returns to MP4's boot overlay; MP4 gives each
 * winner 10 coins, so the winning pair shares first place and the other last. */
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

/* m434's player states (mp4_m434_player) */
enum { ST_FREE = 0, ST_SWEEP = 1 };

int mp4_m434_player(int player, s32* state, float* stick, float* heading, s32* team, int* playing);

static struct {
    int ended;
    s8 groups[PARTY_PLAYERS];
    int swept[PARTY_PLAYERS];   /* the fighter jabbed for this sweep */
} cs;

static void cs_start(void)
{
    ifAll_802F3394();   /* no damage percents or stocks */
    mp4_match_groups(cs.groups);
    party_arena_begin(mp4_overlay_m434, WORLD_X, 0.0f, 0.0f);
}

static void cs_frame(void)
{
    party_arena_frame();
    /* over when MP4 returns to its boot overlay; at once without a disc (MELEE_PARTY_MINIGAME
     * can still name this minigame then: the lists do not) */
    if (!cs.ended && (mp4_match_over() || !mp4_available())) {
        int i;
        cs.ended = 1;
        for (i = 0; i < PARTY_PLAYERS; i++) {
            party_log("m434: P%d pair %d coins %d", i + 1, cs.groups[i], mp4_player_coins(i));
        }
        gm_8016B328();
    }
}

static void cs_setup(StartMeleeData* start)
{
    int order[PARTY_PLAYERS] = { 0, 1, 2, 3 };
    int i;
    memset(&cs, 0, sizeof cs);
    /* random pairs, as Dungeon Duos pairs its teams */
    for (i = PARTY_PLAYERS - 1; i > 0; i--) {
        int j = party_rand(i + 1);
        int t = order[i];
        order[i] = order[j];
        order[j] = t;
    }
    for (i = 0; i < PARTY_PLAYERS; i++) {
        cs.groups[order[i]] = (s8) (i >> 1);
    }
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.x4_0 = false;    /* no pause camera: the minigame keeps its own */
    start->rules.x1_2 = true;     /* no "Ready... GO!": MP4 shows its own start */
    start->rules.x30 = 0.0f;      /* nobody is hurt */
    start->rules.on_match_start = cs_start;
    start->rules.on_frame_start = cs_frame;
}

/* Fighter_procInput: wading by the stick while the player is free, a jab as a sweep begins,
 * following the player the rest of the time. */
static void cs_fighter_input(Fighter* fp)
{
    int slot = fp->player_idx;
    s32 state, team;
    float stick, heading;
    int playing;
    if (!party_arena_input_begin(fp) || cs.ended) {
        return;
    }
    if (!mp4_m434_player(slot, &state, &stick, &heading, &team, &playing)) {
        return;
    }
    if (playing && state == ST_FREE) {
        /* the stick back from MP4's fraction and heading (atan2d(x, -y)) */
        float a = heading * 3.14159265f / 180.0f;
        cs.swept[slot] = 0;
        party_arena_walk(fp, (s16) (72.0f * stick * sinf(a)), (s16) (-72.0f * stick * cosf(a)));
        return;
    }
    party_arena_follow(fp, 0);
    if (playing && state == ST_SWEEP && !cs.swept[slot]) {
        cs.swept[slot] = 1;
        fp->input.held_buttons[0] |= HSD_PAD_A;
    }
}

static float cs_knockback(Fighter* fp, float kb)
{
    (void) fp;
    (void) kb;
    return 0.0f;
}

/* MP4 gives each winner 10 coins and the others nothing; nobody when it is a draw. */
static void cs_result(s8 place[PARTY_PLAYERS])
{
    int i, any = 0;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        any |= mp4_player_coins(i) > 0;
    }
    for (i = 0; i < PARTY_PLAYERS; i++) {
        place[i] = (s8) (!any || mp4_player_coins(i) > 0 ? 0 : 3);
    }
}

const PartyMinigame mg_mp4_m434 = {
    "Cheep Cheep Sweep", "cheep-cheep-sweep", cs_setup, cs_fighter_input, cs_result, 0, NULL,
    St_Kind_Last, -1, party_arena_map, party_arena_drawn, cs_knockback, 1,
    party_arena_camera_views, party_arena_camera_view, 1,
};
