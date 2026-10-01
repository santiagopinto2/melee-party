/* Melee Party: the board. One board turn is one VS match on Final Destination in which every
 * player moves once, in order.
 *
 * The board is a loop of spaces along the floor, drawn as discs (party_draw.c). The fighters are
 * real: each frame the board writes the inputs of every fighter (mu_party_fighter_input), so they
 * walk with their own animations, and nobody can attack. Coming off the last space the path loops
 * back to the first.
 *
 * Per mover: a die counts 1..10 until A (a CPU presses it after a moment), then the fighter walks
 * that many spaces, buying a star for STAR_COST coins if it passes the star, and the space it
 * lands on pays or takes coins. */
#include <math.h>
#include <string.h>

#include <melee/cm/camera.h>
#include <melee/cm/types.h>
#include <melee/ft/inlines.h>
#include <melee/ft/types.h>
#include <melee/gm/gmvs.h>
#include <melee/gr/forward.h>
#include <melee/if/ifall.h>
#include <melee/pl/player.h>
#include <sysdolphin/baselib/controller.h>
#include <sysdolphin/baselib/gobj.h>

#include "party.h"
#include "party_draw.h"
#include "party_hud.h"

#define SPACES 12
#define SPACE_X0 (-66.0f)
#define SPACE_STEP 12.0f
#define FLOOR_Y 0.0f
#define STAR_COST 20
#define DIE_MAX 10
#define ARRIVE 2.5f      /* close enough to a space */
#define STALL_FRAMES 600 /* a mover that cannot reach its space gives up */
#define HUMAN_ROLL_WAIT 600

enum SpaceKind { SPACE_BLUE, SPACE_RED };

enum Phase {
    PH_GATHER,   /* everyone walks to their space */
    PH_INTRO,    /* "P1's turn" */
    PH_ROLL,     /* the die counts until A */
    PH_SHOW,     /* the number holds a moment */
    PH_WALK,
    PH_LAND,     /* the space's effect shows */
    PH_DONE,     /* everyone moved: the match ends shortly */
};

static const u8 layout[SPACES] = {
    SPACE_BLUE, SPACE_BLUE, SPACE_RED, SPACE_BLUE, SPACE_BLUE, SPACE_BLUE,
    SPACE_RED,  SPACE_BLUE, SPACE_BLUE, SPACE_RED, SPACE_BLUE, SPACE_BLUE,
};

static struct {
    int phase;
    int timer;
    int frame;
    int die;
    int steps;       /* spaces left to walk */
    int target;      /* the space being walked to */
    int stall;
    HSD_Text* text;
    int line_turn, line_msg, line_big;
    int line_p[PARTY_PLAYERS];
    int line_s[PARTY_PLAYERS];
    int cpu_wait;    /* frames a CPU lets the die spin */
    CmSubject* view; /* what the camera frames besides the fighters it is allowed */
} bd;

static float space_x(int space)
{
    return SPACE_X0 + SPACE_STEP * (float) space;
}

/* Players sharing a space stand a little apart. */
static float stand_x(int slot, int space)
{
    return space_x(space) + ((float) slot - 1.5f) * 2.0f;
}

static Fighter* fighter(int slot)
{
    HSD_GObj* gobj = Player_GetEntity(slot);
    return gobj != NULL ? GET_FIGHTER(gobj) : NULL;
}

void board_reset(void)
{
    party.turn = 0;
    party.mover = 0;
    party.star_space = 4 + party_rand(SPACES - 4);
    memset(bd.line_p, 0, sizeof bd.line_p);
}

/* ---- drawing ---- */

