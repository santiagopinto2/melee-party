/* Melee Party: the board. One board turn is one VS match on Final Destination in which every
 * player moves once, in order.
 *
 * The board is a circle of spaces as wide as the stage's top, drawn as discs on a round floor of
 * its own (party_draw.c; Final Destination is far wider than it is deep, and the fighters can stand
 * anywhere in depth) and seen from above by a camera that looks down on it, Mario Party style. The
 * fighters are real but Melee only moves them along its 2D line, so the board keeps each one's
 * place on the floor (x and depth z) itself:
 * - mu_party_fighter_input writes their inputs: holding the stick toward their facing side makes
 *   them walk or run with their own animations and speed, and nobody can attack.
 * - mu_party_fighter_map (before the fighter's collision) moves that place by the distance the
 *   fighter's own ground speed covered, along the direction the board chose, and puts the fighter
 *   there, turned to face that way.
 *
 * Per mover: a die counts 1..10 until A (a CPU presses it after a moment), then the mover walks
 * that many spaces around the ring. The board walks every mover: the only thing a player does on
 * the board is press A to stop the die; every other input is dropped.
 * Passing the star buys it for STAR_COST coins, and the space the mover ends on pays or takes
 * coins. */
#include <math.h>
#include <string.h>

#include <melee/cm/camera.h>
#include <melee/cm/types.h>
#include <melee/ft/ftparts.h>
#include <melee/ft/inlines.h>
#include <melee/ft/types.h>
#include <melee/gm/gmvs.h>
#include <melee/gr/forward.h>
#include <melee/if/ifall.h>
#include <melee/pl/player.h>
#include <sysdolphin/baselib/cobj.h>
#include <sysdolphin/baselib/controller.h>
#include <sysdolphin/baselib/gobj.h>

#include "party.h"
#include "party_draw.h"
#include "party_hud.h"

#define SPACES 18
#define RING_R 76.0f     /* the circle of spaces: Final Destination's edges are at x +-85.6 */
#define RING_RX RING_R
#define RING_RZ RING_R
#define BOARD_R 87.0f    /* the board's own floor, around the circle */
#define BOUND_X 80.0f    /* where a walking player can go */
#define BOUND_Z 84.0f
#define STAR_Y 15.0f     /* the star over the star space */
#define STAR_R 5.0f
#define SPACE_R 4.0f
#define FLOOR_Y 0.0f
#define STAR_COST 20
#define DIE_MAX 10
#define ARRIVE 1.5f      /* close enough to where the board walks a fighter */
#define STALL_FRAMES 600 /* a mover that cannot reach its space gives up */
#define HUMAN_ROLL_WAIT 600
#define TURN_RATE 0.3f   /* radians a fighter turns per frame */

extern Camera game_camera;

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
    SPACE_BLUE, SPACE_BLUE, SPACE_BLUE, SPACE_RED,  SPACE_BLUE, SPACE_BLUE,
    SPACE_BLUE, SPACE_RED,  SPACE_BLUE, SPACE_BLUE, SPACE_BLUE, SPACE_BLUE,
    SPACE_RED,  SPACE_BLUE, SPACE_BLUE, SPACE_BLUE, SPACE_RED,  SPACE_BLUE,
};

/* A fighter's place on the board's floor and the way it faces (0 = toward the camera). */
typedef struct Walker {
    float x, z;
    float hx, hz;    /* the direction it walks */
    float yaw;
    int placed;
} Walker;

typedef struct Vec3f {
    float x, y, z;
} Vec3f;

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
    Walker w[PARTY_PLAYERS];
    Vec3f eye, look; /* the camera */
    int camera_set;
} bd;

static float ring_x[SPACES], ring_z[SPACES];
static int ring_ready;

/* Spaces at equal distances along the ellipse, starting at its front (nearest the camera) and
 * going right, the way the movers walk. */
