/* Melee Party: Bob-omb Breakers, Mario Party 4's m413, in a Melee match.
 *
 * The minigame is MP4's own code (mp4/m413), run by the MP4 runtime (mp4/mp4_party.c): the board
 * of twelve columns, each player's section of three, the pairs of coloured blocks and Bob-ombs
 * that fall into it, the groups of three that break, the scores, the CPUs and the banners. A
 * human's stick and buttons play their falling pair as in MP4 (the stick moves it and drops it
 * faster, A and B turn it, X and Y flip it). Nobody walks: each player's MP4 character stands in
 * a window over its section; it is hidden, and the Melee fighter stands in its place, facing the
 * camera, and does not move. It crouches while its section is
 * overflowed (no more blocks for it) and once it has lost, and the winner taunts when the result
 * shows. The match ends when the minigame returns to MP4's boot overlay. MP4 gives the winner
 * (the first to 100 points, or the only player whose section has not overflowed) 10 coins and
 * everyone else nothing, and nobody anything on a draw (every section overflowed): the winner
 * takes first place, the others follow by their points (equal points share a place), and a draw
 * puts everyone last.
 *
 * The Melee side: Final Destination, each fighter colliding on a lane of its own (party_arena.c),
 * with the MP4 board drawn 600 units to the side, clear of the stage's model. */
#include <string.h>

#include <melee/ft/fighter.h>
#include <melee/ft/ftparts.h>
#include <melee/ft/types.h>
#include <melee/gm/gmvs.h>
#include <melee/gr/forward.h>
#include <melee/if/ifall.h>
#include <sysdolphin/baselib/controller.h>
#include <sysdolphin/baselib/gobj.h>
#include <sysdolphin/baselib/jobj.h>

#include "mp4/mp4.h"
#include "party.h"
#include "party_arena.h"

#define WORLD_X 600.0f

/* How far in front of its MP4 character (MP4 units) the fighter stands: the character, hidden
 * (m413/main.c), stands in a window over its section, and the fighter takes its place. */
#define PANEL_FRONT 20.0f

/* mp4_m413_player's result */
enum { RES_ON = 0, RES_WON = 1, RES_LOST = 2, RES_DRAW = 3 };

int mp4_m413_player(int player, float* x, float* y, float* z, int* score, int* out, int* result);

static struct {
    int ended;
    struct {
        int placed;        /* the character's place is known */
        float x, y, z;     /* the MP4 character over the player's section, MP4 units */
        int score;         /* the points MP4 counted, kept for the placements */
        int taunted;       /* the winner's taunt was started */
    } p[PARTY_PLAYERS];
} bb;

static void bb_start(void)
{
    ifAll_802F3394();   /* no damage percents or stocks */
    memset(&bb, 0, sizeof bb);
    party_arena_begin(mp4_overlay_m413, WORLD_X, 0.0f, 0.0f);
}

static void bb_frame(void)
{
    party_arena_frame();
    /* over when MP4 returns to its boot overlay; at once without a disc (MELEE_PARTY_MINIGAME
     * can still name this minigame then: the lists do not) */
    if (!bb.ended && (mp4_match_over() || !mp4_available())) {
        int i;
        bb.ended = 1;
        for (i = 0; i < PARTY_PLAYERS; i++) {
            party_log("m413: P%d coins %d points %d", i + 1, mp4_player_coins(i), bb.p[i].score);
        }
        gm_8016B328();
    }
}

static void bb_setup(StartMeleeData* start)
{
    memset(&bb, 0, sizeof bb);
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.x4_0 = false;    /* no pause camera: the minigame keeps its own */
    start->rules.x1_2 = true;     /* no "Ready... GO!": MP4 shows its own start */
    start->rules.x30 = 0.0f;      /* nobody is hurt */
    start->rules.on_match_start = bb_start;
    start->rules.on_frame_start = bb_frame;
}

/* Fighter_procInput: a human's pad goes to MP4 (party_arena_input_begin), and the fighter only
 * follows: it stands still, crouches while its section is overflowed or once it lost, and taunts
 * once when it won. */
static void bb_fighter_input(Fighter* fp)
{
    int slot = fp->player_idx;
    float x, y, z;
    int score, out, result;
    if (!party_arena_input_begin(fp) || bb.ended) {
        return;
    }
    if (!mp4_m413_player(slot, &x, &y, &z, &score, &out, &result)) {
        return;
    }
    bb.p[slot].placed = 1;
    bb.p[slot].x = x;
    bb.p[slot].y = y;
    bb.p[slot].z = z;
    bb.p[slot].score = score;
    party_arena_follow(fp, 0);
    if (result == RES_WON) {
        if (!bb.p[slot].taunted) {
            bb.p[slot].taunted = 1;
            fp->input.held_buttons[0] = HSD_PAD_DPADUP;
        }
    } else if (out || result == RES_LOST) {
        fp->input.lstick[0].y = -1.0f;
    }
}

/* After collision: drawn where its MP4 character stands over its section, facing the camera (MP4's yaw 0),
 * plus whatever height Melee gave it. */
static void bb_fighter_drawn(Fighter* fp)
{
    int slot = fp->player_idx;
    Vec3 pos;
    if (slot < 0 || slot >= PARTY_PLAYERS || !bb.p[slot].placed) {
        return;
    }
    pos.x = WORLD_X + bb.p[slot].x * MP4_SCALE;
    pos.y = bb.p[slot].y * MP4_SCALE + fp->cur_pos.y;
    pos.z = (bb.p[slot].z + PANEL_FRONT) * MP4_SCALE;
    if (fp->is_sub_fighter) {
        pos.z -= 3.0f;
    }
    ftPartSetRotY(fp, 0, 0.0f);
    HSD_JObjSetTranslate(fp->gobj->hsd_obj, &pos);
}

static float bb_knockback(Fighter* fp, float kb)
{
    (void) fp;
    (void) kb;
    return 0.0f;
}

/* MP4 gives the winner 10 coins and the rest nothing (nobody anything on a draw). The winner is
 * first; the others follow by their points, equal points sharing a place; a draw (or no game)
 * puts everyone last. */
static void bb_result(s8 place[PARTY_PLAYERS])
{
    int i, j, winner = -1;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        if (mp4_player_coins(i) > 0) {
            winner = i;
        }
    }
    for (i = 0; i < PARTY_PLAYERS; i++) {
        int ahead = 1;
        if (winner < 0 || i == winner) {
            place[i] = (s8) (winner < 0 ? 3 : 0);
            continue;
        }
        for (j = 0; j < PARTY_PLAYERS; j++) {
            if (j != winner && j != i && bb.p[j].score > bb.p[i].score) {
                ahead++;
            }
        }
        place[i] = (s8) ahead;
    }
}

const PartyMinigame mg_mp4_m413 = {
    "Bob-omb Breakers", "bob-omb-breakers", bb_setup, bb_fighter_input, bb_result, 0, NULL,
    St_Kind_Last, -1, party_arena_map, bb_fighter_drawn, bb_knockback, 1, NULL, NULL, 1,
};
