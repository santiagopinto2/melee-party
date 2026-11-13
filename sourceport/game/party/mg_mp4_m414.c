/* Melee Party: Long Claw of the Law, Mario Party 4's m414, in a Melee match.
 *
 * The minigame is MP4's own code (mp4/m414), run by the MP4 runtime (mp4/mp4_party.c): a
 * four-way split screen, one Hu3D camera a player (party_arena_camera_views), each player at the
 * controls of a long claw of its own. The prizes sit in a ring round it on two shelves behind
 * shutters that open and close; the stick raises and lowers the claw between the shelves, L and
 * R turn it a step to the next pair of prizes, and A shoots it out. The first to catch the three
 * prizes that match the one on the sign wins; after 60 seconds, nobody does. Its rules, CPUs,
 * shutters, sprites and banners are MP4's. The four players are MP4's player objects, each
 * driving a hidden MP4 character model at its claw's controls; nobody walks in this game, so the
 * fighters stand where those models stand and follow them (party_arena_follow), with a jab as
 * the claw goes for a prize and a taunt for the win. L and R turn the claw opposite
 * ways, so a human's triggers reach MP4 apart (mp4_match_triggers_apart). The match ends when the
 * minigame returns to MP4's boot overlay. MP4 gives the winner 10 coins and the others nothing;
 * the winner is first and the others are placed by the prizes they caught, ties sharing, a
 * player with none last. With no winner (time ran out) MP4 pays nobody: everyone last. */
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

/* m414's claw states (mp4_m414_player) */
enum { CLAW_GRAB_UP = 4, CLAW_GRAB_DOWN = 5 };

int mp4_m414_player(int player, s32* claw, s32* turning, s32* catches, s32* phase);

static struct {
    int ended;
    s32 claw[PARTY_PLAYERS];      /* the claw state seen last frame */
    s32 catches[PARTY_PLAYERS];   /* MP4's catch counters, kept for the placements */
} lc;

static void lc_start(void)
{
    int i;
    ifAll_802F3394();   /* no damage percents or stocks */
    memset(&lc, 0, sizeof lc);
    for (i = 0; i < PARTY_PLAYERS; i++) {
        lc.claw[i] = -1;
    }
    mp4_match_triggers_apart();
    party_arena_begin(mp4_overlay_m414, WORLD_X, 0.0f, 0.0f);
}

static void lc_frame(void)
{
    party_arena_frame();
    /* over when MP4 returns to its boot overlay; at once without a disc (MELEE_PARTY_MINIGAME
     * can still name this minigame then: the lists do not) */
    if (!lc.ended && (mp4_match_over() || !mp4_available())) {
        int i;
        lc.ended = 1;
        for (i = 0; i < PARTY_PLAYERS; i++) {
            party_log("m414: P%d coins %d catches %d", i + 1, mp4_player_coins(i), lc.catches[i]);
        }
        gm_8016B328();
    }
}

static void lc_setup(StartMeleeData* start)
{
    memset(&lc, 0, sizeof lc);
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.x4_0 = false;    /* no pause camera: the minigame keeps its own */
    start->rules.x1_2 = true;     /* no "Ready... GO!": MP4 shows its own start */
    start->rules.x30 = 0.0f;      /* nobody is hurt */
    start->rules.on_match_start = lc_start;
    start->rules.on_frame_start = lc_frame;
}

/* Fighter_procInput: the fighter follows its player throughout (nobody walks), and jabs as the
 * claw goes for a prize. */
static void lc_fighter_input(Fighter* fp)
{
    int slot = fp->player_idx;
    s32 claw, turning, catches, phase;
    if (!party_arena_input_begin(fp) || lc.ended) {
        return;
    }
    if (!mp4_m414_player(slot, &claw, &turning, &catches, &phase)) {
        return;
    }
    lc.catches[slot] = catches;
    party_arena_follow(fp, 0);
    if ((claw == CLAW_GRAB_UP || claw == CLAW_GRAB_DOWN) && lc.claw[slot] != claw) {
        fp->input.held_buttons[0] |= HSD_PAD_A;
    }
    lc.claw[slot] = claw;
}

static float lc_knockback(Fighter* fp, float kb)
{
    (void) fp;
    (void) kb;
    return 0.0f;
}

/* MP4 gives the winner 10 coins: first. The others by their catches, ties sharing, after the
 * winner; a player with no catch last. A draw (MP4 pays nobody) puts everyone last. */
static void lc_result(s8 place[PARTY_PLAYERS])
{
    int i, j, any = 0;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        any |= mp4_player_coins(i) > 0;
    }
    for (i = 0; i < PARTY_PLAYERS; i++) {
        int p = 0;
        if (!any) {
            place[i] = 3;
            continue;
        }
        if (mp4_player_coins(i) > 0) {
            place[i] = 0;
            continue;
        }
        if (lc.catches[i] <= 0) {
            place[i] = 3;
            continue;
        }
        for (j = 0; j < PARTY_PLAYERS; j++) {
            if (mp4_player_coins(j) > 0 || lc.catches[j] > lc.catches[i]) {
                p++;
            }
        }
        place[i] = (s8) (p > 3 ? 3 : p);
    }
}

const PartyMinigame mg_mp4_m414 = {
    "Long Claw of the Law", "long-claw-of-the-law", lc_setup, lc_fighter_input, lc_result, 0, NULL,
    St_Kind_Last, -1, party_arena_map, party_arena_drawn, lc_knockback, 1,
    party_arena_camera_views, party_arena_camera_view, 1,
};
