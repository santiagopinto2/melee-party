/* Melee Party: Right Oar Left?, Mario Party 4's m427, in a Melee match.
 *
 * The minigame is MP4's own code (mp4/m427), run by the MP4 runtime (mp4/mp4_party.c): a race of
 * two rowing boats down a river, its rocks and fire traps, its water (drawn from a copy of each
 * half of the screen), its rules, CPUs, timer and banners, and the winners' row across the lake
 * at the end. The screen is split in two, one boat a half (Hu3D cameras 0 and 1, so the runtime
 * makes two views: party_arena_camera_views), and is whole again for the end. Two against two:
 * MP4 seats the pairs in the boats by GWPlayerCfg.group, which its board decides; the party pairs
 * the players at random (as Dungeon Duos does) and hands the groups to the runtime
 * (mp4_match_groups). Each rower pulls the left oar with A and the right one with B; a CPU next to
 * a human copies the human's strokes. The four players are MP4's player objects, each driving a
 * hidden MP4 character model seated in its boat; the Melee fighters stand where those models sit,
 * through party_arena.c. Nobody walks: MP4 moves the boats, the fighters follow, jab at each pull
 * of an oar and taunt when their boat wins. Both boats race on the same course, each drawn in its
 * own half only, so each half shows only its own boat's fighters (ro_camera_view). Nobody is hit:
 * the river runs below the floor of Final Destination, so a fighter sent flying from a crash
 * would fall out of the stage; the crashed boat spins and its rowers flicker, as in MP4. The match
 * ends when the minigame returns to MP4's boot overlay; MP4 gives both rowers of the first boat
 * across the line 10 coins, so the winning pair shares first place and the other last; when the
 * five minutes run out nobody wins and all four share last. */
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

/* m427's boat states, as each rower sees them (mp4_m427_player) */
enum { ST_GLIDE = 0, ST_RACE = 2, ST_ROW_IN = 5, ST_WIN = 6 };

int mp4_m427_player(int player, s32* boat, s32* state, int* stroke);

static struct {
    int ended;
    s8 groups[PARTY_PLAYERS];
    s8 boat[PARTY_PLAYERS];             /* the player's boat, -1 until MP4 has seated it */
    int won[PARTY_PLAYERS];             /* the fighter taunted for its boat's win */
    Fighter* fp[PARTY_PLAYERS][2];      /* each slot's fighters (Nana second), for the views */
    u8 hidden[PARTY_PLAYERS][2];        /* hidden from the view being drawn (Fighter.invisible) */
} ro;

static void ro_start(void)
{
    ifAll_802F3394();   /* no damage percents or stocks */
    mp4_match_groups(ro.groups);
    party_arena_begin(mp4_overlay_m427, WORLD_X, 0.0f, 0.0f);
}

static void ro_frame(void)
{
    party_arena_frame();
    /* over when MP4 returns to its boot overlay; at once without a disc (MELEE_PARTY_MINIGAME
     * can still name this minigame then: the lists do not) */
    if (!ro.ended && (mp4_match_over() || !mp4_available())) {
        int i;
        ro.ended = 1;
        for (i = 0; i < PARTY_PLAYERS; i++) {
            party_log("m427: P%d pair %d boat %d coins %d", i + 1, ro.groups[i], ro.boat[i],
                      mp4_player_coins(i));
        }
        gm_8016B328();
    }
}

static void ro_setup(StartMeleeData* start)
{
    int order[PARTY_PLAYERS] = { 0, 1, 2, 3 };
    int i;
    memset(&ro, 0, sizeof ro);
    /* random pairs, as Dungeon Duos pairs its teams */
    for (i = PARTY_PLAYERS - 1; i > 0; i--) {
        int j = party_rand(i + 1);
        int t = order[i];
        order[i] = order[j];
        order[j] = t;
    }
    for (i = 0; i < PARTY_PLAYERS; i++) {
        ro.groups[order[i]] = (s8) (i >> 1);
        ro.boat[i] = -1;
    }
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.x4_0 = false;    /* no pause camera: the minigame keeps its own */
    start->rules.x1_2 = true;     /* no "Ready... GO!": MP4 shows its own start */
    start->rules.x30 = 0.0f;      /* nobody is hurt */
    start->rules.on_match_start = ro_start;
    start->rules.on_frame_start = ro_frame;
}

/* Fighter_procInput: the fighter follows its seat in the boat, jabs as its player pulls an oar
 * (racing, and while MP4 rows the boats in at the start and the winners' across the lake) and
 * taunts once its boat has won. */
static void ro_fighter_input(Fighter* fp)
{
    int slot = fp->player_idx;
    s32 boat, state;
    int stroke;
    if (slot >= 0 && slot < PARTY_PLAYERS) {
        int sub = fp->is_sub_fighter ? 1 : 0;
        ro.fp[slot][sub] = fp;
        /* shown again for the frame's logic and a whole-screen draw (ro_camera_view) */
        if (ro.hidden[slot][sub]) {
            ro.hidden[slot][sub] = 0;
            fp->invisible = false;
        }
    }
    if (!party_arena_input_begin(fp) || ro.ended) {
        return;
    }
    if (!mp4_m427_player(slot, &boat, &state, &stroke)) {
        return;
    }
    ro.boat[slot] = (s8) boat;
    party_arena_follow(fp, 0);
    if (stroke && (state == ST_GLIDE || state == ST_RACE || state == ST_ROW_IN)) {
        fp->input.held_buttons[0] |= HSD_PAD_A;
    }
    if (state == ST_WIN && !ro.won[slot]) {
        ro.won[slot] = 1;
        fp->input.held_buttons[0] = HSD_PAD_DPADUP;
    }
}

/* camera_view: the half of the screen of one boat. The runtime orders its views by Hu3D camera,
 * and boat b is drawn by camera b; of the two views, call 1 is drawn first and is view 0
 * (mp4_view_begin). The other boat sits on the same course, drawn only in its own half, so its
 * fighters stay out of this half. */
static void ro_camera_view(int view, struct HSD_CObj* cobj)
{
    int shown = 1 - view;
    int i, sub;
    party_arena_camera_view(view, cobj);
    for (i = 0; i < PARTY_PLAYERS; i++) {
        for (sub = 0; sub < 2; sub++) {
            Fighter* fp = ro.fp[i][sub];
            int hide;
            if (fp == NULL || ro.boat[i] < 0) {
                continue;
            }
            hide = ro.boat[i] != shown;
            fp->invisible = hide;
            ro.hidden[i][sub] = (u8) hide;
        }
    }
}

static float ro_knockback(Fighter* fp, float kb)
{
    (void) fp;
    (void) kb;
    return 0.0f;
}

/* MP4 gives both rowers of the winning boat 10 coins and the others nothing; nobody when the
 * time runs out. */
static void ro_result(s8 place[PARTY_PLAYERS])
{
    int i;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        place[i] = (s8) (mp4_player_coins(i) > 0 ? 0 : 3);
    }
}

const PartyMinigame mg_mp4_m427 = {
    "Right Oar Left?", "right-oar-left", ro_setup, ro_fighter_input, ro_result, 0, NULL,
    St_Kind_Last, -1, party_arena_map, party_arena_drawn, ro_knockback, 1,
    party_arena_camera_views, ro_camera_view, 1,
};
