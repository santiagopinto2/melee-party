/* Melee Party: the board. One board turn is one VS match on Final Destination in which every
 * player moves once, in order.
 *
 * The board is Mario Party 4's Goomba's Greedy Gala: its spaces, paths and branches as the game
 * lays them out (board_ggg.h, from the game's own data), seen the way MP4's board camera sees
 * it, drawn as discs on a floor of its own (party_draw.c; Final Destination is far wider than it
 * is deep, and the fighters can stand anywhere in depth) and seen from above by a camera that
 * looks down on it. The fighters are real but Melee only moves them along its 2D
 * line, so the board keeps each one's place on the floor (x and depth z) itself:
 * - mu_party_fighter_input writes their inputs: holding the stick toward their facing side makes
 *   them walk or run with their own animations and speed, and nobody can attack.
 * - mu_party_fighter_map (before the fighter's collision) moves that place by the distance the
 *   fighter's own ground speed covered, along the direction the board chose, and puts the fighter
 *   there, turned to face that way.
 *
 * Per mover: a die counts 1..10 until A (a CPU presses it after a moment), then the board walks
 * the mover that many spaces along the paths. Only the visible spaces count; the path's hidden
 * nodes (MP4 type 0) do not. Where a path splits the mover picks a way with the stick and A. The
 * path into the middle ends on the Goomba's roulette, which sends the mover out one of its four
 * ways at random. Passing the star buys it for STAR_COST coins without using a step, and the
 * space the mover ends on pays or takes coins.
 *
 * Not in play yet: the shops, the Boo house and the lottery are only arrows by the path, the
 * pipes (MP4: for Mini Mushroom players) cannot be taken, and Mushroom, Bowser, Happening,
 * Battle, Fortune and Warp spaces are plain blue, red or green spaces with no event. */
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
#include "board_ggg.h"

/* MP4 units to the board's, about the board's middle. MP4's board camera looks from +z like
 * Melee's, so x and z carry over as they are. The board's wide side (5700 MP4 units between its
 * outermost spaces) fits between Final Destination's edges (x +-85.6). */
#define MP4_MID_X 1.0f
#define MP4_MID_Z 451.0f
#define MP4_SCALE 0.02737f
#define FLOOR_X 82.0f    /* the board's own floor */
#define FLOOR_Z 88.0f    /* deeper than the spaces: it covers the stage */
#define BOUND_X 80.0f    /* where a walking player can go */
#define BOUND_Z 70.0f
#define STAR_Y 15.0f     /* the star over the star space */
#define STAR_R 4.0f
#define SPACE_R 2.9f
#define FLOOR_Y 0.0f
#define STAR_COST 20
#define DIE_MAX 10
#define ARRIVE 1.5f      /* close enough to where the board walks a fighter */
#define STALL_FRAMES 600 /* a mover that cannot reach its space gives up */
#define HUMAN_ROLL_WAIT 600
#define CPU_CHOICE_WAIT 40
#define SPIN_FRAMES 150  /* the roulette's spin */
#define SPIN_HOLD 50     /* and how long its result shows */
#define TURN_RATE 0.3f   /* radians a fighter turns per frame */

/* MP4's space flags (partyboard: src/game/board/space.c, src/REL/w02Dll). */
#define F_ROULETTE_IN 0x00000001u  /* the path's last node before the roulette */
#define F_ROULETTE    0x0000000Eu  /* 1: the roulette's platform; 2..5: its way out, by colour */
#define F_ROULETTE_MID 0x00000800u
#define F_SHOP        0x00180000u
#define F_OFF_PATH    0x02000000u  /* where a shop, the Boo house etc. stand: never walked */
#define F_STAR_HOST   0x04000000u
#define F_BOO         0x08000000u
#define F_LOTTERY     0x10000000u
#define F_PIPE        0x20000000u
#define F_START       0x80000000u
#define MP4_NODE 0
#define MP4_STAR 8

extern Camera game_camera;

enum SpaceKind { SPACE_NONE, SPACE_BLUE, SPACE_RED, SPACE_GREEN };

