/* Melee Party: Revers-a-Bomb, Mario Party 4's m426, in a Melee match.
 *
 * The minigame is MP4's own code (mp4/m426): the six lanes between the two teams, the arrows on
 * them, the bombs, the blasts, the lives, its rules, CPUs, timer and banners. Two against two: MP4
 * tells the pairs apart by GWPlayerCfg.group, which its board decides; the party pairs the players
 * at random (as Cheep Cheep Sweep does) and hands the groups to the runtime (mp4_match_groups).
 * The four players are MP4's player objects, each driving a hidden MP4 character model; the Melee
 * fighters stand where those models stand, through party_arena.c. Nobody walks by the stick here:
 * the stick only picks the next lane, and MP4 hops the player there at its own fixed speed (the
 * timing of the bombs counts on it), so the fighters follow throughout, running in place while a
 * player hops. A swing of the bat (A) is MP4's, on the hidden model, and the fighter jabs as it
 * comes round; a blast sends the player through the air, and the fighter takes one Melee hit and
 * follows MP4's flight. The match ends when the minigame returns to MP4's boot overlay; MP4 gives
 * each player of the pair with more lives left 10 coins, so that pair shares first place and the
 * other last; with the lives level nobody gets coins and everyone is last. */
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

/* m426's player states (mp4_m426_player): 2 swings the bat (the swing counts frames from 1; the
 * bat meets a bomb at 17), 3 and 5 are blasted through the air. */
enum { ST_SWING = 2, ST_BLASTED = 3, ST_BOUNCE = 5 };
/* the fighter's jab, this many frames into the swing (a jab's hit comes a few frames after the
 * press, so it lands about when the bat meets the bomb) */
#define JAB_FRAME 11

int mp4_m426_player(int player, s32* state, s32* swing, s32* team);

static struct {
    int ended;
    s8 groups[PARTY_PLAYERS];
    int jabbed[PARTY_PLAYERS];   /* the fighter jabbed for this swing */
    int hit[PARTY_PLAYERS];      /* the fighter took its hit for this blast */
} rb;

static void rb_start(void)
{
    ifAll_802F3394();   /* no damage percents or stocks */
    mp4_match_groups(rb.groups);
    party_arena_begin(mp4_overlay_m426, WORLD_X, 0.0f, 0.0f);
}

static void rb_frame(void)
{
    party_arena_frame();
    /* over when MP4 returns to its boot overlay; at once without a disc (MELEE_PARTY_MINIGAME
     * can still name this minigame then: the lists do not) */
    if (!rb.ended && (mp4_match_over() || !mp4_available())) {
        int i;
        rb.ended = 1;
        for (i = 0; i < PARTY_PLAYERS; i++) {
            party_log("m426: P%d pair %d coins %d", i + 1, rb.groups[i], mp4_player_coins(i));
        }
        gm_8016B328();
    }
}

static void rb_setup(StartMeleeData* start)
{
    int order[PARTY_PLAYERS] = { 0, 1, 2, 3 };
    int i;
    memset(&rb, 0, sizeof rb);
    /* random pairs, as Cheep Cheep Sweep pairs its teams */
    for (i = PARTY_PLAYERS - 1; i > 0; i--) {
        int j = party_rand(i + 1);
        int t = order[i];
        order[i] = order[j];
        order[j] = t;
    }
    for (i = 0; i < PARTY_PLAYERS; i++) {
        rb.groups[order[i]] = (s8) (i >> 1);
    }
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.x4_0 = false;    /* no pause camera: the minigame keeps its own */
    start->rules.x1_2 = true;     /* no "Ready... GO!": MP4 shows its own start */
    start->rules.x30 = 0.0f;      /* nobody is hurt */
    start->rules.on_match_start = rb_start;
    start->rules.on_frame_start = rb_frame;
}

/* Fighter_procInput: the fighter follows its player (running in place while it hops between
 * lanes), jabs as the bat swings, and takes one hit when a blast throws the player. */
static void rb_fighter_input(Fighter* fp)
{
    int slot = fp->player_idx;
    s32 state, swing, team;
    if (!party_arena_input_begin(fp) || rb.ended) {
        return;
    }
    if (!mp4_m426_player(slot, &state, &swing, &team)) {
        return;
    }
    if (state == ST_BLASTED || state == ST_BOUNCE) {
        if (!rb.hit[slot]) {
            rb.hit[slot] = 1;
            party_arena_hit(fp);
        } else {
            party_arena_follow(fp, 1);
        }
        return;
    }
    rb.hit[slot] = 0;
    party_arena_follow(fp, 0);
    if (state != ST_SWING) {
        rb.jabbed[slot] = 0;
    } else if (!rb.jabbed[slot] && swing >= JAB_FRAME) {
        rb.jabbed[slot] = 1;
        fp->input.held_buttons[0] |= HSD_PAD_A;
    }
}

static float rb_knockback(Fighter* fp, float kb)
{
    (void) fp;
    (void) kb;
    return 0.0f;
}

/* MP4 gives each player of the pair with more lives left 10 coins and the others nothing; nobody
 * when the lives are level. */
static void rb_result(s8 place[PARTY_PLAYERS])
{
    int i, any = 0;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        any |= mp4_player_coins(i) > 0;
    }
    for (i = 0; i < PARTY_PLAYERS; i++) {
        place[i] = (s8) (any && mp4_player_coins(i) > 0 ? 0 : 3);
    }
}

const PartyMinigame mg_mp4_m426 = {
    "Revers-a-Bomb", "revers-a-bomb", rb_setup, rb_fighter_input, rb_result, 0, NULL,
    St_Kind_Last, -1, party_arena_map, party_arena_drawn, rb_knockback, 1, NULL, NULL, 1,
};