static void ring_init(void)
{
    enum { N = 720 };
    float length[N + 1];
    float px = 0.0f, pz = RING_RZ;
    int i, k = 0;
    if (ring_ready) {
        return;
    }
    length[0] = 0.0f;
    for (i = 1; i <= N; i++) {
        float t = (float) i * (2.0f * (float) M_PI / N);
        float x = RING_RX * sinf(t), z = RING_RZ * cosf(t);
        length[i] = length[i - 1] + sqrtf((x - px) * (x - px) + (z - pz) * (z - pz));
        px = x;
        pz = z;
    }
    for (i = 0; i < SPACES; i++) {
        float want = length[N] * (float) i / SPACES;
        float t;
        while (k < N && length[k + 1] < want) {
            k++;
        }
        t = ((float) k + (want - length[k]) / (length[k + 1] - length[k])) *
            (2.0f * (float) M_PI / N);
        ring_x[i] = RING_RX * sinf(t);
        ring_z[i] = RING_RZ * cosf(t);
    }
    ring_ready = 1;
}

/* Players sharing a space stand a little apart. */
static void stand_at(int slot, int space, float* x, float* z)
{
    *x = ring_x[space] + ((float) slot - 1.5f) * 2.4f;
    *z = ring_z[space] + ((slot & 1) ? 1.2f : -1.2f);
}

static Fighter* fighter(int slot)
{
    HSD_GObj* gobj = Player_GetEntity(slot);
    return gobj != NULL ? GET_FIGHTER(gobj) : NULL;
}

static float dist(float x0, float z0, float x1, float z1)
{
    return sqrtf((x1 - x0) * (x1 - x0) + (z1 - z0) * (z1 - z0));
}

void board_reset(void)
{
    party.turn = 0;
    party.mover = 0;
    party.star_space = 4 + party_rand(SPACES - 4);
    memset(bd.line_p, 0, sizeof bd.line_p);
}

/* ---- drawing ---- */

static void draw_path(void)
{
    static const GXColor path = { 235, 215, 160, 255 };
    int i;
    for (i = 0; i < SPACES; i++) {
        int j = (i + 1) % SPACES;
        float dx = ring_x[j] - ring_x[i], dz = ring_z[j] - ring_z[i];
        float len = sqrtf(dx * dx + dz * dz);
        float nx = -dz / len * 1.1f, nz = dx / len * 1.1f;
        float c[8];
        c[0] = ring_x[i] - nx; c[1] = ring_z[i] - nz;
        c[2] = ring_x[j] - nx; c[3] = ring_z[j] - nz;
        c[4] = ring_x[j] + nx; c[5] = ring_z[j] + nz;
        c[6] = ring_x[i] + nx; c[7] = ring_z[i] + nz;
        party_draw_floor_quad(path, FLOOR_Y + 0.1f, c);
    }
}