enum Phase {
    PH_GATHER,   /* everyone walks to their space */
    PH_INTRO,    /* "P1's turn" */
    PH_ROLL,     /* the die counts until A */
    PH_SHOW,     /* the number holds a moment */
    PH_WALK,
    PH_CHOOSE,   /* the path splits: the mover picks a way */
    PH_SPIN,     /* the roulette picks a way */
    PH_LAND,     /* the space's effect shows */
    PH_DONE,     /* everyone moved: the match ends shortly */
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
    int target;      /* the node being walked to */
    int stall;
    int choice[4];   /* PH_CHOOSE: the ways on, and the one picked */
    int nchoice, pick;
    int spin;        /* PH_SPIN: the roulette's result, 0..3 */
    HSD_Text* text;
    int line_turn, line_msg, line_big;
    int line_p[PARTY_PLAYERS];
    int line_s[PARTY_PLAYERS];
    int cpu_wait;    /* frames a CPU lets the die spin or thinks at a split */
    Walker w[PARTY_PLAYERS];
    Vec3f eye, look; /* the camera */
    int camera_set;
    int paused;      /* the match was paused last frame */
} bd;

/* The board, worked out once from board_ggg.h. */
static struct {
    int ready;
    float x[GGG_NODES], z[GGG_NODES];
    u8 on_path[GGG_NODES];   /* reachable from the start without a pipe */
    int start, roulette_in, platform, middle;
    int exit[4];             /* the roulette's ways out, by colour */
    int star[8];             /* the spaces the star can be on */
    int nstar;
} g;

/* The boards the Melee Party menu lists, in its order. Only one so far: board.c plays it. */
static const struct {
    const char* name;
    const char* description;   /* two lines under the menu */
} boards[] = {
    { "Goomba's Greedy Gala", "From Mario Party 4. A wheel\nin the middle picks your way." },
};

int board_count(void)
{
    return (int) (sizeof boards / sizeof boards[0]);
}

const char* board_name(int index)
{
    return boards[index >= 0 && index < board_count() ? index : 0].name;
}

const char* board_description(int index)
{
    return boards[index >= 0 && index < board_count() ? index : 0].description;
}

static const GXColor wheel_color[4] = {
    { 230, 60, 60, 255 }, { 60, 110, 255, 255 }, { 250, 210, 40, 255 }, { 170, 80, 230, 255 },
};
static const char* const wheel_name[4] = { "RED", "BLUE", "YELLOW", "PURPLE" };

static int kind(int n)
{
    switch (ggg_nodes[n].type) {
    case 1: case 4: case 8: return SPACE_BLUE;   /* blue, Mushroom, a star space without the star */
    case 2: case 3: return SPACE_RED;            /* red, Bowser */
    case 5: case 6: case 7: case 9: return SPACE_GREEN;   /* Battle, Happening, Fortune, Warp */
    default: return SPACE_NONE;
    }
}

/* Whether a step onto n counts: the visible spaces do, except the one the star is on. */
static int counts(int n)
{
    return kind(n) != SPACE_NONE && n != party.star_space;
}

/* Whether a mover may walk from a node to n. */
static int walkable(int n)
{
    return !(ggg_nodes[n].flag & (F_OFF_PATH | F_STAR_HOST | F_PIPE));
}

/* The ways on from n. */
static int ways(int n, int out[4])
{
    int i, count = 0;
    for (i = 0; i < ggg_nodes[n].nlink; i++) {
        if (walkable(ggg_nodes[n].link[i])) {
            out[count++] = ggg_nodes[n].link[i];
        }
    }
    return count;
}

/* Walking distance (in nodes) from every node to `to`, the roulette's ways included. */
static void distances(int to, int out[GGG_NODES])
{
    int queue[GGG_NODES], head = 0, tail = 0, i, j;
    for (i = 0; i < GGG_NODES; i++) {
        out[i] = 9999;
    }
    out[to] = 0;
    queue[tail++] = to;
    while (head < tail) {
        int n = queue[head++];
        for (i = 0; i < GGG_NODES; i++) {
            int w[4], k = ways(i, w);
            if (i == g.platform) {
                for (j = 0; j < 4; j++) {
                    w[j] = g.exit[j];
                }
                k = 4;
            }
            for (j = 0; j < k; j++) {
                if (w[j] == n && out[i] > out[n] + 1) {
                    out[i] = out[n] + 1;
                    queue[tail++] = i;
                }
            }
        }
    }
}