static void draw_board(void)
{
    static const GXColor blue = { 60, 110, 255, 255 };
    static const GXColor red = { 230, 50, 50, 255 };
    static const GXColor gold = { 255, 200, 40, 255 };
    static const GXColor rim = { 250, 250, 250, 255 };
    int i;
    for (i = 0; i < SPACES; i++) {
        float x = space_x(i);
        party_draw_disc(rim, x, FLOOR_Y + 0.05f, 4.6f, 4.6f);
        party_draw_disc(layout[i] == SPACE_RED ? red : blue, x, FLOOR_Y + 0.1f, 4.0f, 4.0f);
        if (i == party.star_space) {
            party_draw_disc(gold, x, FLOOR_Y + 0.15f, 2.4f, 2.4f);
            party_draw_quad(gold, x - 0.4f, FLOOR_Y, x + 0.4f, FLOOR_Y + 18.0f, 0.0f);
            party_draw_quad(gold, x - 3.0f, FLOOR_Y + 18.0f, x + 3.0f, FLOOR_Y + 22.0f, 0.0f);
        }
    }
}

/* ---- text ---- */

static void show_players(void)
{
    int i;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        party_hud_set(bd.text, bd.line_s[i], "%d STAR  %d COIN", party.p[i].stars, party.p[i].coins);
    }
}

static void say(const char* fmt, int a, int b)
{
    party_hud_set(bd.text, bd.line_msg, fmt, a, b);
}

/* ---- turn logic ---- */

static int human_roll_wait(void)
{
    return party_env_int("MELEE_PARTY_AUTO_ROLL", 0) ? 30 : HUMAN_ROLL_WAIT;
}

static int mover_human(void)
{
    return party.p[party.mover].slot_type == Gm_PKind_Human;
}

static int a_pressed(int slot)
{
    Fighter* fp = fighter(slot);
    int port = fp != NULL ? fp->pad_port : slot;
    return (HSD_PadGameStatus[port & 3].trigger & HSD_PAD_A) != 0;
}

static void start_mover(void)
{
    bd.phase = PH_INTRO;
    bd.timer = 70;
    party_hud_set(bd.text, bd.line_big, "");
    say("P%d's turn!", party.mover + 1, 0);
    party_hud_color(bd.text, bd.line_msg, PARTY_GOLD);
}

static void land(void)
{
    PartyPlayer* pp = &party.p[party.mover];
    int space = pp->space;
    if (layout[space] == SPACE_RED) {
        int lose = pp->coins < 3 ? pp->coins : 3;
        pp->coins = (s16) (pp->coins - lose);
        say("Red space: -%d coins", lose, 0);
        party_hud_color(bd.text, bd.line_msg, PARTY_RED);
    } else {
        pp->coins = (s16) (pp->coins + 3);
        say("Blue space: +3 coins", 0, 0);
        party_hud_color(bd.text, bd.line_msg, PARTY_BLUE);
    }
    party_log("board: P%d lands on space %d (%s), coins %d", party.mover + 1, space,
              layout[space] == SPACE_RED ? "red" : "blue", pp->coins);
    show_players();
    bd.phase = PH_LAND;
    bd.timer = 70;
}

/* The mover reached bd.target. */
static void step_done(void)
{
    PartyPlayer* pp = &party.p[party.mover];
    pp->space = (s16) bd.target;
    bd.steps--;
    party_hud_set(bd.text, bd.line_big, bd.steps > 0 ? "%d" : "", bd.steps);

    if (pp->space == party.star_space) {
        if (pp->coins >= STAR_COST) {
            int next;
            pp->coins = (s16) (pp->coins - STAR_COST);
            pp->stars++;
            do {
                next = party_rand(SPACES);
            } while (next == party.star_space);
            party.star_space = next;
            say("P%d got a STAR!", party.mover + 1, 0);
            party_hud_color(bd.text, bd.line_msg, PARTY_GOLD);
            party_log("board: P%d buys a star (%d), the star moves to %d", party.mover + 1,
                      pp->stars, next);
            show_players();
        } else {
            say("A star costs %d coins...", STAR_COST, 0);
            party_hud_color(bd.text, bd.line_msg, PARTY_WHITE);
        }
    }
    if (bd.steps <= 0) {
        land();
        return;
    }
    bd.target = (bd.target + 1) % SPACES;
    bd.stall = 0;
}

