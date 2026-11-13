/* Melee Party: Photo Finish, Mario Party 4's m411, in a Melee match.
 *
 * The minigame is MP4's own code (mp4/m411), run by the MP4 runtime (mp4/mp4_party.c): one of its
 * three scenes is rendered every frame into a corner of the screen and copied (camera 0), and that
 * copy is the photo each player puts back together in a quarter of the screen of its own (cameras
 * 1 to 4): eight pieces, moved with the stick, turned with L and R, dropped in place with A. The
 * first to finish wins; after 60 seconds nobody does. MP4 runs all of it: the scenes, the pieces,
 * the CPUs, the timer, the banners and the ending, where the winner's view fills the screen and its
 * character walks in and jumps into the photo.
 *
 * The Melee side: nobody walks in this game, so the fighters only follow (party_arena.c). The
 * runtime draws them with one camera on the whole screen (m411 gives the characters a camera of
 * their own, with the quarters' eye), so they cannot stand in four quarters at once: they stay out
 * of sight while the puzzles are up, and the winner's fighter shows when its character walks into
 * its view at the end, until it jumps into the photo (the copy, where no fighter can be drawn).
 * The match ends when the minigame returns to MP4's boot overlay; MP4 gives the winner 10 coins
 * and the others nothing, so the winner takes first place and the others share last, everyone on
 * a draw. */
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

int mp4_m411_player(int player, s32* phase, int* won, int* in_photo);

static struct {
    int ended;
} pf;

static void pf_start(void)
{
    ifAll_802F3394();   /* no damage percents or stocks */
    memset(&pf, 0, sizeof pf);
    mp4_match_triggers_apart();   /* L and R turn a piece opposite ways */
    party_arena_begin(mp4_overlay_m411, WORLD_X, 0.0f, 0.0f);
}

static void pf_frame(void)
{
    party_arena_frame();
    /* over when MP4 returns to its boot overlay; at once without a disc (MELEE_PARTY_MINIGAME
     * can still name this minigame then: the lists do not) */
    if (!pf.ended && (mp4_match_over() || !mp4_available())) {
        int i;
        pf.ended = 1;
        for (i = 0; i < PARTY_PLAYERS; i++) {
            party_log("m411: P%d coins %d", i + 1, mp4_player_coins(i));
        }
        gm_8016B328();
    }
}

static void pf_setup(StartMeleeData* start)
{
    memset(&pf, 0, sizeof pf);
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.x4_0 = false;    /* no pause camera: the minigame keeps its own */
    start->rules.x1_2 = true;     /* no "Ready... GO!": MP4 shows its own start */
    start->rules.x30 = 0.0f;      /* nobody is hurt */
    start->rules.on_match_start = pf_start;
    start->rules.on_frame_start = pf_frame;
}

/* Fighter_procInput: a human's pad goes to MP4 (the stick moves the piece, L and R turn it, A
 * drops it), and the fighter follows its character the whole game. */
static void pf_fighter_input(Fighter* fp)
{
    if (!party_arena_input_begin(fp) || pf.ended) {
        return;
    }
    party_arena_follow(fp, 0);
}

/* After collision: drawn where its character stands, but only the winner's, from when the
 * character walks into the winner's view until it jumps into the photo; out of sight otherwise
 * (the characters stand in the players' quarters, which the fighters' one camera does not see). */
static void pf_fighter_drawn(Fighter* fp)
{
    int slot = fp->player_idx;
    s32 phase;
    int won, in_photo;
    Vec3 pos;
    party_arena_drawn(fp);
    if (slot < 0 || slot >= PARTY_PLAYERS) {
        return;
    }
    if (mp4_m411_player(slot, &phase, &won, &in_photo) && won && !in_photo) {
        return;
    }
    pos = fp->cur_pos;
    pos.y = -4000.0f;
    HSD_JObjSetTranslate(fp->gobj->hsd_obj, &pos);
}

static float pf_knockback(Fighter* fp, float kb)
{
    (void) fp;
    (void) kb;
    return 0.0f;
}

/* MP4 gives the winner 10 coins and the others nothing; nobody anything on a draw. */
static void pf_result(s8 place[PARTY_PLAYERS])
{
    int i;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        place[i] = (s8) (mp4_player_coins(i) > 0 ? 0 : 3);
    }
}

const PartyMinigame mg_mp4_m411 = {
    "Photo Finish", "photo-finish", pf_setup, pf_fighter_input, pf_result, 0, NULL,
    St_Kind_Last, -1, party_arena_map, pf_fighter_drawn, pf_knockback, 1, NULL, NULL, 1,
};
