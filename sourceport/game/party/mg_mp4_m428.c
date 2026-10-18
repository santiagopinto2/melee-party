/* Melee Party: Cliffhangers, Mario Party 4's m428, in a Melee match.
 *
 * The minigame is MP4's own code (mp4/m428), run by the MP4 runtime (mp4/mp4_party.c): two pairs
 * race up a cliff in a gale, roped together, each pair on its own copy of the cliff in its half
 * of a split screen (Hu3D cameras 0 and 1, one view each: party_arena_camera_views), with the
 * gusts, the rope, the meter between the views, its rules, CPUs and banners. The stick climbs, A
 * holds on through a gust; whoever lets go hangs from the rope until the partner holds. Two against
 * two: MP4 tells the pairs apart by GWPlayerCfg.group, which its board decides; the party pairs
 * the players at random (as Dungeon Duos does) and hands the groups to the runtime
 * (mp4_match_groups). The four players are MP4's player objects, each driving a hidden MP4
 * character model; the Melee fighters stand where those models stand, through party_arena.c.
 * Nobody walks on the ground here, so MP4 moves every player and the fighters follow throughout
 * (party_arena_follow), crouched while they hold on. Both copies of the cliff sit at the same
 * place in MP4's world, so each view shows its own pair's fighters only (cl_camera_view). The
 * match ends when the minigame returns to MP4's boot overlay; MP4 gives both climbers of the pair
 * that reaches the top first 10 coins, so that pair shares first place and the other last. */
#include <string.h>

#include <melee/ft/fighter.h>
#include <melee/ft/types.h>
#include <melee/gm/gmvs.h>
#include <melee/gr/forward.h>
#include <melee/if/ifall.h>
#include <sysdolphin/baselib/gobj.h>
#include <sysdolphin/baselib/jobj.h>

#include "mp4/mp4.h"
#include "party.h"
#include "party_arena.h"

#define WORLD_X 600.0f

/* m428's player states (mp4_m428_player) */
enum { ST_HOLD = 3 };

int mp4_m428_player(int player, s32* state, s32* team, int* playing);

/* hsfman.c: the Hu3D cameras the view being drawn renders (bits; set by mp4_view_begin), -1 for
 * all */
extern s16 mp4_view_cameras;

static struct {
    int ended;
    s8 groups[PARTY_PLAYERS];
    Fighter* fp[PARTY_PLAYERS][2];   /* each slot's fighter and its sub fighter, for the views */
} cl;

static void cl_start(void)
{
    ifAll_802F3394();   /* no damage percents or stocks */
    mp4_match_groups(cl.groups);
    party_arena_begin(mp4_overlay_m428, WORLD_X, 0.0f, 0.0f);
}

static void cl_frame(void)
{
    party_arena_frame();
    /* over when MP4 returns to its boot overlay; at once without a disc (MELEE_PARTY_MINIGAME
     * can still name this minigame then: the lists do not) */
    if (!cl.ended && (mp4_match_over() || !mp4_available())) {
        int i;
        cl.ended = 1;
        for (i = 0; i < PARTY_PLAYERS; i++) {
            party_log("m428: P%d pair %d coins %d", i + 1, cl.groups[i], mp4_player_coins(i));
        }
        gm_8016B328();
    }
}

static void cl_setup(StartMeleeData* start)
{
    int order[PARTY_PLAYERS] = { 0, 1, 2, 3 };
    int i;
    memset(&cl, 0, sizeof cl);
    /* random pairs, as Dungeon Duos pairs its teams */
    for (i = PARTY_PLAYERS - 1; i > 0; i--) {
        int j = party_rand(i + 1);
        int t = order[i];
        order[i] = order[j];
        order[j] = t;
    }
    for (i = 0; i < PARTY_PLAYERS; i++) {
        cl.groups[order[i]] = (s8) (i >> 1);
    }
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.x4_0 = false;    /* no pause camera: the minigame keeps its own */
    start->rules.x1_2 = true;     /* no "Ready... GO!": MP4 shows its own start */
    start->rules.x30 = 0.0f;      /* nobody is hurt */
    start->rules.on_match_start = cl_start;
    start->rules.on_frame_start = cl_frame;
}

/* Fighter_procInput: the fighter follows its player all the time, on the ground as far as Melee
 * knows (it is drawn at the player's height on the cliff, party_arena_drawn), crouched while the
 * player holds on through a gust. */
static void cl_fighter_input(Fighter* fp)
{
    int slot = fp->player_idx;
    s32 state, team;
    int playing;
    if (slot >= 0 && slot < PARTY_PLAYERS) {
        cl.fp[slot][fp->is_sub_fighter ? 1 : 0] = fp;
    }
    if (!party_arena_input_begin(fp) || cl.ended) {
        return;
    }
    if (!mp4_m428_player(slot, &state, &team, &playing)) {
        return;
    }
    party_arena_follow(fp, 0);
    if (playing && state == ST_HOLD) {
        fp->input.lstick[0].y = -1.0f;
    }
}

static float cl_knockback(Fighter* fp, float kb)
{
    (void) fp;
    (void) kb;
    return 0.0f;
}

/* One view of the split screen: the runtime's view (its camera, viewport and eye), and only the
 * fighters of the pair climbing in it. The other pair climbs the same spot of MP4's world in the
 * other view, so its fighters go out of sight for this one (as party_arena_drawn hides a player
 * the game hid); party_arena_drawn puts every fighter back where its player is first. */
static void cl_camera_view(int view, struct HSD_CObj* cobj)
{
    int slot, k;
    party_arena_camera_view(view, cobj);
    if (cl.ended) {
        return;
    }
    for (slot = 0; slot < PARTY_PLAYERS; slot++) {
        s32 state, team;
        int playing, shown;
        if (!mp4_m428_player(slot, &state, &team, &playing)) {
            continue;
        }
        shown = mp4_view_cameras < 0 || (mp4_view_cameras & (1 << team)) != 0;
        for (k = 0; k < 2; k++) {
            Fighter* fp = cl.fp[slot][k];
            Vec3 pos;
            if (fp == NULL || fp->gobj == NULL || fp->gobj->hsd_obj == NULL) {
                continue;
            }
            party_arena_drawn(fp);
            if (!shown) {
                HSD_JObjGetTranslation(fp->gobj->hsd_obj, &pos);
                pos.y = -4000.0f;
                HSD_JObjSetTranslate(fp->gobj->hsd_obj, &pos);
            }
        }
    }
}

/* MP4 gives both climbers of the pair at the top first 10 coins and the others nothing (the
 * first pair to reach it; a coin toss when both do on the same frame). When the time runs out
 * with neither at the top it is a draw and nobody gets any: everyone is last. */
static void cl_result(s8 place[PARTY_PLAYERS])
{
    int i;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        place[i] = (s8) (mp4_player_coins(i) > 0 ? 0 : 3);
    }
}

const PartyMinigame mg_mp4_m428 = {
    "Cliffhangers", "cliffhangers", cl_setup, cl_fighter_input, cl_result, 0, NULL,
    St_Kind_Last, -1, party_arena_map, party_arena_drawn, cl_knockback, 1,
    party_arena_camera_views, cl_camera_view, 1,
};