static void board_init(void)
{
    int i, j, queue[GGG_NODES], head = 0, tail = 0;
    if (g.ready) {
        return;
    }
    g.start = g.roulette_in = g.platform = g.middle = -1;
    for (i = 0; i < GGG_NODES; i++) {
        const GggNode* n = &ggg_nodes[i];
        g.x[i] = ((float) n->x - MP4_MID_X) * MP4_SCALE;
        g.z[i] = ((float) n->z - MP4_MID_Z) * MP4_SCALE;
        if (n->flag & F_START) {
            g.start = i;
        }
        if (n->flag & F_ROULETTE_IN) {
            g.roulette_in = i;
        }
        if (n->type == MP4_NODE && ((n->flag & F_ROULETTE) >> 1) == 1) {
            g.platform = i;
        }
        if (n->flag & F_ROULETTE_MID) {
            g.middle = i;
            for (j = 0; j < n->nlink && j < 4; j++) {
                int out = n->link[j], colour = (int) ((ggg_nodes[out].flag & F_ROULETTE) >> 1) - 2;
                g.exit[colour & 3] = out;
            }
        }
        if (n->type == MP4_STAR && g.nstar < 8) {
            g.star[g.nstar++] = i;
        }
    }
    /* Everything a mover can reach from the start; the rest is behind the pipes. */
    g.on_path[g.start] = 1;
    queue[tail++] = g.start;
    while (head < tail) {
        int n = queue[head++], w[4], k = ways(n, w);
        if (n == g.platform) {
            for (k = 0; k < 4; k++) {
                w[k] = g.exit[k];
            }
        }
        for (j = 0; j < k; j++) {
            if (!g.on_path[w[j]]) {
                g.on_path[w[j]] = 1;
                queue[tail++] = w[j];
            }
        }
    }
    g.ready = 1;
}