static void draw_board(void)
{
    static const GXColor floor_rim = { 196, 170, 112, 255 };
    static const GXColor floor = { 70, 128, 82, 255 };
    static const GXColor blue = { 60, 110, 255, 255 };
    static const GXColor red = { 230, 50, 50, 255 };
    static const GXColor gold = { 255, 200, 40, 255 };
    static const GXColor rim = { 250, 250, 250, 255 };
    int i;
    party_draw_disc_n(floor_rim, 0.0f, FLOOR_Y + 0.04f, 0.0f, BOARD_R + 2.0f, 96);
    party_draw_disc_n(floor, 0.0f, FLOOR_Y + 0.06f, 0.0f, BOARD_R, 96);
    draw_path();
    for (i = 0; i < SPACES; i++) {
        float x = ring_x[i], z = ring_z[i];
        /* The space the mover walks to blinks gold. */
        int next = bd.phase == PH_WALK && i == bd.target && (bd.frame / 8) % 2 == 0;
        party_draw_disc(next ? gold : rim, x, FLOOR_Y + 0.2f, z, SPACE_R + 0.7f, SPACE_R + 0.7f);
        party_draw_disc(layout[i] == SPACE_RED ? red : blue, x, FLOOR_Y + 0.3f, z, SPACE_R,
                        SPACE_R);
        if (i == party.star_space) {
            /* A star floating over it, bobbing. */
            float bob = 1.2f * sinf((float) bd.frame * 0.06f);
            party_draw_disc(gold, x, FLOOR_Y + 0.4f, z, 2.4f, 2.4f);
            party_draw_star(gold, x, FLOOR_Y + STAR_Y + bob, z, STAR_R);
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
    party_log("board: P%d reaches space %d, %d to go", party.mover + 1, bd.target, bd.steps);
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
        float x, z;
        stand_at(i, party.p[i].space, &x, &z);
        if (fighter(i) != NULL && dist(bd.w[i].x, bd.w[i].z, x, z) > ARRIVE * 2.0f) {
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
    show_players();
    party_draw_init(draw_board);
}

/* ---- camera ---- */

/* The match camera's debug free mode has no update of its own: the board sets where it looks
 * from and at every frame. High and looking down on the whole board, or nearer the mover while
 * they move. */
static void board_camera(void)
{
    int focus = bd.phase >= PH_INTRO && bd.phase <= PH_LAND ? party.mover : -1;
    Vec3f look = { 0.0f, 0.0f, 4.0f };
    float distance = 330.0f, pitch = 0.76f;   /* about 44 degrees down: the whole circle */
    float k = bd.camera_set ? 0.06f : 1.0f;
    Vec3f eye;
    HSD_CObj* cobj;

    if (focus >= 0 && bd.w[focus].placed) {
        look.x = bd.w[focus].x;
        look.z = bd.w[focus].z;
        if (bd.phase == PH_WALK) {
            look.x = (look.x + ring_x[bd.target]) * 0.5f;   /* lead toward the next space */
            look.z = (look.z + ring_z[bd.target]) * 0.5f;
        }
        look.y = 6.0f;
        distance = 140.0f;
        pitch = 0.80f;
    }
    eye.x = look.x;
    eye.y = look.y + distance * sinf(pitch);
    eye.z = look.z + distance * cosf(pitch);

    bd.look.x += (look.x - bd.look.x) * k;
    bd.look.y += (look.y - bd.look.y) * k;
    bd.look.z += (look.z - bd.look.z) * k;
    bd.eye.x += (eye.x - bd.eye.x) * k;
    bd.eye.y += (eye.y - bd.eye.y) * k;
    bd.eye.z += (eye.z - bd.eye.z) * k;
    bd.camera_set = 1;

    if (!Camera_80030178()) {
        Camera_8003006C();
    }
    cm_80453004.free_int_pos.x = bd.look.x;
    cm_80453004.free_int_pos.y = bd.look.y;
    cm_80453004.free_int_pos.z = bd.look.z;
    cm_80453004.free_eye_pos.x = bd.eye.x;
    cm_80453004.free_eye_pos.y = bd.eye.y;
    cm_80453004.free_eye_pos.z = bd.eye.z;
    cm_80453004.free_fov = 32.0f;
    cobj = game_camera.gobj != NULL ? game_camera.gobj->hsd_obj : NULL;
    if (cobj != NULL) {
        HSD_CObjSetNear(cobj, 1.0f);
        HSD_CObjSetFar(cobj, 16384.0f);
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
        Walker* w = &bd.w[party.mover];
        if (fighter(party.mover) == NULL || ++bd.stall > STALL_FRAMES) {
            party_log("board: P%d could not reach space %d", party.mover + 1, bd.target);
            step_done();
            break;
        }
        if (dist(w->x, w->z, ring_x[bd.target], ring_z[bd.target]) <= SPACE_R) {
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
    ring_init();
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

/* ---- the fighters ---- */

static float wrap_angle(float a)
{
    while (a > (float) M_PI) {
        a -= 2.0f * (float) M_PI;
    }
    while (a < -(float) M_PI) {
        a += 2.0f * (float) M_PI;
    }
    return a;
}

static void turn_toward(Walker* w, float yaw)
{
    float d = wrap_angle(yaw - w->yaw);
    if (d > TURN_RATE) {
        d = TURN_RATE;
    } else if (d < -TURN_RATE) {
        d = -TURN_RATE;
    }
    w->yaw = wrap_angle(w->yaw + d);
}

/* How hard to push toward (x, z): a full walk, slower for the last few units, nothing once there.
 * Sets the walking direction. */
static float steer(Walker* w, float x, float z)
{
    float dx = x - w->x, dz = z - w->z;
    float d = sqrtf(dx * dx + dz * dz);
    if (d <= ARRIVE) {
        return 0.0f;
    }
    w->hx = dx / d;
    w->hz = dz / d;
    return d < 4.0f ? 0.6f : 1.0f;
}

void board_fighter_input(struct Fighter* fp)
{
    int slot = fp->player_idx;
    float push = 0.0f;
    Walker* w;

    fp->input.lstick[0].x = 0.0f;
    fp->input.lstick[0].y = 0.0f;
    fp->input.cstick[0].x = 0.0f;
    fp->input.cstick[0].y = 0.0f;
    fp->input.triggers[0] = 0.0f;
    fp->input.held_buttons[0] = 0;
    /* Nana (the Ice Climbers' second climber) shares Popo's slot and place: she only follows
     * him (board_fighter_map). */
    if (slot < 0 || slot >= PARTY_PLAYERS || !bd.w[slot].placed || fp->is_sub_fighter) {
        return;
    }
    w = &bd.w[slot];

    /* Whatever the pad or the CPU asked for is dropped above: the board walks everyone. */
    if (bd.phase == PH_WALK && slot == party.mover) {
        push = steer(w, ring_x[bd.target], ring_z[bd.target]);
    } else {
        float x, z;
        stand_at(slot, party.p[slot].space, &x, &z);
        push = steer(w, x, z);
    }

    if (push > 0.0f) {
        turn_toward(w, atan2f(w->hx, w->hz));
        fp->input.lstick[0].x = push * fp->facing_dir;   /* forward, whichever way it faces */
    } else if (!(bd.phase == PH_WALK && slot == party.mover)) {
        turn_toward(w, 0.0f);   /* standing players face the camera */
    }
    /* A walk, never a dash or a run: the stick was where it is last frame too, so Melee never sees
     * it pushed hard all at once (the dash's smash window, ftCo_Dash_CheckInput). */
    fp->input.lstick[1] = fp->input.lstick[0];
}

/* Fighter_procMap, before collision: the fighter's own ground speed carries it along the board's
 * direction; it stands at its board place, turned that way. */
void board_fighter_map(struct Fighter* fp)
{
    int slot = fp->player_idx;
    Walker* w;
    if (slot < 0 || slot >= PARTY_PLAYERS) {
        return;
    }
    w = &bd.w[slot];
    if (fp->is_sub_fighter) {
        /* Nana: a step behind Popo, turned his way. His place is the board's, not hers. */
        if (w->placed) {
            fp->cur_pos.x = w->x - w->hx * 3.0f;
            fp->cur_pos.z = w->z - w->hz * 3.0f;
            ftPartSetRotY(fp, 0, w->yaw);
        }
        return;
    }
    if (!w->placed) {
        w->x = fp->cur_pos.x;
        w->z = 0.0f;
        w->hx = fp->facing_dir;
        w->hz = 0.0f;
        w->yaw = fp->facing_dir * (float) M_PI_2;
        w->placed = 1;
    }
    if (fp->ground_or_air == GA_Ground) {
        float v = fp->gr_vel * fp->facing_dir;   /* forward speed */
        w->x += w->hx * v;
        w->z += w->hz * v;
        w->x = w->x > BOUND_X ? BOUND_X : w->x < -BOUND_X ? -BOUND_X : w->x;
        w->z = w->z > BOUND_Z ? BOUND_Z : w->z < -BOUND_Z ? -BOUND_Z : w->z;
        fp->cur_pos.x = w->x;
    } else {
        w->x = fp->cur_pos.x;   /* the entry: it drops in from above */
    }
    fp->cur_pos.z = w->z;
    ftPartSetRotY(fp, 0, w->yaw);
}

int board_turn_done(void)
{
    return bd.phase == PH_DONE;
}