static void next_mover(void)
{
    party.mover++;
    if (party.mover >= PARTY_PLAYERS) {
        party.mover = 0;
        bd.phase = PH_DONE;
        bd.timer = 60;
        say("Minigame time!", 0, 0);
        party_hud_color(bd.text, bd.line_msg, PARTY_GOLD);
        return;
    }
    start_mover();
}

static int everyone_home(void)
{
    int i;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        Fighter* fp = fighter(i);
        if (fp != NULL && fabsf(fp->cur_pos.x - stand_x(i, party.p[i].space)) > ARRIVE) {
            return 0;
        }
    }
    return 1;
}

static void board_start(void)
{
    int i;
    ifAll_802F3394();   /* no damage percents or stocks on the board */
    party_hud_init();
    bd.text = party_hud_text();
    bd.line_turn = party_hud_line(bd.text, 0.0f, -200.0f, 0.7f, PARTY_WHITE);
    bd.line_msg = party_hud_line(bd.text, 0.0f, -150.0f, 0.75f, PARTY_GOLD);
    bd.line_big = party_hud_line(bd.text, 0.0f, -95.0f, 1.6f, PARTY_WHITE);
    for (i = 0; i < PARTY_PLAYERS; i++) {
        static const GXColor port[PARTY_PLAYERS] = {
            { 255, 80, 80, 255 }, { 90, 150, 255, 255 }, { 255, 210, 60, 255 }, { 80, 220, 110, 255 },
        };
        float x = -225.0f + 150.0f * (float) i;
        bd.line_p[i] = party_hud_line(bd.text, x, 185.0f, 0.55f, port[i]);
        bd.line_s[i] = party_hud_line(bd.text, x, 210.0f, 0.36f, PARTY_WHITE);
        party_hud_set(bd.text, bd.line_p[i], "P%d", i + 1);
    }
    party_hud_set(bd.text, bd.line_turn, "TURN %d OF %d", party.turn, party.max_turns);
    /* The match camera follows the fighters; a subject the width of the board keeps every
     * space in view as well (TM-CE's Eggs event frames its egg the same way). */
    bd.view = Camera_80029020();
    if (bd.view != NULL) {
        bd.view->state = CmSubjectState_Active;
        bd.view->pos.y = FLOOR_Y + 8.0f;
    }
    show_players();
    party_draw_init(draw_board);
}

/* While a player moves the camera frames only them and the spaces around them, Mario Party
 * style; otherwise the whole board. The other fighters leave the framing the way Home-Run Contest
 * takes the sandbag out of it (force_inactive). */
static void board_camera(void)
{
    int focus = bd.phase >= PH_INTRO && bd.phase <= PH_LAND ? party.mover : -1;
    CmSubjectExtents wide = { { -74.0f, 74.0f }, { 30.0f, -4.0f, 0.0f } };
    CmSubjectExtents near = { { -34.0f, 34.0f }, { 26.0f, -4.0f, 0.0f } };
    int i;

    for (i = 0; i < PARTY_PLAYERS; i++) {
        Fighter* fp = fighter(i);
        if (fp != NULL && fp->x890_cameraBox != NULL) {
            fp->x890_cameraBox->force_inactive = focus >= 0 && i != focus;
        }
    }
    if (bd.view == NULL) {
        return;
    }
    if (focus >= 0 && fighter(focus) != NULL) {
        float x = fighter(focus)->cur_pos.x;
        if (bd.phase == PH_WALK) {
            x = (x + stand_x(focus, bd.target)) * 0.5f;   /* lead toward the next space */
        }
        bd.view->pos.x = x;
        bd.view->target_ext = near;
    } else {
        bd.view->pos.x = 0.0f;
        bd.view->target_ext = wide;
    }
}