/* Players sharing a space stand a little apart. */
static void stand_at(int slot, int space, float* x, float* z)
{
    *x = g.x[space] + ((float) slot - 1.5f) * 1.5f;
    *z = g.z[space] + ((slot & 1) ? 0.8f : -0.8f);
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

static void move_star(void)
{
    int next;
    do {
        next = g.star[party_rand(g.nstar)];
    } while (next == party.star_space && g.nstar > 1);
    party.star_space = next;
}

void board_reset(void)
{
    int i;
    board_init();
    party.turn = 0;
    party.mover = 0;
    party.star_space = -1;
    move_star();
    for (i = 0; i < PARTY_PLAYERS; i++) {
        party.p[i].space = (s16) g.start;
    }
    memset(bd.line_p, 0, sizeof bd.line_p);
}

/* ---- drawing ---- */

static float angle_to(int from, int to)
{
    return atan2f(g.z[to] - g.z[from], g.x[to] - g.x[from]);
}

static void draw_link(GXColor color, int a, int b, float half)
{
    float dx = g.x[b] - g.x[a], dz = g.z[b] - g.z[a];
    float len = sqrtf(dx * dx + dz * dz);
    float nx = -dz / len * half, nz = dx / len * half;
    float c[8];
    c[0] = g.x[a] - nx; c[1] = g.z[a] - nz;
    c[2] = g.x[b] - nx; c[3] = g.z[b] - nz;
    c[4] = g.x[b] + nx; c[5] = g.z[b] + nz;
    c[6] = g.x[a] + nx; c[7] = g.z[a] + nz;
    party_draw_floor_quad(color, FLOOR_Y + 0.1f, c);
}

static void draw_paths(void)
{
    static const GXColor path = { 235, 215, 160, 255 };
    static const GXColor pipe_path = { 120, 150, 110, 255 };
    int i, j;
    for (i = 0; i < GGG_NODES; i++) {
        const GggNode* n = &ggg_nodes[i];
        if (n->flag & (F_OFF_PATH | F_STAR_HOST)) {
            continue;
        }
        for (j = 0; j < n->nlink; j++) {
            int to = n->link[j];
            if (ggg_nodes[to].flag & (F_OFF_PATH | F_STAR_HOST)) {
                continue;
            }
            draw_link(g.on_path[i] && g.on_path[to] ? path : pipe_path, i, to, 0.6f);
        }
    }
}

/* The four-colour wheel in the middle, and its pointer. */
static void draw_roulette(void)
{
    static const GXColor rim = { 250, 250, 250, 255 };
    static const GXColor grey = { 150, 150, 160, 255 };
    static const GXColor dark = { 40, 40, 50, 255 };
    float mx = g.x[g.middle], mz = g.z[g.middle], a[4], pointer;
    int i;
    for (i = 0; i < 4; i++) {
        a[i] = angle_to(g.middle, g.exit[i]);
    }
    party_draw_disc(grey, g.x[g.platform], FLOOR_Y + 0.2f, g.z[g.platform], SPACE_R + 0.5f,
                    SPACE_R + 0.5f);
    party_draw_disc(dark, g.x[g.platform], FLOOR_Y + 0.3f, g.z[g.platform], SPACE_R, SPACE_R);
    party_draw_disc_n(rim, mx, FLOOR_Y + 0.2f, mz, 7.0f, 48);
    for (i = 0; i < 4; i++) {
        /* Each colour faces its way out, reaching halfway to the colours beside it. */
        float lo = 0.0f, hi = 0.0f;
        float best_lo = 9.0f, best_hi = 9.0f;
        int k;
        for (k = 0; k < 4; k++) {
            float d = a[k] - a[i];
            while (d <= 0.0f) {
                d += 2.0f * (float) M_PI;
            }
            if (k != i && d < best_hi) {
                best_hi = d;
            }
            d = 2.0f * (float) M_PI - d;
            if (k != i && d < best_lo) {
                best_lo = d;
            }
        }
        lo = a[i] - best_lo * 0.5f;
        hi = a[i] + best_hi * 0.5f;
        party_draw_floor_sector(wheel_color[i], mx, FLOOR_Y + 0.3f, mz, 6.3f, lo, hi);
        /* The way out the colour leads to. */
        party_draw_disc(wheel_color[i], g.x[g.exit[i]], FLOOR_Y + 0.3f, g.z[g.exit[i]], 1.6f, 1.6f);
    }
    if (bd.phase == PH_SPIN) {
        /* Fast at first, slowing onto the result. */
        float u = 1.0f - (float) (bd.timer > SPIN_HOLD ? bd.timer - SPIN_HOLD : 0) / SPIN_FRAMES;
        float left = 1.0f - u;
        pointer = a[bd.spin] - 6.0f * 2.0f * (float) M_PI * left * left * left;
    } else {
        pointer = -(float) M_PI_2 + 0.4f * sinf((float) bd.frame * 0.03f);
    }
    party_draw_disc(dark, mx, FLOOR_Y + 0.4f, mz, 1.3f, 1.3f);
    party_draw_floor_arrow(rim, FLOOR_Y + 0.5f, mx, mz, cosf(pointer), sinf(pointer), 5.8f, 1.0f);
}

/* Shops, the Boo house and the lottery: an arrow from the path toward where they would stand. */
static void draw_markers(void)
{
    static const GXColor shop = { 255, 220, 60, 255 };
    static const GXColor boo = { 180, 110, 240, 255 };
    static const GXColor lottery = { 255, 140, 40, 255 };
    static const GXColor pipe_rim = { 20, 110, 40, 255 };
    static const GXColor pipe_in = { 60, 190, 80, 255 };
    static const GXColor pipe_hole = { 10, 30, 15, 255 };
    int i, j;
    for (i = 0; i < GGG_NODES; i++) {
        const GggNode* n = &ggg_nodes[i];
        const GXColor* c = (n->flag & F_SHOP) ? &shop : (n->flag & F_BOO) ? &boo
                         : (n->flag & F_LOTTERY) ? &lottery : NULL;
        if (n->flag & F_PIPE) {
            party_draw_disc(pipe_rim, g.x[i], FLOOR_Y + 0.2f, g.z[i], 3.5f, 3.5f);
            party_draw_disc(pipe_in, g.x[i], FLOOR_Y + 0.3f, g.z[i], 2.7f, 2.7f);
            party_draw_disc(pipe_hole, g.x[i], FLOOR_Y + 0.4f, g.z[i], 1.9f, 1.9f);
            continue;
        }
        if (c == NULL || n->type != MP4_NODE) {
            continue;
        }
        for (j = 0; j < n->nlink; j++) {
            int to = n->link[j];
            if (ggg_nodes[to].flag & F_OFF_PATH) {
                float a = angle_to(i, to);
                party_draw_floor_arrow(*c, FLOOR_Y + 0.25f, g.x[i] + cosf(a) * 1.2f,
                                       g.z[i] + sinf(a) * 1.2f, cosf(a), sinf(a), 5.4f, 1.2f);
            }
        }
    }
}

static void draw_board(void)
{
    static const GXColor floor_rim = { 196, 170, 112, 255 };
    static const GXColor floor = { 70, 128, 82, 255 };
    static const GXColor blue = { 60, 110, 255, 255 };
    static const GXColor red = { 230, 50, 50, 255 };
    static const GXColor green = { 60, 200, 90, 255 };
    static const GXColor gold = { 255, 200, 40, 255 };
    static const GXColor rim = { 250, 250, 250, 255 };
    float c[8];
    int i;

    c[0] = -FLOOR_X - 2.0f; c[1] = -FLOOR_Z - 2.0f;
    c[2] = FLOOR_X + 2.0f;  c[3] = -FLOOR_Z - 2.0f;
    c[4] = FLOOR_X + 2.0f;  c[5] = FLOOR_Z + 2.0f;
    c[6] = -FLOOR_X - 2.0f; c[7] = FLOOR_Z + 2.0f;
    party_draw_floor_quad(floor_rim, FLOOR_Y + 0.04f, c);
    c[0] = -FLOOR_X; c[1] = -FLOOR_Z;
    c[2] = FLOOR_X;  c[3] = -FLOOR_Z;
    c[4] = FLOOR_X;  c[5] = FLOOR_Z;
    c[6] = -FLOOR_X; c[7] = FLOOR_Z;
    party_draw_floor_quad(floor, FLOOR_Y + 0.06f, c);
    draw_paths();
    draw_markers();
    draw_roulette();
    for (i = 0; i < GGG_NODES; i++) {
        int k = kind(i), next;
        float x = g.x[i], z = g.z[i];
        if (k == SPACE_NONE) {
            continue;
        }
        /* The space the mover walks to blinks gold. */
        next = bd.phase == PH_WALK && i == bd.target && (bd.frame / 8) % 2 == 0;
        party_draw_disc(next ? gold : rim, x, FLOOR_Y + 0.2f, z, SPACE_R + 0.5f, SPACE_R + 0.5f);
        party_draw_disc(k == SPACE_RED ? red : k == SPACE_GREEN ? green : blue, x, FLOOR_Y + 0.3f, z,
                        SPACE_R, SPACE_R);
        if (i == party.star_space) {
            /* A star floating over it, bobbing. */
            float bob = 1.2f * sinf((float) bd.frame * 0.06f);
            party_draw_disc(gold, x, FLOOR_Y + 0.4f, z, 1.8f, 1.8f);
            party_draw_star(gold, x, FLOOR_Y + STAR_Y + bob, z, STAR_R);
        }
    }
    if (bd.phase == PH_CHOOSE) {
        int at = party.p[party.mover].space;
        for (i = 0; i < bd.nchoice; i++) {
            float a = angle_to(at, bd.choice[i]);
            party_draw_floor_arrow(i == bd.pick ? gold : rim, FLOOR_Y + 0.5f, g.x[at] + cosf(a) * 3.2f,
                                   g.z[at] + sinf(a) * 3.2f, cosf(a), sinf(a), 4.8f, 1.4f);
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

static HSD_PadStatus* pad(int slot)
{
    Fighter* fp = fighter(slot);
    int port = fp != NULL ? fp->pad_port : slot;
    return &HSD_PadGameStatus[port & 3];
}

static int a_pressed(int slot)
{
    return (pad(slot)->trigger & HSD_PAD_A) != 0;
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
    int space = pp->space, k = kind(space);
    if (k == SPACE_RED) {
        int lose = pp->coins < 3 ? pp->coins : 3;
        pp->coins = (s16) (pp->coins - lose);
        say("Red space: -%d coins", lose, 0);
        party_hud_color(bd.text, bd.line_msg, PARTY_RED);
    } else if (k == SPACE_GREEN) {
        say("Green space", 0, 0);
        party_hud_color(bd.text, bd.line_msg, PARTY_WHITE);
    } else {
        pp->coins = (s16) (pp->coins + 3);
        say("Blue space: +3 coins", 0, 0);
        party_hud_color(bd.text, bd.line_msg, PARTY_BLUE);
    }
    party_log("board: P%d lands on space %d (%s), coins %d", party.mover + 1, space,
              k == SPACE_RED ? "red" : k == SPACE_GREEN ? "green" : "blue", pp->coins);
    show_players();
    bd.phase = PH_LAND;
    bd.timer = 70;
}

/* Walk on from the node the mover stands on: the one way on, or a choice where the path splits. */
static void walk_on(void)
{
    int at = party.p[party.mover].space;
    bd.nchoice = ways(at, bd.choice);
    bd.stall = 0;
    if (bd.nchoice > 1) {
        int d[GGG_NODES], i;
        bd.phase = PH_CHOOSE;
        bd.timer = 0;
        bd.pick = 0;
        bd.cpu_wait = CPU_CHOICE_WAIT;
        if (!mover_human()) {
            /* A CPU heads for the star. */
            distances(party.star_space, d);
            for (i = 1; i < bd.nchoice; i++) {
                if (d[bd.choice[i]] < d[bd.choice[bd.pick]]) {
                    bd.pick = i;
                }
            }
        }
        say(mover_human() ? "Choose a path! (stick, then A)" : "", 0, 0);
        party_hud_color(bd.text, bd.line_msg, PARTY_WHITE);
        return;
    }
    bd.target = bd.nchoice == 1 ? bd.choice[0] : ggg_nodes[at].link[0];
    bd.phase = PH_WALK;
}

/* The mover reached bd.target. */
static void arrive(void)
{
    PartyPlayer* pp = &party.p[party.mover];
    int at = bd.target;
    pp->space = (s16) at;

    if (at == party.star_space) {
        /* The star: passing it does not use a step. */
        if (pp->coins >= STAR_COST) {
            pp->coins = (s16) (pp->coins - STAR_COST);
            pp->stars++;
            move_star();
            say("P%d got a STAR!", party.mover + 1, 0);
            party_hud_color(bd.text, bd.line_msg, PARTY_GOLD);
            party_log("board: P%d buys a star (%d), the star moves to %d", party.mover + 1,
                      pp->stars, party.star_space);
            show_players();
        } else {
            say("A star costs %d coins...", STAR_COST, 0);
            party_hud_color(bd.text, bd.line_msg, PARTY_WHITE);
        }
    } else if (counts(at)) {
        bd.steps--;
        party_hud_set(bd.text, bd.line_big, bd.steps > 0 ? "%d" : "", bd.steps);
    }
    party_log("board: P%d reaches %d, %d to go", party.mover + 1, at, bd.steps);

    if (at == g.roulette_in) {
        bd.target = g.platform;   /* onto the roulette */
        bd.stall = 0;
        return;
    }
    if (at == g.platform) {
        bd.spin = party_rand(4);
        bd.phase = PH_SPIN;
        bd.timer = SPIN_FRAMES + SPIN_HOLD;
        say("The Goomba spins the wheel...", 0, 0);
        party_hud_color(bd.text, bd.line_msg, PARTY_WHITE);
        return;
    }
    if (counts(at) && bd.steps <= 0) {
        land();
        return;
    }
    walk_on();
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
    /* The turn, shown while paused: in the middle, clear of the pause screen's banner. */
    bd.line_turn = party_hud_line(bd.text, 0.0f, 20.0f, 1.1f, PARTY_GOLD);
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
    show_players();   /* the turn shows only while paused (board_frame) */
    party_draw_init(draw_board);
}

/* ---- camera ---- */

/* The match camera's debug free mode has no update of its own: the board sets where it looks
 * from and at every frame. High and looking down on the whole board, nearer the mover while they
 * move, or on the roulette while it spins. */
static void board_camera(void)
{
    int focus = bd.phase >= PH_INTRO && bd.phase <= PH_LAND ? party.mover : -1;
    Vec3f look = { 0.0f, 0.0f, 0.0f };
    float distance = 380.0f, pitch = 0.80f;   /* the whole board */
    float k = bd.camera_set ? 0.06f : 1.0f;
    Vec3f eye;
    HSD_CObj* cobj;

    if (bd.phase == PH_SPIN) {
        look.x = (g.x[g.middle] + g.x[g.platform]) * 0.5f;
        look.z = (g.z[g.middle] + g.z[g.platform]) * 0.5f;
        look.y = 4.0f;
        distance = 150.0f;
        pitch = 0.95f;
    } else if (focus >= 0 && bd.w[focus].placed) {
        look.x = bd.w[focus].x;
        look.z = bd.w[focus].z;
        if (bd.phase == PH_WALK) {
            look.x = (look.x + g.x[bd.target]) * 0.5f;   /* lead toward the next space */
            look.z = (look.z + g.z[bd.target]) * 0.5f;
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

/* PH_CHOOSE: a human turns the highlight with the stick toward a way and confirms with A. */
static void choose_frame(void)
{
    int at = party.p[party.mover].space;
    int done;
    bd.timer++;
    if (mover_human()) {
        HSD_PadStatus* p = pad(party.mover);
        float sx = p->nml_stickX, sz = -p->nml_stickY;   /* up on the stick is into the screen */
        if (sx * sx + sz * sz > 0.5f * 0.5f) {
            float best = -2.0f;
            int i;
            for (i = 0; i < bd.nchoice; i++) {
                float a = angle_to(at, bd.choice[i]);
                float dot = (cosf(a) * sx + sinf(a) * sz) / sqrtf(sx * sx + sz * sz);
                if (dot > best) {
                    best = dot;
                    bd.pick = i;
                }
            }
        }
        /* An idle player online must not stop the party: frame-counted, like the die. */
        done = a_pressed(party.mover) || bd.timer >= human_roll_wait();
    } else {
        done = bd.timer >= bd.cpu_wait;
    }
    if (done) {
        party_log("board: P%d takes the way to %d at %d", party.mover + 1, bd.choice[bd.pick], at);
        say("", 0, 0);
        bd.target = bd.choice[bd.pick];
        bd.stall = 0;
        bd.phase = PH_WALK;
    }
}

/* PH_SPIN: the wheel spins down onto bd.spin, then the mover goes out that way. */
static void spin_frame(void)
{
    if (bd.timer == SPIN_HOLD) {
        party_hud_set(bd.text, bd.line_msg, "The wheel says %s!", wheel_name[bd.spin]);
        party_hud_color(bd.text, bd.line_msg, PARTY_GOLD);
    }
    if (--bd.timer <= 0) {
        int slot = party.mover, out = g.exit[bd.spin];
        party.p[slot].space = (s16) out;
        bd.w[slot].x = g.x[out];
        bd.w[slot].z = g.z[out];
        party_log("board: the roulette sends P%d %s, to %d", slot + 1, wheel_name[bd.spin], out);
        say("", 0, 0);
        walk_on();
    }
}

static void board_frame(void)
{
    int paused = party_paused();
    if (paused != bd.paused && bd.text != NULL) {
        /* Paused: the turn shows. Unpausing brings back the damage percents and stocks the board
         * hides, so they go again. */
        party_hud_set(bd.text, bd.line_turn, paused ? "TURN %d OF %d" : "", party.turn,
                      party.max_turns);
        if (!paused) {
            ifAll_802F3394();
        }
        bd.paused = paused;
    }
    board_camera();   /* the board's own view, paused or not */
    if (paused) {
        return;   /* everything holds still, as the match does */
    }
    bd.frame++;
    if (bd.frame == 1) {
        /* Start pauses only once the match's HUD is on, which "GO!" does; the board hides that
         * HUD before it, so the board turns pausing on itself. */
        gmVs_GetSceneController()->state.hud_enabled = 1;
    }
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
            walk_on();
        }
        break;
    case PH_WALK: {
        Walker* w = &bd.w[party.mover];
        if (fighter(party.mover) == NULL || ++bd.stall > STALL_FRAMES) {
            party_log("board: P%d could not reach %d", party.mover + 1, bd.target);
            arrive();
            break;
        }
        if (dist(w->x, w->z, g.x[bd.target], g.z[bd.target]) <= SPACE_R) {
            arrive();
        }
        break;
    }
    case PH_CHOOSE:
        choose_frame();
        break;
    case PH_SPIN:
        spin_frame();
        break;
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
    board_init();
    party.turn++;
    party.mover = 0;
    memset(&bd, 0, sizeof bd);
    bd.phase = PH_GATHER;
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.x4_0 = false;   /* no pause camera: the board keeps its own */
    start->rules.x30 = 0.0f;   /* no damage, should anyone be hit */
    start->rules.on_match_start = board_start;
    start->rules.on_frame_start = board_frame;
    party_fill_players(start);
    party_log("turn %d/%d: board %s, star on space %d", party.turn, party.max_turns,
              board_name(party.board), party.star_space);
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
        push = steer(w, g.x[bd.target], g.z[bd.target]);
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