static void board_frame(void)
{
    bd.frame++;
    board_camera();
    switch (bd.phase) {
    case PH_GATHER:
        if (bd.frame > 90 && (everyone_home() || bd.frame > 90 + STALL_FRAMES)) {
            party.mover = 0;
            start_mover();
        }
        break;
    case PH_INTRO:
        if (--bd.timer <= 0) {
            bd.phase = PH_ROLL;
            bd.timer = 0;
            bd.cpu_wait = 40 + party_rand(30);
            say(mover_human() ? "Press A to roll!" : "", 0, 0);
            party_hud_color(bd.text, bd.line_msg, PARTY_WHITE);
        }
        break;
    case PH_ROLL:
        bd.timer++;
        bd.die = (bd.timer / 3) % DIE_MAX + 1;
        party_hud_set(bd.text, bd.line_big, "%d", bd.die);
        /* A human who does not press A rolls after HUMAN_ROLL_WAIT (an idle player online must
         * not stop the party; frame-counted, so both sides roll on the same frame). */
        if (mover_human() ? a_pressed(party.mover) || bd.timer >= human_roll_wait()
                          : bd.timer >= bd.cpu_wait) {
            if (!mover_human()) {
                bd.die = party_rand(DIE_MAX) + 1;   /* a human gets the number they stopped on */
            }
            party_hud_set(bd.text, bd.line_big, "%d", bd.die);
            party_hud_color(bd.text, bd.line_big, PARTY_GOLD);
            party_log("board: P%d rolls %d", party.mover + 1, bd.die);
            bd.phase = PH_SHOW;
            bd.timer = 45;
        }
        break;
    case PH_SHOW:
        if (--bd.timer <= 0) {
            party_hud_color(bd.text, bd.line_big, PARTY_WHITE);
            say("", 0, 0);
            bd.steps = bd.die;
            bd.target = (party.p[party.mover].space + 1) % SPACES;
            bd.stall = 0;
            bd.phase = PH_WALK;
        }
        break;
    case PH_WALK: {
        Fighter* fp = fighter(party.mover);
        if (fp == NULL || ++bd.stall > STALL_FRAMES) {
            party_log("board: P%d could not reach space %d", party.mover + 1, bd.target);
            step_done();
            break;
        }
        if (fabsf(fp->cur_pos.x - stand_x(party.mover, bd.target)) <= ARRIVE) {
            step_done();
        }
        break;
    }
    case PH_LAND:
        if (--bd.timer <= 0) {
            next_mover();
        }
        break;
    case PH_DONE:
        if (--bd.timer == 0) {
            gm_8016B328();
        }
        break;
    }
}

void board_setup(StartMeleeData* start)
{
    party.turn++;
    party.mover = 0;
    memset(&bd, 0, sizeof bd);
    bd.phase = PH_GATHER;
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.disable_pausing = true;
    start->rules.x30 = 0.0f;   /* no damage, should anyone be hit */
    start->rules.on_match_start = board_start;
    start->rules.on_frame_start = board_frame;
    party_fill_players(start);
    party_log("turn %d/%d: board, star on space %d", party.turn, party.max_turns, party.star_space);
}

/* Walk toward x: run while far, walk while near, nothing once there. */
static void steer(Fighter* fp, float x)
{
    float dx = x - fp->cur_pos.x;
    float ax = fabsf(dx);
    float s = ax <= ARRIVE * 0.6f ? 0.0f : ax < 14.0f ? 0.55f : 1.0f;
    fp->input.lstick[0].x = dx < 0.0f ? -s : s;
}

void board_fighter_input(struct Fighter* fp)
{
    int slot = fp->player_idx;

    fp->input.lstick[0].x = 0.0f;
    fp->input.lstick[0].y = 0.0f;
    fp->input.cstick[0].x = 0.0f;
    fp->input.cstick[0].y = 0.0f;
    fp->input.triggers[0] = 0.0f;
    fp->input.held_buttons[0] = 0;
    if (slot < 0 || slot >= PARTY_PLAYERS) {
        return;
    }
    if (bd.phase == PH_GATHER) {
        steer(fp, stand_x(slot, party.p[slot].space));
    } else if (bd.phase == PH_WALK && slot == party.mover) {
        steer(fp, stand_x(slot, bd.target));
    }
}

int board_turn_done(void)
{
    return bd.phase == PH_DONE;
}
