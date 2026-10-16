/* Melee Party minigame: Dungeon Duos, after Mario Party 4's (m432 in the MP4 decompilation,
 * github.com/mariopartyrd/partyboard src/REL/m432Dll/main.c). Two teams of two each run their own
 * copy of a dungeon, on a split screen, and the first team out wins:
 * - gates: each partner's gates open while the other mashes B on a switch in the wall;
 * - two turntables over pits: mash A on a crank to turn the bar, jump onto it and ride it across
 *   (MP4: Y for the first, X for the second);
 * - warp holes: one hole of each field leads on, the others to a random other hole;
 * - a pump: alternate L and R; the team's strokes add up, and 1000 wins.
 * Five minutes at most; then nobody wins.
 *
 * The layout is m432's own, read from the game's disc by tools/extract_mp4_dungeon.py
 * (dungeon_m432.h): its locators, its collision map's floors and walls, the turntables' and
 * gates' sizes. Positions here are MP4's scaled by 0.1: team 0's dungeon runs along x = -300,
 * team 1's along x = +300, from z = +50 into the screen to z = -1600. Camera, timings, mashing,
 * scoring and the CPUs' rates are m432's.
 *
 * The fighters keep Melee's physics in 3D. Melee owns their height (jumps, double jumps, gravity,
 * landing) and their speeds (walk, dash, run, air drift); the minigame owns where they are on the
 * dungeon's floor (x, z) and the height of the floor under them. Every fighter collides on Final
 * Destination at a fixed x of its own, on a floor that stands for whatever floor is under it in
 * the dungeon, and is drawn in the dungeon (dg_fighter_drawn). The control stick turns the
 * fighter to its direction and Melee moves it forward at its own speed; X and Y jump. */
#include <math.h>
#include <string.h>

#include <melee/cm/camera.h>
#include <melee/cm/types.h>
#include <melee/ft/fighter.h>
#include <melee/ft/forward.h>
#include <melee/ft/ftcommon.h>
#include <melee/ft/ftparts.h>
#include <melee/ft/inlines.h>
#include <melee/ft/kinds/ftCommon/ftCo_Fall.h>
#include <melee/ft/types.h>
#include <melee/gm/gmvs.h>
#include <melee/gr/forward.h>
#include <melee/if/ifall.h>
#include <melee/lb/lbaudio_ax.h>
#include <melee/pl/player.h>
#include <sysdolphin/baselib/cobj.h>
#include <sysdolphin/baselib/controller.h>
#include <sysdolphin/baselib/gobj.h>
#include <sysdolphin/baselib/jobj.h>

#include "dungeon_m432.h"
#include "party.h"
#include "party_draw.h"
#include "party_hud.h"

#define SCALE 0.1f            /* MP4 units to Melee's */
#define TEAM_X 300.0f         /* team t's dungeon runs along x = (2t - 1) * TEAM_X, well clear
                               * of Final Destination's own model */
#define HALF_W 40.0f          /* a dungeon's walls: x within HALF_W of its middle */
#define RADIUS 7.0f           /* a player's, against walls (m432: 70) */
#define STEP 1.0f             /* a floor this much above the feet is still underfoot */
#define LANDING 8.0f          /* in the air: Melee lands a fighter up to this far below a floor */
#define HEAD 15.0f            /* a wall below the feet + HEAD blocks */
#define FLOAT_Y 30.0f         /* over a pit, Melee's height is kept here (it never lands) */
#define FALLEN_Y (-80.0f)     /* below this a player has fallen (m432: -800) */
#define PLAY_FRAMES (300 * 60) /* m432: five minutes */
#define COUNTDOWN_AT (270 * 60)
#define INTRO_FRAMES 60
#define BOARD_DEG 12.0f       /* a CPU boards a turntable whose end points this near its way */
#define REACH 7.0f            /* how near a device's stand point a player can use it (m432: 70) */
#define MASH_MAX 25.0f        /* a switch's presses to open its gate */
#define GATE_DROP 40.0f       /* how far an open gate sinks (m432: 400) */
#define PUMP_GOAL 1000.0f
#define WIN_COINS_WAIT 200    /* frames of "WINS!" before the match ends */
#define SFX_COIN 0xAA         /* a full pump stroke */
#define SFX_THUD 0x61A87      /* a gate opening, a turntable stopping */

extern Camera game_camera;

enum { NO_FLOOR = 0, ON_MAP, ON_BAR0, ON_BAR1 };

/* A spot of a lane's table, m432's lbl_1_data_5EC / lbl_1_data_EDC: the node and the node it
 * links to (a gate's switch), what it is, and for devices the side a player stands at to use it
 * (rot degrees about y, dist MP4 units from the node). */
enum {
    F_GATE = 0x1,
    F_HOLE_P = 0x2,    /* the first field of warp holes */
    F_HOLE_R = 0x4,    /* the second */
    F_EXIT_Q = 0x8,    /* where the right hole of the first field leads */
    F_EXIT_S = 0x10,   /* and of the second */
    F_SWITCH = 0x100,  /* mash B */
    F_CRANK_X = 0x200, /* the second turntable's cranks (m432: mash X; here A) */
    F_CRANK_Y = 0x400, /* the first's (m432: mash Y; here A) */
    F_PUMP = 0x800,    /* alternate L and R */
    F_DEVICE = 0xF00,
};

typedef struct Spot {
    const char* node;
    const char* link;
    u16 flag;
    s16 rot, dist;
} Spot;

#define SPOTS 31
static const Spot lanes[2][SPOTS] = {
    {
        { "i0", NULL, 0, 0, 0 },
        { "b0", "a0", F_GATE, 0, 0 },
        { "a1", NULL, F_SWITCH, 90, 150 },
        { "b2", "a2", F_GATE, 0, 0 },
        { "a3", NULL, F_SWITCH, 90, 150 },
        { "c0", NULL, F_CRANK_Y, 90, 150 },
        { "c1", NULL, F_CRANK_Y, -90, 150 },
        { "d0", NULL, 0, 0, 0 },
        { "c2", NULL, F_CRANK_X, 90, 150 },
        { "c3", NULL, F_CRANK_X, -90, 150 },
        { "d1", NULL, 0, 0, 0 },
        { "p0", NULL, F_HOLE_P, 0, 0 },
        { "p1", NULL, F_HOLE_P, 0, 0 },
        { "p2", NULL, F_HOLE_P, 0, 0 },
        { "p3", NULL, F_HOLE_P, 0, 0 },
        { "p4", NULL, F_HOLE_P, 0, 0 },
        { "q0", NULL, F_EXIT_Q, 0, 0 },
        { "r0", NULL, F_HOLE_R, 0, 0 },
        { "r1", NULL, F_HOLE_R, 0, 0 },
        { "r2", NULL, F_HOLE_R, 0, 0 },
        { "r5", NULL, F_HOLE_R, 0, 0 },
        { "r4", NULL, F_HOLE_R, 0, 0 },
        { "r3", NULL, F_HOLE_R, 0, 0 },
        { "r6", NULL, F_HOLE_R, 0, 0 },
        { "r7", NULL, F_HOLE_R, 0, 0 },
        { "r8", NULL, F_HOLE_R, 0, 0 },
        { "s0", NULL, F_EXIT_S, 0, 0 },
        { "e0", NULL, F_PUMP, 180, 120 },
        { "j0", NULL, 0, 0, 0 },
        { "h0", NULL, 0, 0, 0 },
        { "t0", NULL, 0, 0, 0 },
    },
    {
        { "i1", NULL, 0, 0, 0 },
        { "a0", NULL, F_SWITCH, -90, 150 },
        { "b1", "a1", F_GATE, 0, 0 },
        { "a2", NULL, F_SWITCH, -90, 150 },
        { "b3", "a3", F_GATE, 0, 0 },
        { "c0", NULL, F_CRANK_Y, 90, 150 },
        { "c1", NULL, F_CRANK_Y, -90, 150 },
        { "d0", NULL, 0, 0, 0 },
        { "c2", NULL, F_CRANK_X, 90, 150 },
        { "c3", NULL, F_CRANK_X, -90, 150 },
        { "d1", NULL, 0, 0, 0 },
        { "p0", NULL, F_HOLE_P, 0, 0 },
        { "p1", NULL, F_HOLE_P, 0, 0 },
        { "p2", NULL, F_HOLE_P, 0, 0 },
        { "p3", NULL, F_HOLE_P, 0, 0 },
        { "p4", NULL, F_HOLE_P, 0, 0 },
        { "q0", NULL, F_EXIT_Q, 0, 0 },
        { "r0", NULL, F_HOLE_R, 0, 0 },
        { "r1", NULL, F_HOLE_R, 0, 0 },
        { "r2", NULL, F_HOLE_R, 0, 0 },
        { "r5", NULL, F_HOLE_R, 0, 0 },
        { "r4", NULL, F_HOLE_R, 0, 0 },
        { "r3", NULL, F_HOLE_R, 0, 0 },
        { "r6", NULL, F_HOLE_R, 0, 0 },
        { "r7", NULL, F_HOLE_R, 0, 0 },
        { "r8", NULL, F_HOLE_R, 0, 0 },
        { "s0", NULL, F_EXIT_S, 0, 0 },
        { "e1", NULL, F_PUMP, 180, 120 },
        { "k0", NULL, 0, 0, 0 },
        { "h0", NULL, 0, 0, 0 },
        { "t0", NULL, 0, 0, 0 },
    },
};

/* Where each part of the table starts (m432 fn_1_8540): what a player's progress means. */
#define P_GATES 1
#define P_TURN0 5
#define P_TURN1 8
#define P_HOLES_P 11
#define P_HOLES_R 17
#define P_PUMP 27
#define P_DONE 31

enum State {
    ST_INTRO,   /* the cameras ease down the dungeons (m432 0x3E9) */
    ST_READY,   /* "READY", "START!" */
    ST_PLAY,
    ST_FINISH,  /* "FINISH!" */
    ST_WIN,     /* the winners, or a draw */
    ST_END,
};

enum Mode {
    MD_WALK,
    MD_SWITCH,  /* mashing B at a switch (m432 0x7D4) */
    MD_CRANK,   /* mashing A at a crank (0x7D5) */
    MD_PUMP,    /* L and R at the pump (0x7D6) */
    MD_STOP,    /* the minigame is over for them */
};

typedef struct Vec3f {
    float x, y, z;
} Vec3f;

typedef struct Tri {
    float v[9];
    float x0, z0, x1, z1;   /* its extent, to skip it fast */
} Tri;

typedef struct Wall {
    float ax, az, bx, bz;   /* along the floor */
    float y0, y1;
} Wall;

/* A player: where it is in the dungeon (Melee has it on Final Destination), and what it does. */
typedef struct Duo {
    int team, lane, partner;      /* team = slot >> 1 of m432's order, lane = slot & 1 */
    int placed;
    float x, z;                   /* in the world */
    float floor;                  /* the height Melee's floor stands for */
    float yaw;                    /* facing (sin yaw, cos yaw) */
    float in_x, in_z;             /* this frame's stick, in the world (length up to 1) */
    int jump;                     /* this frame's X or Y */
    int press_a, press_b;         /* pressed this frame */
    float trig_l, trig_r;         /* analog, 0..1 */
    u32 held_prev;
    int drop;                     /* walked off a floor: falls next frame */
    float last_feet;              /* where Melee's collision left its feet last frame */
    int on;                       /* NO_FLOOR, ON_MAP or ON_BARn: what it stood on */
    int mode;
    int progress;                 /* m432 unk_17C: its index in its lane's table */
    int dev;                      /* the spot it uses (or popped out of), -1 none */
    int idle;                     /* frames since its last press (m432 unk_14C) */
    float lever, stroke_from, lever_dir;   /* the pump (m432 unk_158, unk_154, unk_150) */
    int jab;                      /* a press shows as a jab at the device */
    int exited;                   /* it came out of this field's right hole */
    int pop;                      /* frames left of coming out of a hole */
    float pop_dz;
    float back_z;                 /* m432 unk_15C: it cannot go back past a cleared turntable */
    int walk_to;                  /* m432 unk_13C: walking itself to the pump */
    float walk_x, walk_z;
    int prompt;                   /* its HUD line */
    /* CPU */
    float cpu_next, cpu_rate, cpu_scale, cpu_pump;
    int cpu_tries, cpu_hole, cpu_jump;
    int cpu_tried[10], cpu_ntried;
    float cpu_last_x, cpu_last_z;
    int cpu_stuck;
} Duo;

typedef struct Team {
    float cx;                     /* the dungeon's middle */
    Tri floors[DG_FLOORS];
    int nfloors;
    Wall walls[DG_WALLS];
    int nwalls;
    /* m432's per-lane spot state: a switch's or pump's count (unk_38), a crank's angle (unk_38)
     * and spin (unk_3C), a gate's height (unk_2C.y). */
    float val[2][SPOTS], spin[2][SPOTS], gate_y[2][SPOTS];
    u16 flag[2][SPOTS];
    float bar[2];                 /* the turntables' angles, degrees about y (m432 unk_80, unk_84) */
    float bar_prev[2];
    int section;                  /* m432 unk_18 */
    float look_y, look_z;         /* the camera's target (m432 unk_28) */
    float lock_y, lock_z;         /* where it is easing to after a section (unk_70, unk_74) */
    int ride;                     /* frames of the move to the end (unk_7C), 0 = none */
    Vec3f ride_from_look, ride_from_eye, ride_look, ride_eye;
    int checkpoint;               /* m432 unk_1C0: n0, then n1 */
    int hole_p, hole_r;           /* the right holes (m432 unk_1B8, unk_1BC) */
    float pumped;                 /* the team's strokes (m432 unk_124) */
    int line_pump;
} Team;

static struct {
    int state, timer, frame, play_frames;
    int paused;
    int winner;                   /* the winning team, -1 none */
    Duo d[PARTY_PLAYERS];
    Team t[2];
    int slot_of[2][2];            /* the player in team t, lane l */
    int view;                     /* the view being drawn */
    HSD_RectF32 full_viewport;
    Scissor full_scissor;
    float full_aspect;
    HSD_Text* text;
    int line_big, line_timer;
} dg;

/* What is drawn of each team's map: floors by shade, walls by shade (each wall at most two
 * triangles once cut off). */
static float floor_xyz[2][3][DG_FLOORS * 9];
static int floor_n[2][3];
static float wall_xyz[2][3][DG_WALLS * 2 * 9];
static int wall_n[2][3];

static Fighter* fighter(int slot)
{
    HSD_GObj* gobj = Player_GetEntity(slot);
    return gobj != NULL ? GET_FIGHTER(gobj) : NULL;
}

static int human(int slot)
{
    return party.p[slot].slot_type != Gm_PKind_Cpu;
}

/* ---- the layout ---- */

static const DgNode* node(const char* name)
{
    int i;
    for (i = 0; i < DG_NODES; i++) {
        if (strcmp(dg_nodes[i].name, name) == 0) {
            return &dg_nodes[i];
        }
    }
    party_log("dungeon: no node %s", name);
    return &dg_nodes[0];
}

static float node_x(int team, const char* name)
{
    return dg.t[team].cx + SCALE * (float) node(name)->x;
}

static float node_y(const char* name)
{
    return SCALE * (float) node(name)->y;
}

static float node_z(const char* name)
{
    return SCALE * (float) node(name)->z;
}

/* m432 fn_1_52B8: where a player stands to use a device, dist from it on its rot side. */
static void stand_point(int team, const Spot* s, float* x, float* z)
{
    float a = (float) s->rot * (float) M_PI / 180.0f;
    *x = node_x(team, s->node) + SCALE * (float) s->dist * sinf(a);
    *z = node_z(s->node) + SCALE * (float) s->dist * cosf(a);
}

/* The facing that looks at the device from its stand point (m432 fn_1_BF9C: 180 + rot). */
static float stand_yaw(const Spot* s)
{
    return ((float) s->rot + 180.0f) * (float) M_PI / 180.0f;
}

static int spot_index(int lane, const char* name)
{
    int i;
    for (i = 0; i < SPOTS; i++) {
        if (strcmp(lanes[lane][i].node, name) == 0) {
            return i;
        }
    }
    return -1;
}

/* The turntables: bar k turns about its node; its length runs along its own z. */
static const char* const bar_node[2] = { "d0", "d1" };
static const float bar_along[2][2] = {
    { SCALE * DG_TT0_ALONG0, SCALE * DG_TT0_ALONG1 },
    { SCALE * DG_TT1_ALONG0, SCALE * DG_TT1_ALONG1 },
};
static const float bar_half_w[2] = { SCALE * DG_TT0_HALF_W, SCALE * DG_TT1_HALF_W };

static int on_bar(int team, int k, float x, float z)
{
    float a = dg.t[team].bar[k] * (float) M_PI / 180.0f;
    float dx = x - node_x(team, bar_node[k]), dz = z - node_z(bar_node[k]);
    float along = dx * sinf(a) + dz * cosf(a);
    float across = dx * cosf(a) - dz * sinf(a);
    return along >= bar_along[k][0] && along <= bar_along[k][1] && fabsf(across) <= bar_half_w[k];
}

/* The highest floor at (x, z) no higher than `top`: the map's, or a turntable's bar. */
static int floor_at(int team, float x, float z, float top, float* y)
{
    const Team* t = &dg.t[team];
    int i, k, on = NO_FLOOR;
    float best = -1e9f;
    for (i = 0; i < t->nfloors; i++) {
        const Tri* f = &t->floors[i];
        const float* v = f->v;
        float d, l1, l2, l3, h;
        if (x < f->x0 || x > f->x1 || z < f->z0 || z > f->z1) {
            continue;
        }
        d = (v[5] - v[8]) * (v[0] - v[6]) + (v[6] - v[3]) * (v[2] - v[8]);
        if (d == 0.0f) {
            continue;
        }
        l1 = ((v[5] - v[8]) * (x - v[6]) + (v[6] - v[3]) * (z - v[8])) / d;
        l2 = ((v[8] - v[2]) * (x - v[6]) + (v[0] - v[6]) * (z - v[8])) / d;
        l3 = 1.0f - l1 - l2;
        if (l1 < -1e-4f || l2 < -1e-4f || l3 < -1e-4f) {
            continue;
        }
        h = l1 * v[1] + l2 * v[4] + l3 * v[7];
        if (h <= top && h > best) {
            best = h;
            on = ON_MAP;
        }
    }
    /* The gates: each stands in a slot in the floor, and its top is the floor there once it is
     * down (the map has no floor in the slot). */
    for (k = 0; k < 2; k++) {
        for (i = 0; i < SPOTS; i++) {
            const Spot* s = &lanes[k][i];
            float gx, gz, h;
            if (!(s->flag & F_GATE)) {
                continue;
            }
            gx = node_x(team, s->node);
            gz = node_z(s->node);
            if (fabsf(x - gx) > SCALE * DG_GATE_HALF_W || fabsf(z - gz) > SCALE * DG_GATE_HALF_D) {
                continue;
            }
            h = node_y(s->node) + SCALE * DG_GATE_HEIGHT + t->gate_y[k][i];
            if (h <= top && h > best) {
                best = h;
                on = ON_MAP;
            }
        }
    }
    for (k = 0; k < 2; k++) {
        float h = SCALE * DG_TT_TOP;
        if (h <= top && h > best && on_bar(team, k, x, z)) {
            best = h;
            on = ON_BAR0 + k;
        }
    }
    *y = best;
    return on;
}

/* Pushes a circle at (x, z) out of the segment a-b, `r` from it. */
static void push_out(float* x, float* z, float ax, float az, float bx, float bz, float r)
{
    float ex = bx - ax, ez = bz - az, len2 = ex * ex + ez * ez;
    float t = len2 > 0.0f ? ((*x - ax) * ex + (*z - az) * ez) / len2 : 0.0f;
    float px, pz, dx, dz, d;
    t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
    px = ax + ex * t;
    pz = az + ez * t;
    dx = *x - px;
    dz = *z - pz;
    d = sqrtf(dx * dx + dz * dz);
    if (d >= r) {
        return;
    }
    if (d < 1e-4f) {   /* on the line: out along its normal */
        float l = sqrtf(len2);
        if (l <= 0.0f) {
            return;
        }
        dx = -ez / l;
        dz = ex / l;
        d = 0.0f;
    } else {
        dx /= d;
        dz /= d;
    }
    *x += dx * (r - d);
    *z += dz * (r - d);
}

/* The map's walls and every closed gate, for feet at `feet`. */
static void collide(int team, float* x, float* z, float feet)
{
    const Team* t = &dg.t[team];
    int pass, i, lane;
    for (pass = 0; pass < 2; pass++) {
        for (i = 0; i < t->nwalls; i++) {
            const Wall* w = &t->walls[i];
            if (w->y1 > feet + STEP && w->y0 < feet + HEAD) {
                push_out(x, z, w->ax, w->az, w->bx, w->bz, RADIUS);
            }
        }
        for (lane = 0; lane < 2; lane++) {
            for (i = 0; i < SPOTS; i++) {
                const Spot* s = &lanes[lane][i];
                float gx, gz, hw = SCALE * DG_GATE_HALF_W;
                if (!(t->flag[lane][i] & F_GATE)) {
                    continue;
                }
                gx = node_x(team, s->node);
                gz = node_z(s->node);
                /* m432 fn_1_AA1C holds players behind closed gates at any height. */
                push_out(x, z, gx - hw, gz, gx + hw, gz, RADIUS + SCALE * DG_GATE_HALF_D);
            }
        }
    }
}

/* Splits a triangle's part below y = top into triangles, appended to out; returns how many. */
static int clip_below(const float* v, float top, float* out)
{
    float poly[4][3];
    int n = 0, i;
    for (i = 0; i < 3; i++) {
        const float* a = &v[3 * i];
        const float* b = &v[3 * ((i + 1) % 3)];
        if (a[1] <= top) {
            poly[n][0] = a[0];
            poly[n][1] = a[1];
            poly[n][2] = a[2];
            n++;
        }
        if ((a[1] <= top) != (b[1] <= top)) {
            float k = (top - a[1]) / (b[1] - a[1]);
            poly[n][0] = a[0] + (b[0] - a[0]) * k;
            poly[n][1] = top;
            poly[n][2] = a[2] + (b[2] - a[2]) * k;
            n++;
        }
    }
    if (n < 3) {
        return 0;
    }
    for (i = 0; i < 3; i++) {
        out[i] = poly[0][i];
        out[3 + i] = poly[1][i];
        out[6 + i] = poly[2][i];
    }
    if (n == 4) {
        for (i = 0; i < 3; i++) {
            out[9 + i] = poly[0][i];
            out[12 + i] = poly[2][i];
            out[15 + i] = poly[3][i];
        }
        return 2;
    }
    return 1;
}

static void build_team(int team)
{
    Team* t = &dg.t[team];
    int i, j, k;
    t->nfloors = t->nwalls = 0;
    for (i = 0; i < DG_FLOORS; i++) {
        Tri* f = &t->floors[t->nfloors++];
        f->x0 = f->z0 = 1e9f;
        f->x1 = f->z1 = -1e9f;
        for (j = 0; j < 3; j++) {
            f->v[3 * j] = t->cx + SCALE * (float) dg_floors[i].v[j][0];
            f->v[3 * j + 1] = SCALE * (float) dg_floors[i].v[j][1];
            f->v[3 * j + 2] = SCALE * (float) dg_floors[i].v[j][2];
            f->x0 = f->v[3 * j] < f->x0 ? f->v[3 * j] : f->x0;
            f->x1 = f->v[3 * j] > f->x1 ? f->v[3 * j] : f->x1;
            f->z0 = f->v[3 * j + 2] < f->z0 ? f->v[3 * j + 2] : f->z0;
            f->z1 = f->v[3 * j + 2] > f->z1 ? f->v[3 * j + 2] : f->z1;
        }
    }
    /* A wall is its triangle's longest side along the floor, at the triangle's heights. */
    for (i = 0; i < DG_WALLS; i++) {
        float p[3][3], best = 0.0f;
        Wall w;
        for (j = 0; j < 3; j++) {
            p[j][0] = t->cx + SCALE * (float) dg_walls[i].v[j][0];
            p[j][1] = SCALE * (float) dg_walls[i].v[j][1];
            p[j][2] = SCALE * (float) dg_walls[i].v[j][2];
        }
        w.y0 = p[0][1] < p[1][1] ? p[0][1] : p[1][1];
        w.y0 = p[2][1] < w.y0 ? p[2][1] : w.y0;
        w.y1 = p[0][1] > p[1][1] ? p[0][1] : p[1][1];
        w.y1 = p[2][1] > w.y1 ? p[2][1] : w.y1;
        for (j = 0; j < 3; j++) {
            k = (j + 1) % 3;
            {
                float dx = p[k][0] - p[j][0], dz = p[k][2] - p[j][2], d = dx * dx + dz * dz;
                if (d > best) {
                    best = d;
                    w.ax = p[j][0];
                    w.az = p[j][2];
                    w.bx = p[k][0];
                    w.bz = p[k][2];
                }
            }
        }
        if (best > 0.25f) {
            t->walls[t->nwalls++] = w;
        }
    }
}

/* What is drawn of the map: floors in three shades by height, walls in three by the way they
 * face, every wall cut off a little above the lowest floor near it (the collision map's walls go
 * far up, out of sight in MP4). */
static void build_drawing(int team)
{
    const Team* t = &dg.t[team];
    int i, j;
    for (i = 0; i < 3; i++) {
        floor_n[team][i] = wall_n[team][i] = 0;
    }
    for (i = 0; i < t->nfloors; i++) {
        const float* v = t->floors[i].v;
        float y = (v[1] + v[4] + v[7]) / 3.0f;
        int shade = y < -1.0f ? 0 : ((int) (y / 10.0f + 0.5f) & 1) + 1;
        memcpy(&floor_xyz[team][shade][9 * floor_n[team][shade]++], v, 9 * sizeof(float));
    }
    for (i = 0; i < DG_WALLS; i++) {
        float v[9], ux, uz, vx, vz, nx, nz, low = 1e9f, x0 = 1e9f, x1 = -1e9f, z0 = 1e9f,
                                                    z1 = -1e9f;
        int shade, n;
        for (j = 0; j < 3; j++) {
            v[3 * j] = t->cx + SCALE * (float) dg_walls[i].v[j][0];
            v[3 * j + 1] = SCALE * (float) dg_walls[i].v[j][1];
            v[3 * j + 2] = SCALE * (float) dg_walls[i].v[j][2];
            x0 = v[3 * j] < x0 ? v[3 * j] : x0;
            x1 = v[3 * j] > x1 ? v[3 * j] : x1;
            z0 = v[3 * j + 2] < z0 ? v[3 * j + 2] : z0;
            z1 = v[3 * j + 2] > z1 ? v[3 * j + 2] : z1;
        }
        for (j = 0; j < t->nfloors; j++) {
            const Tri* f = &t->floors[j];
            if (f->x1 >= x0 - 1.0f && f->x0 <= x1 + 1.0f && f->z1 >= z0 - 1.0f &&
                f->z0 <= z1 + 1.0f)
            {
                float y = f->v[1] < f->v[4] ? f->v[1] : f->v[4];
                y = f->v[7] < y ? f->v[7] : y;
                low = y < low ? y : low;
            }
        }
        if (low > 1e8f) {
            low = 0.0f;
        }
        ux = v[3] - v[0];
        uz = v[5] - v[2];
        vx = v[6] - v[0];
        vz = v[8] - v[2];
        {
            float uy = v[4] - v[1], vy = v[7] - v[1];
            nx = uy * vz - uz * vy;
            nz = ux * vy - uy * vx;
        }
        (void) uz;
        shade = fabsf(nx) > fabsf(nz) ? 1 : nz > 0.0f ? 2 : 0;
        n = clip_below(v, low + 30.0f, &wall_xyz[team][shade][9 * wall_n[team][shade]]);
        wall_n[team][shade] += n;
    }
}

/* ---- the camera ---- */

static void team_camera(int team, Vec3f* look, Vec3f* eye)
{
    Team* t = &dg.t[team];
    look->x = t->cx;
    look->y = t->look_y;
    look->z = t->look_z;
    /* m432 fn_1_12EA8: the eye 3900 out and 3250 up from the target. */
    eye->x = look->x;
    eye->y = look->y + SCALE * 3250.0f;
    eye->z = look->z + SCALE * 3900.0f;
    if (t->ride > 0) {
        /* The move to the end (m432 main.c 3316): from where it was to the pump, over 120. */
        float k = 1.0f - (float) t->ride / 120.0f;
        k = sinf(k * (float) M_PI * 0.5f);
        look->y = t->ride_from_look.y + (t->ride_look.y - t->ride_from_look.y) * k;
        look->z = t->ride_from_look.z + (t->ride_look.z - t->ride_from_look.z) * k;
        eye->y = t->ride_from_eye.y + (t->ride_eye.y - t->ride_from_eye.y) * k;
        eye->z = t->ride_from_eye.z + (t->ride_eye.z - t->ride_from_eye.z) * k;
    } else if (t->section >= 4) {
        *look = t->ride_look;
        *eye = t->ride_eye;
    }
}

/* Where a point shows in a team's view (318 by 480, as Hu3D3Dto2D gives it): x across, y down. */
static void project(const Vec3f* look, const Vec3f* eye, float x, float y, float z, float* sx,
                    float* sy)
{
    float fx = look->x - eye->x, fy = look->y - eye->y, fz = look->z - eye->z;
    float fl = sqrtf(fx * fx + fy * fy + fz * fz);
    float rx, rz, rl, ux, uy, uz, px, py, pz, depth, up;
    fx /= fl;
    fy /= fl;
    fz /= fl;
    rx = -fz;   /* f x (0, 1, 0) */
    rz = fx;
    rl = sqrtf(rx * rx + rz * rz);
    rx /= rl;
    rz /= rl;
    ux = -rz * fy;   /* r x f */
    uy = rz * fx - rx * fz;
    uz = rx * fy;
    px = x - eye->x;
    py = y - eye->y;
    pz = z - eye->z;
    depth = px * fx + py * fy + pz * fz;
    up = px * ux + py * uy + pz * uz;
    if (depth <= 0.0f) {
        *sx = *sy = -1000.0f;
        return;
    }
    depth *= tanf(10.0f * (float) M_PI / 180.0f);   /* fov 20 */
    *sx = 159.0f + 240.0f * (px * rx + pz * rz) / depth;
    *sy = 240.0f - 240.0f * up / depth;
}

/* m432 fn_1_7C1C: the camera follows the team into the dungeon, never back, and waits at each
 * section until both are through it. */
static void follow(int team)
{
    Team* t = &dg.t[team];
    Duo* a = &dg.d[dg.slot_of[team][0]];
    Duo* b = &dg.d[dg.slot_of[team][1]];
    if (t->ride > 0 || t->section >= 4) {
        return;
    }
    if (t->lock_y > 0.0f || t->lock_z < 0.0f) {
        if (t->lock_y > 0.0f) {
            t->look_y += 0.05f * (t->lock_y - t->look_y);
            if (fabsf(t->lock_y - t->look_y) < 0.2f) {
                t->look_y = t->lock_y;
                t->lock_y = 0.0f;
            }
        }
        if (t->lock_z < 0.0f) {
            t->look_z += 0.05f * (t->lock_z - t->look_z);
            if (fabsf(t->lock_z - t->look_z) < 0.2f) {
                t->look_z = t->lock_z;
                t->lock_z = 0.0f;
            }
        }
        return;
    }
    {
        Vec3f look, eye;
        float avg, step, sx, ya, yb;
        team_camera(team, &look, &eye);
        project(&look, &eye, a->x, t->look_y, a->z, &sx, &ya);   /* m432 fn_1_A974 */
        project(&look, &eye, b->x, t->look_y, b->z, &sx, &yb);
        avg = 0.5f * (ya + yb);
        if (avg < 265.0f) {
            step = 0.2f * (265.0f - avg);
            if (step >= 1.0f) {
                t->look_z -= SCALE * step;
            }
        }
    }
}

static int both(int team, int (*test)(const Duo*))
{
    return test(&dg.d[dg.slot_of[team][0]]) && test(&dg.d[dg.slot_of[team][1]]);
}

static int past_turn0(const Duo* d)
{
    return d->z <= SCALE * -7750.0f;
}

static int past_turn1(const Duo* d)
{
    return d->z <= SCALE * -10200.0f;
}

static int past_holes_p(const Duo* d)
{
    return d->z <= SCALE * -12300.0f;
}

static int past_holes_r(const Duo* d)
{
    return d->z <= SCALE * -14400.0f;
}

/* Out of the field's right hole and standing (m432: unk_C8 is the exit, unk_38 == 0). */
static int out_of_q(const Duo* d)
{
    Fighter* fp = fighter(dg.slot_of[d->team][d->lane]);
    return d->exited && d->pop == 0 && fp != NULL && fp->ground_or_air == GA_Ground;
}

static void advance(int team, int by)
{
    dg.d[dg.slot_of[team][0]].progress += by;
    dg.d[dg.slot_of[team][1]].progress += by;
}

static void sections(int team)
{
    Team* t = &dg.t[team];
    int i;
    switch (t->section) {
    case 0:
        if (t->look_z < SCALE * -7100.0f && !both(team, past_turn0)) {
            t->look_z = SCALE * -7100.0f;
        }
        if (both(team, past_turn0)) {
            t->section++;
            t->lock_z = SCALE * -8700.0f;
            advance(team, 3);
            for (i = 0; i < 2; i++) {
                dg.d[dg.slot_of[team][i]].back_z = SCALE * -7750.0f;
            }
            t->checkpoint = 1;
        }
        break;
    case 1:
        if (t->look_z < SCALE * -9500.0f && !both(team, past_turn1)) {
            t->look_z = SCALE * -9500.0f;
        }
        if (both(team, past_turn1)) {
            t->section++;
            t->lock_z = SCALE * -11100.0f;
            advance(team, 3);
            for (i = 0; i < 2; i++) {
                dg.d[dg.slot_of[team][i]].back_z = SCALE * -10200.0f;
            }
        }
        break;
    case 2:
        if (t->look_z < SCALE * -11500.0f && !both(team, past_holes_p)) {
            t->look_z = SCALE * -11500.0f;
        }
        if (both(team, out_of_q)) {
            t->section++;
            t->lock_z = SCALE * -13500.0f;
            t->lock_y = node_y("q0");
            advance(team, 6);
            for (i = 0; i < 2; i++) {
                dg.d[dg.slot_of[team][i]].exited = 0;
            }
        }
        break;
    case 3:
        if (t->look_z < SCALE * -13700.0f && !both(team, past_holes_r)) {
            t->look_z = SCALE * -13700.0f;
        }
        if (both(team, out_of_q)) {
            /* Both out of the last hole: the camera moves to the pump and both walk to it. */
            Vec3f look, eye;
            team_camera(team, &look, &eye);
            t->section++;
            t->ride = 120;
            t->ride_from_look = look;
            t->ride_from_eye = eye;
            t->ride_look.x = t->ride_eye.x = t->cx;
            t->ride_look.y = node_y("s0") + SCALE * 400.0f;
            t->ride_look.z = SCALE * -15700.0f;
            t->ride_eye.y = node_y("s0") + SCALE * 800.0f;
            t->ride_eye.z = t->ride_look.z + SCALE * 3800.0f;
            advance(team, 10);
            for (i = 0; i < 2; i++) {
                Duo* d = &dg.d[dg.slot_of[team][i]];
                d->walk_to = 1;
                stand_point(team, &lanes[d->lane][P_PUMP], &d->walk_x, &d->walk_z);
            }
        }
        break;
    default:
        if (t->ride > 0) {
            t->ride--;
        }
        break;
    }
}

/* cm/camera.c, in each view's pass after Melee has set the match camera up: the team's dungeon
 * in its half of the screen (m432 Hu3DCameraCreate: 318 by 480 each, 4 apart; fov 20). */
static void dg_camera_view(int view, HSD_CObj* cobj)
{
    HSD_RectF32 vp;
    float w, half, gap;
    Vec3f look, eye;
    Vec3 v;
    if (view == 1) {   /* drawn first */
        dg.full_viewport = cobj->viewport;
        dg.full_scissor = cobj->scissor;
        dg.full_aspect = HSD_CObjGetAspect(cobj);
    }
    dg.view = view;
    vp = dg.full_viewport;
    w = vp.xmax - vp.xmin;
    half = w * (318.0f / 640.0f);
    gap = w * (4.0f / 640.0f);
    if (view == 0) {
        vp.xmax = vp.xmin + half;
    } else {
        vp.xmin = vp.xmin + half + gap;
    }
    HSD_CObjSetViewportfx4(cobj, vp.xmin, vp.xmax, vp.ymin, vp.ymax);
    HSD_CObjSetScissorx4(cobj, (u16) vp.xmin, (u16) vp.xmax, dg.full_scissor.top,
                         dg.full_scissor.bottom);
    HSD_CObjSetAspect(cobj, dg.full_aspect * (318.0f / 640.0f));
    team_camera(view, &look, &eye);
    v.x = look.x;
    v.y = look.y;
    v.z = look.z;
    HSD_CObjSetInterest(cobj, &v);
    v.x = eye.x;
    v.y = eye.y;
    v.z = eye.z;
    HSD_CObjSetEyePosition(cobj, &v);
    HSD_CObjSetFov(cobj, 20.0f);
}

static int dg_camera_views(void)
{
    return dg.state >= ST_WIN && dg.winner >= 0 ? 1 : 2;
}

/* The camera of a whole screen: the winners', at the end. */
static void camera_frame(void)
{
    Vec3f look, eye;
    HSD_CObj* cobj;
    int team = dg.winner >= 0 ? dg.winner : 0;
    if (!Camera_80030178()) {
        Camera_8003006C();
    }
    team_camera(team, &look, &eye);
    dg.view = team;
    cm_80453004.free_int_pos.x = look.x;
    cm_80453004.free_int_pos.y = look.y;
    cm_80453004.free_int_pos.z = look.z;
    cm_80453004.free_eye_pos.x = eye.x;
    cm_80453004.free_eye_pos.y = eye.y;
    cm_80453004.free_eye_pos.z = eye.z;
    cm_80453004.free_fov = 20.0f;
    cobj = game_camera.gobj != NULL ? game_camera.gobj->hsd_obj : NULL;
    if (cobj != NULL) {
        HSD_CObjSetNear(cobj, 1.0f);
        HSD_CObjSetFar(cobj, 16384.0f);
    }
}

/* ---- drawing ---- */

static GXColor rgb(u8 r, u8 g, u8 b)
{
    GXColor c;
    c.r = r;
    c.g = g;
    c.b = b;
    c.a = 255;
    return c;
}

/* A bar lying at height y, turned `deg` about (x, z), from along a0 to a1 and hw either side. */
static void draw_bar(GXColor top, GXColor side, float x, float y, float z, float deg, float a0,
                     float a1, float hw, float depth)
{
    float a = deg * (float) M_PI / 180.0f, s = sinf(a), c = cosf(a);
    float corner[4][2], tris[8 * 9];
    int i, n = 0;
    const float along[4] = { a0, a1, a1, a0 }, across[4] = { -hw, -hw, hw, hw };
    for (i = 0; i < 4; i++) {
        corner[i][0] = x + along[i] * s + across[i] * c;
        corner[i][1] = z + along[i] * c - across[i] * s;
    }
    {
        float q[8];
        for (i = 0; i < 4; i++) {
            q[2 * i] = corner[i][0];
            q[2 * i + 1] = corner[i][1];
        }
        party_draw_floor_quad(top, y, q);
    }
    for (i = 0; i < 4; i++) {
        const float* p = corner[i];
        const float* r = corner[(i + 1) & 3];
        float quad[2][9] = {
            { p[0], y, p[1], r[0], y, r[1], r[0], y - depth, r[1] },
            { p[0], y, p[1], r[0], y - depth, r[1], p[0], y - depth, p[1] },
        };
        memcpy(&tris[9 * n++], quad[0], sizeof quad[0]);
        memcpy(&tris[9 * n++], quad[1], sizeof quad[1]);
    }
    party_draw_tris(side, tris, n);
}

static void draw_team(int team)
{
    static const u8 floor_rgb[3][3] = { { 40, 34, 48 }, { 150, 140, 128 }, { 132, 122, 112 } };
    static const u8 wall_rgb[3][3] = { { 92, 84, 96 }, { 110, 100, 108 }, { 128, 118, 122 } };
    const Team* t = &dg.t[team];
    int i, lane;
    for (i = 0; i < 3; i++) {
        party_draw_tris(rgb(floor_rgb[i][0], floor_rgb[i][1], floor_rgb[i][2]),
                        floor_xyz[team][i], floor_n[team][i]);
        party_draw_tris(rgb(wall_rgb[i][0], wall_rgb[i][1], wall_rgb[i][2]), wall_xyz[team][i],
                        wall_n[team][i]);
    }
    /* The pits' depths. */
    {
        float q[8];
        q[0] = t->cx - HALF_W; q[1] = SCALE * -5800.0f;
        q[2] = t->cx + HALF_W; q[3] = SCALE * -5800.0f;
        q[4] = t->cx + HALF_W; q[5] = SCALE * -10400.0f;
        q[6] = t->cx - HALF_W; q[7] = SCALE * -10400.0f;
        party_draw_floor_quad(rgb(12, 10, 18), FALLEN_Y, q);
    }
    /* The turntables. */
    for (i = 0; i < 2; i++) {
        draw_bar(rgb(170, 120, 70), rgb(110, 74, 40), node_x(team, bar_node[i]),
                 SCALE * DG_TT_TOP, node_z(bar_node[i]), t->bar[i], bar_along[i][0],
                 bar_along[i][1], bar_half_w[i], 3.5f);
    }
    for (lane = 0; lane < 2; lane++) {
        for (i = 0; i < SPOTS; i++) {
            const Spot* s = &lanes[lane][i];
            float x = node_x(team, s->node), y = node_y(s->node), z = node_z(s->node);
            if (s->flag & F_GATE) {
                float hw = SCALE * DG_GATE_HALF_W, hd = SCALE * DG_GATE_HALF_D;
                float top = y + SCALE * DG_GATE_HEIGHT + t->gate_y[lane][i];
                if (top > y + 0.5f) {
                    party_draw_box(rgb(120, 96, 60), rgb(150, 118, 70), x - hw, y, z - hd, x + hw,
                                   top, z + hd);
                }
            } else if (s->flag & F_SWITCH) {
                /* In the wall: red, going green as it is mashed. */
                float k = t->val[lane][i] / MASH_MAX;
                float dx = s->rot > 0 ? 1.0f : -1.0f;
                GXColor c = rgb((u8) (220 - 180 * k), (u8) (60 + 160 * k), 50);
                party_draw_box(c, rgb(90, 80, 70), x - (dx < 0 ? 1.5f : 0.0f), y + 8.0f, z - 3.0f,
                               x + (dx > 0 ? 1.5f : 0.0f), y + 14.0f, z + 3.0f);
            } else if (s->flag & (F_CRANK_X | F_CRANK_Y)) {
                if (lane == 1) {
                    continue;   /* both lanes' tables hold the same cranks */
                }
                party_draw_box(rgb(200, 150, 60), rgb(130, 96, 40), x - 2.0f, y, z - 2.0f, x + 2.0f,
                               y + 8.0f, z + 2.0f);
                {
                    float a = (t->val[0][i] + t->val[1][i]) * (float) M_PI / 180.0f * 4.0f;
                    draw_bar(rgb(230, 190, 90), rgb(150, 110, 50), x, y + 9.0f, z, a * 57.3f,
                             -4.0f, 4.0f, 0.8f, 1.0f);
                }
            } else if (s->flag & (F_HOLE_P | F_HOLE_R | F_EXIT_Q | F_EXIT_S)) {
                if (lane == 1) {
                    continue;
                }
                party_draw_disc_n(s->flag & (F_EXIT_Q | F_EXIT_S) ? rgb(250, 220, 90)
                                                                 : rgb(20, 16, 26),
                                  x, y + 0.08f, z, 6.0f, 20);
            } else if (s->flag & F_PUMP) {
                float lever = 0.0f;
                int slot = dg.slot_of[team][lane];
                if (dg.d[slot].mode == MD_PUMP) {
                    lever = dg.d[slot].lever;
                }
                party_draw_box(rgb(200, 60, 60), rgb(140, 40, 40), x - 3.0f, y, z - 2.0f, x + 3.0f,
                               y + 4.0f, z + 2.0f);
                party_draw_box(rgb(240, 240, 240), rgb(180, 180, 180), x - 0.6f, y + 4.0f,
                               z - 0.6f, x + 0.6f, y + 4.0f + 6.0f * (1.0f - lever), z + 0.6f);
            }
        }
    }
    /* The way out, past the pump: lit up as the team pumps. */
    {
        float k = t->pumped / PUMP_GOAL;
        float x = node_x(team, "h0"), y = node_y("h0"), z = node_z("h0");
        party_draw_disc_n(rgb((u8) (60 + 190 * k), (u8) (60 + 160 * k), 80), x, y + 0.08f, z, 12.0f,
                          24);
    }
}

static void draw_dungeon(void)
{
    if (dg.view >= 0 && dg.view < 2) {
        draw_team(dg.view);
    }
}

/* ---- the players ---- */

static void say(const char* text, GXColor color)
{
    party_hud_set(dg.text, dg.line_big, "%s", text);
    party_hud_color(dg.text, dg.line_big, color);
}

static void sfx(int id, int vol)
{
    lbAudioAx_800237A8(id, vol, 0x40);
}

static float wrap(float a)
{
    while (a > (float) M_PI) {
        a -= 2.0f * (float) M_PI;
    }
    while (a < -(float) M_PI) {
        a += 2.0f * (float) M_PI;
    }
    return a;
}

/* A device the player can use from where it stands (m432 fn_1_C33C), or -1. */
static int device_here(const Duo* d)
{
    const Duo* partner = &dg.d[d->partner];
    int i;
    for (i = d->progress; i < SPOTS; i++) {
        const Spot* s = &lanes[d->lane][i];
        float x, z;
        if (!(s->flag & F_DEVICE)) {
            continue;
        }
        stand_point(d->team, s, &x, &z);
        if ((x - d->x) * (x - d->x) + (z - d->z) * (z - d->z) > REACH * REACH) {
            continue;
        }
        if (partner->mode != MD_WALK && partner->dev >= 0 &&
            strcmp(lanes[partner->lane][partner->dev].node, s->node) == 0)
        {
            continue;   /* the partner is at it */
        }
        return i;
    }
    return -1;
}

static void stop_using(Duo* d)
{
    d->mode = MD_WALK;
    d->dev = -1;
}

/* A switch (m432 fn_1_C724): each press of B counts one, to 25; 20 frames without one take one
 * back. At 25 it is done; at 0 the player lets go. */
static void switch_frame(Duo* d)
{
    float* v = &dg.t[d->team].val[d->lane][d->dev];
    if (d->press_b) {
        d->idle = 0;
        d->jab = 1;
        *v += 1.0f;
        if (*v > MASH_MAX) {
            *v = MASH_MAX;
        }
    } else if (++d->idle >= 20) {
        d->idle = 0;
        *v -= 1.0f;
        if (*v < 0.0f) {
            *v = 0.0f;
        }
    }
    if (*v >= MASH_MAX || *v <= 0.0f) {
        if (*v >= MASH_MAX) {
            d->progress++;
        }
        stop_using(d);
    }
}

/* A crank (m432 fn_1_D0E0): each press of A spins it 3 more; 20 frames without one, the player
 * lets go. */
static void crank_frame(Duo* d)
{
    if (d->press_a) {
        d->idle = 0;
        d->jab = 1;
        dg.t[d->team].spin[d->lane][d->dev] += 3.0f;
    } else if (++d->idle >= 20) {
        stop_using(d);
    }
}

/* The pump (m432 fn_1_D9EC): L pulls the lever down, R pushes it up, one at a time. Each time it
 * turns from going up to going down, the stroke up counts ten times its length, more for a full
 * one. */
static void pump_frame(Duo* d)
{
    Team* t = &dg.t[d->team];
    float dir = 0.0f;
    if (d->trig_l <= 0.0f || d->trig_r <= 0.0f) {
        if (d->trig_l > 0.0f) {
            dir = -d->trig_l;
        }
        if (d->trig_r > 0.0f) {
            dir = d->trig_r;
        }
    }
    if (dir != 0.0f) {
        if ((dir < 0.0f) != (d->lever_dir < 0.0f)) {
            float stroke = d->lever - d->stroke_from;
            if (stroke > 0.0f) {
                if (stroke > 0.98f) {
                    stroke += 0.6f;
                    sfx(SFX_COIN, 0x50);
                }
                t->val[d->lane][d->dev] += 10.0f * stroke;
            }
            d->stroke_from = d->lever;
        }
        d->lever_dir = dir;
        d->lever += 0.1f * dir;
        d->lever = d->lever < 0.0f ? 0.0f : d->lever > 1.0f ? 1.0f : d->lever;
    }
}

/* m432 fn_1_5848: a wrong hole sends the player to another, neither the one it went in, nor the
 * right one, nor near its partner. */
static int other_hole(const Duo* d, int first, int count, int right, int in)
{
    const Duo* p = &dg.d[d->partner];
    int i, h = party_rand(count);
    for (i = 0; i < count; i++) {
        h = (h + 1) % count;
        if (h != in && h != right) {
            const Spot* s = &lanes[d->lane][first + h];
            float dx = p->x - node_x(d->team, s->node), dz = p->z - node_z(s->node);
            if (dx * dx + dz * dz > 20.0f * 20.0f) {
                return h;
            }
        }
    }
    return (in + 1 + (in + 1 == right)) % count;
}

/* A player sinking below a hole's rim within 10 of it goes in (m432 fn_1_5F2C) and comes out of
 * the next one at once: the right hole's exit, forward, or another hole, back. */
static void holes(Duo* d, Fighter* fp)
{
    Team* t = &dg.t[d->team];
    int field, first, count, right, i;
    float feet = fp->cur_pos.y + d->floor;
    if (d->progress >= P_HOLES_P && d->progress < P_HOLES_R) {
        field = 0;
        first = P_HOLES_P;
        count = 5;
        right = t->hole_p;
    } else if (d->progress >= P_HOLES_R && d->progress < P_PUMP) {
        field = 1;
        first = P_HOLES_R;
        count = 9;
        right = t->hole_r;
    } else {
        return;
    }
    for (i = 0; i < count; i++) {
        const Spot* s = &lanes[d->lane][first + i];
        float hx = node_x(d->team, s->node), hz = node_z(s->node), hy = node_y(s->node);
        const char* out;
        float dx = d->x - hx, dz = d->z - hz;
        if (feet >= hy || dx * dx + dz * dz >= 10.0f * 10.0f) {
            continue;
        }
        if (i == right) {
            out = field == 0 ? "q0" : "s0";
            d->exited = 1;
            d->pop_dz = -1.0f;   /* forward, into the screen */
            d->yaw = (float) M_PI;
            sfx(SFX_COIN, 0x7F);
        } else {
            out = lanes[d->lane][first + other_hole(d, first, count, right, i)].node;
            d->pop_dz = 1.0f;    /* back, toward the camera */
            d->yaw = 0.0f;
        }
        if (!human(dg.slot_of[d->team][d->lane])) {
            d->cpu_tried[d->cpu_ntried++ % 10] = i;
            d->cpu_tries++;
            d->cpu_hole = -1;
        }
        party_log("dungeon: P%d into %s, out of %s", dg.slot_of[d->team][d->lane] + 1, s->node, out);
        /* Out of it in a hop (m432 0x7D8): above its rim, drifting off it. */
        d->x = node_x(d->team, out);
        d->z = node_z(out);
        {
            float y;
            d->on = floor_at(d->team, d->x, d->z, node_y(out), &y);
            d->floor = d->on != NO_FLOOR ? y : node_y(out);
        }
        fp->cur_pos.y = node_y(out) + 25.0f - d->floor;
        fp->self_vel.y = 1.5f;
        d->drop = 1;
        d->pop = 24;
        return;
    }
}

/* m432 fn_1_6F28: a player who fell into a pit is put back at the checkpoint, dropping in. */
static void rescue(Duo* d, Fighter* fp)
{
    const char* n = dg.t[d->team].checkpoint ? "n1" : "n0";
    float y;
    d->x = node_x(d->team, n) + (d->lane ? 6.0f : -6.0f);
    d->z = node_z(n) + 15.0f;   /* on the floor before the pit */
    d->on = floor_at(d->team, d->x, d->z, 1000.0f, &y);
    d->floor = d->on != NO_FLOOR ? y : 0.0f;
    fp->cur_pos.y = node_y(n) + 40.0f - d->floor;
    fp->self_vel.x = fp->self_vel.y = 0.0f;
    d->yaw = (float) M_PI;
    party_log("dungeon: P%d fell, back at %s", dg.slot_of[d->team][d->lane] + 1, n);
}

/* ---- the CPUs (after m432 fn_1_93BC, with Melee's jumps) ---- */

static void cpu_go(Duo* d, float x, float z, float speed)
{
    float dx = x - d->x, dz = z - d->z, len = sqrtf(dx * dx + dz * dz);
    float k = len > 4.0f ? 1.0f : len / 4.0f;
    if (len < 0.5f) {
        return;
    }
    d->in_x = dx / len * k * speed * d->cpu_scale;
    d->in_z = dz / len * k * speed * d->cpu_scale;
}

/* Presses at the CPU's rate (m432 fn_1_906C: every 10/8/6/4 frames by difficulty). */
static int cpu_press(Duo* d)
{
    if ((float) dg.play_frames < d->cpu_next) {
        return 0;
    }
    d->cpu_next = (float) dg.play_frames + d->cpu_rate * (0.9f + 0.5f * (float) party_rand(100) / 100.0f);
    return 1;
}

/* Where an end of bar k points, as the cosine of its angle to -z: 1 toward the far side. */
static float bar_far(int team, int k, int end)
{
    float c = cosf(dg.t[team].bar[k] * (float) M_PI / 180.0f);
    return end ? c : -c;
}

/* The end of bar k the player stands on: 0 its +along end, 1 the other. */
static int bar_end(const Duo* d, int k)
{
    float a = dg.t[d->team].bar[k] * (float) M_PI / 180.0f;
    float along = (d->x - node_x(d->team, bar_node[k])) * sinf(a) +
                  (d->z - node_z(bar_node[k])) * cosf(a);
    return along >= 0.0f ? 0 : 1;
}

/* An end of bar k within `deg` of pointing at the near side (+z). */
static int bar_points_near(int team, int k, float deg)
{
    float lim = cosf(deg * (float) M_PI / 180.0f);
    if (-bar_far(team, k, 0) >= lim) {
        return 1;
    }
    return bar_along[k][0] < -10.0f && -bar_far(team, k, 1) >= lim;
}

static void cpu_turntable(Duo* d, Fighter* fp)
{
    int k = d->progress >= P_TURN1 ? 1 : 0;
    const char* edge_near = k ? "f2" : "f0";
    const char* edge_far = k ? "f3" : "f1";
    float near_z = node_z(edge_near) + 5.0f, far_z = node_z(edge_far) - 5.0f;
    int crank_near = k ? P_TURN1 : P_TURN0, crank_far = crank_near + 1;
    const Duo* p = &dg.d[d->partner];
    int me_near = d->z > near_z - 1.0f && d->on == ON_MAP;
    int me_far = d->z < far_z + 1.0f && d->on == ON_MAP;
    int p_near = p->z > near_z - 1.0f && p->on == ON_MAP;
    int p_far = p->z < far_z + 1.0f && p->on == ON_MAP;
    int p_bar = p->on == ON_BAR0 + k;
    float cx = dg.t[d->team].cx;
    int crank = -1;

    if (d->on == ON_BAR0 + k) {
        /* Riding: off to the far side once this end points there. */
        int end = bar_end(d, k);
        if (bar_far(d->team, k, end) > 0.97f) {
            /* The tip of its end, out over the far side's gap. */
            float len = end ? -bar_along[k][0] : bar_along[k][1];
            float tip_z = node_z(bar_node[k]) - len;
            cpu_go(d, d->x, far_z - 10.0f, 1.0f);
            if (fp->ground_or_air == GA_Ground && d->cpu_jump == 0 && d->z < tip_z + 6.0f) {
                d->cpu_jump = 6;
            }
        }
        return;
    }
    if (me_far) {
        if (!p_far && (p_bar || p_near)) {
            crank = crank_far;
        } else {
            cpu_go(d, cx + (d->lane ? 8.0f : -8.0f), SCALE * (k ? -10200.0f : -7750.0f) - 4.0f, 0.6f);   /* past where the section is through */
            return;
        }
    } else if (me_near) {
        if (p_bar || (p_near && d->lane == 0)) {
            crank = crank_near;
        } else {
            /* Boarding: wait at the edge in line with the bar, then run and jump onto it. */
            float wait_z = near_z + 3.0f;
            if (bar_points_near(d->team, k, BOARD_DEG)) {
                cpu_go(d, cx, near_z - 20.0f, 1.0f);
                if (d->z < wait_z + 1.0f && fp->ground_or_air == GA_Ground && d->cpu_jump == 0) {
                    d->cpu_jump = 6;
                }
            } else {
                cpu_go(d, cx, wait_z, 0.7f);
            }
            return;
        }
    } else {
        /* In the air or the pit: on toward wherever it was going. */
        if (d->z < near_z) {
            cpu_go(d, d->x, d->z - 10.0f, 1.0f);
        }
        return;
    }
    /* Cranking: until the bar is where the partner needs it. */
    {
        const Spot* s = &lanes[d->lane][crank];
        float x, z;
        int needed;
        stand_point(d->team, s, &x, &z);
        if (p_bar) {
            needed = bar_far(d->team, k, bar_end(p, k)) < 0.97f;
        } else {
            needed = !bar_points_near(d->team, k, BOARD_DEG);
        }
        if (d->mode == MD_CRANK) {
            if (needed && cpu_press(d)) {
                d->press_a = 1;
            }
        } else {
            cpu_go(d, x, z, 1.0f);
            if (needed && (x - d->x) * (x - d->x) + (z - d->z) * (z - d->z) < REACH * REACH &&
                cpu_press(d))
            {
                d->press_a = 1;
            }
        }
    }
}

/* m432 fn_1_8CB4: a hole it has not tried lately, or the right one once it tried enough, or once
 * its partner found it. */
static int cpu_pick_hole(Duo* d, int count, int right)
{
    static const u8 remember[4] = { 2, 4, 6, 8 };
    static const u8 tries[4] = { 6, 5, 4, 3 };
    int level = party.p[dg.slot_of[d->team][d->lane]].cpu_level;
    int diff = level <= 3 ? 0 : level <= 5 ? 1 : level <= 7 ? 2 : 3;
    int i, h = party_rand(count);
    if (d->cpu_tries > tries[diff] || dg.d[d->partner].exited) {
        return right;
    }
    for (i = 0; i < count; i++) {
        int j, seen = 0;
        h = (h + 1) % count;
        if (h == right) {
            continue;
        }
        for (j = 0; j < remember[diff] && j < d->cpu_ntried && j < 10; j++) {
            if (d->cpu_tried[(d->cpu_ntried - 1 - j) % 10] == h) {
                seen = 1;
            }
        }
        if (!seen) {
            return h;
        }
    }
    return right;
}

static void cpu_frame(Duo* d, Fighter* fp)
{
    Team* t = &dg.t[d->team];
    d->in_x = d->in_z = 0.0f;
    d->jump = d->press_a = d->press_b = 0;
    d->trig_l = d->trig_r = 0.0f;
    if (d->progress < P_TURN0) {
        /* The gates: to its next switch, and mash. */
        int i = d->progress;
        while (i < SPOTS && !(lanes[d->lane][i].flag & F_DEVICE)) {
            i++;
        }
        if (i < SPOTS) {
            float x, z;
            stand_point(d->team, &lanes[d->lane][i], &x, &z);
            if (d->mode == MD_SWITCH) {
                d->press_b = cpu_press(d);
            } else {
                cpu_go(d, x, z, 1.0f);
                if ((x - d->x) * (x - d->x) + (z - d->z) * (z - d->z) < REACH * REACH) {
                    d->press_b = cpu_press(d);
                }
            }
        }
    } else if (d->progress < P_HOLES_P) {
        cpu_turntable(d, fp);
    } else if (d->progress < P_PUMP) {
        int field = d->progress >= P_HOLES_R;
        int first = field ? P_HOLES_R : P_HOLES_P, count = field ? 9 : 5;
        int right = field ? t->hole_r : t->hole_p;
        if (d->exited) {
            return;   /* out: waits for its partner */
        }
        if (d->cpu_hole < 0 || d->pop > 0) {
            if (d->pop > 0) {
                return;
            }
            d->cpu_hole = cpu_pick_hole(d, count, right);
        }
        {
            const Spot* s = &lanes[d->lane][first + d->cpu_hole];
            float x = node_x(d->team, s->node), z = node_z(s->node);
            float dx = x - d->x, dz = z - d->z;
            cpu_go(d, x, z, 1.0f);
            if (dx * dx + dz * dz < 16.0f * 16.0f && fp->ground_or_air == GA_Ground &&
                d->cpu_jump == 0 && d->floor >= node_y(s->node) - 0.5f)
            {
                d->cpu_jump = 4;   /* over the rim */
            }
        }
    } else if (d->mode == MD_PUMP) {
        /* Full strokes, faster or slower to finish near its target time (m432 fn_1_906C). */
        static const float target[4] = { 105.0f, 95.0f, 85.0f, 75.0f };
        int level = party.p[dg.slot_of[d->team][d->lane]].cpu_level;
        int diff = level <= 3 ? 0 : level <= 5 ? 1 : level <= 7 ? 2 : 3;
        if (d->lever_dir > 0.0f ? d->lever < 1.0f : d->lever <= 0.0f) {
            d->trig_r = d->cpu_pump;
        } else {
            d->trig_l = d->cpu_pump;
        }
        d->cpu_pump *= (float) dg.play_frames < target[diff] * 60.0f ? 0.99f : 1.01f;
        d->cpu_pump = d->cpu_pump < 0.5f ? 0.5f : d->cpu_pump > 1.0f ? 1.0f : d->cpu_pump;
    }

    /* Stuck against a step or a rim while trying to move: jump. */
    if (d->in_x * d->in_x + d->in_z * d->in_z > 0.2f && d->mode == MD_WALK) {
        float mx = d->x - d->cpu_last_x, mz = d->z - d->cpu_last_z;
        if (mx * mx + mz * mz < 0.01f && fp->ground_or_air == GA_Ground) {
            if (++d->cpu_stuck > 10 && d->cpu_jump == 0) {
                d->cpu_jump = 6;
                d->cpu_stuck = 0;
            }
        } else {
            d->cpu_stuck = 0;
        }
    }
    d->cpu_last_x = d->x;
    d->cpu_last_z = d->z;
    if (d->cpu_jump > 0) {
        /* A full jump, and a second one on its way down. */
        d->jump = d->cpu_jump > 1;
        d->cpu_jump--;
        if (d->cpu_jump == 0 && fp->ground_or_air == GA_Air) {
            d->cpu_jump = -24;
        }
    } else if (d->cpu_jump < 0) {
        if (++d->cpu_jump == -8 && fp->self_vel.y < 0.0f) {
            d->jump = 1;
        }
    }
}

/* ---- the game ---- */

static void set_state(int state)
{
    dg.state = state;
    dg.timer = 0;
}

static void finish(int winner)
{
    int i;
    dg.winner = winner;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        dg.d[i].mode = MD_STOP;
        dg.d[i].walk_to = 0;
    }
    party_log("dungeon: %s after %d frames", winner < 0 ? "no winner" : winner ? "team 2 wins" : "team 1 wins",
              dg.play_frames);
    set_state(ST_FINISH);
    say("FINISH!", PARTY_GOLD);
}

/* m432 fn_1_623C: the cranks spin down, the turntables turn by both cranks of theirs, the gates
 * follow their switches, and the pumps add up. */
static void devices(int team)
{
    Team* t = &dg.t[team];
    float turn[2] = { 0.0f, 0.0f };
    int lane, i;
    t->pumped = 0.0f;
    for (lane = 0; lane < 2; lane++) {
        for (i = 0; i < SPOTS; i++) {
            const Spot* s = &lanes[lane][i];
            if (s->flag & (F_CRANK_X | F_CRANK_Y)) {
                float* spin = &t->spin[lane][i];
                if (*spin > 1.0f) {
                    *spin = 1.0f;
                }
                t->val[lane][i] += *spin;
                *spin -= 0.05f;
                if (*spin < 0.0f) {
                    *spin = 0.0f;
                }
                turn[(s->flag & F_CRANK_Y) ? 0 : 1] += t->val[lane][i];
            } else if (s->flag & F_PUMP) {
                t->pumped += t->val[lane][i];
            } else if (t->flag[lane][i] & F_GATE) {
                /* m432 fn_1_5BAC: 8 down for each press on its switch, all the way at 25. */
                int other = spot_index(lane ^ 1, s->link);
                float v = other >= 0 ? t->val[lane ^ 1][other] : 0.0f;
                float target = v >= MASH_MAX ? -GATE_DROP : -0.8f * v;
                t->gate_y[lane][i] += 0.25f * (target - t->gate_y[lane][i]);
                if (t->gate_y[lane][i] < -(GATE_DROP - 0.1f)) {
                    t->flag[lane][i] &= ~F_GATE;
                    dg.d[dg.slot_of[team][lane]].progress++;
                    sfx(SFX_THUD, 0x60);
                }
            }
        }
    }
    t->bar_prev[0] = t->bar[0];
    t->bar_prev[1] = t->bar[1];
    t->bar[0] = 80.0f - turn[0];
    t->bar[1] = 160.0f - turn[1];
    if (t->pumped >= PUMP_GOAL) {
        t->pumped = PUMP_GOAL;
        if (dg.state == ST_PLAY) {
            finish(team);
        }
    }
}

static void show_hud(void)
{
    int i;
    for (i = 0; i < 2; i++) {
        Team* t = &dg.t[i];
        if (t->section >= 4 && (dg.state < ST_WIN || i == dg.winner)) {
            party_hud_set(dg.text, t->line_pump, "%d", (int) t->pumped);
        }
    }
    if (dg.state == ST_PLAY) {
        int s = dg.play_frames / 60;
        if (dg.play_frames >= COUNTDOWN_AT) {
            party_hud_set(dg.text, dg.line_timer, "%d", (PLAY_FRAMES - dg.play_frames + 59) / 60);
            party_hud_color(dg.text, dg.line_timer, PARTY_GOLD);
        } else {
            party_hud_set(dg.text, dg.line_timer, "%d:%02d", s / 60, s % 60);
        }
    }
    /* The button a player can press, over its head in its team's view. */
    for (i = 0; i < PARTY_PLAYERS; i++) {
        Duo* d = &dg.d[i];
        const char* label = "";
        Fighter* fp = fighter(i);
        int dev = -1;
        if (fp == NULL || dg.state != ST_PLAY) {
            party_hud_set(dg.text, d->prompt, "");
            continue;
        }
        if (d->mode == MD_WALK && fp->ground_or_air == GA_Ground) {
            dev = device_here(d);
        } else if (d->mode != MD_WALK && d->mode != MD_STOP) {
            dev = d->dev;
        }
        if (dev >= 0) {
            u16 f = lanes[d->lane][dev].flag;
            label = f & F_SWITCH ? "B" : f & F_PUMP ? "L R" : "A";
        }
        if (*label != '\0' && human(i)) {
            Vec3f look, eye;
            float sy, sx;
            team_camera(d->team, &look, &eye);
            project(&look, &eye, d->x, fp->cur_pos.y + d->floor + 22.0f, d->z, &sx, &sy);
            party_hud_move(dg.text, d->prompt, (d->team ? 322.0f : 0.0f) + sx - 320.0f, sy - 240.0f);
        }
        party_hud_set(dg.text, d->prompt, "%s", human(i) ? label : "");
    }
}

static void dg_frame(void)
{
    int paused = party_paused();
    int i;

    if (paused != dg.paused) {
        if (!paused) {
            ifAll_802F3394();   /* unpausing brings back the damage percents and stocks */
        }
        dg.paused = paused;
    }
    if (paused) {
        return;
    }
    dg.frame++;
    if (dg.frame == 1) {
        /* Start pauses only once the match's HUD is on, which "GO!" does; the minigame hides that
         * HUD before it, so it turns pausing on itself. */
        gmVs_GetSceneController()->state.hud_enabled = 1;
    }
    dg.timer++;
    for (i = 0; i < 2; i++) {
        devices(i);
    }
    switch (dg.state) {
    case ST_INTRO: {
        /* m432 0x3E9: the targets ease from z -300 to -700 over a second. */
        float k = sinf((float) dg.timer / INTRO_FRAMES * (float) M_PI * 0.5f);
        for (i = 0; i < 2; i++) {
            dg.t[i].look_z = SCALE * (-300.0f - 400.0f * k * k);
        }
        if (dg.timer >= INTRO_FRAMES) {
            set_state(ST_READY);
            say("READY", PARTY_WHITE);
        }
        break;
    }
    case ST_READY:
        if (dg.timer == 60) {
            say("START!", PARTY_GOLD);
        } else if (dg.timer >= 100) {
            say("", PARTY_WHITE);
            set_state(ST_PLAY);
        }
        break;
    case ST_PLAY:
        dg.play_frames++;
        for (i = 0; i < 2; i++) {
            follow(i);
            sections(i);
        }
        if (dg.play_frames >= PLAY_FRAMES && dg.state == ST_PLAY) {
            finish(-1);
        }
        break;
    case ST_FINISH:
        for (i = 0; i < 2; i++) {
            if (dg.t[i].ride > 0) {
                dg.t[i].ride--;
            }
        }
        if (dg.timer >= 90) {
            set_state(ST_WIN);
            if (dg.winner < 0) {
                say("DRAW", PARTY_WHITE);
            } else {
                int a = dg.slot_of[dg.winner][0], b = dg.slot_of[dg.winner][1];
                party_hud_set(dg.text, dg.line_big, "P%d   P%d  WIN!", (a < b ? a : b) + 1,
                              (a < b ? b : a) + 1);
                party_hud_color(dg.text, dg.line_big, PARTY_GOLD);
                party_hud_set(dg.text, dg.t[dg.winner ^ 1].line_pump, "");
                party_hud_move(dg.text, dg.t[dg.winner].line_pump, 0.0f, -170.0f);
            }
            party_hud_set(dg.text, dg.line_timer, "");
        }
        break;
    case ST_WIN:
        if (dg.timer >= WIN_COINS_WAIT) {
            set_state(ST_END);
            gm_8016B328();
        }
        break;
    default:
        break;
    }
    show_hud();
    camera_frame();
}

static void dg_start(void)
{
    static const GXColor port[PARTY_PLAYERS] = {
        { 255, 80, 80, 255 }, { 90, 150, 255, 255 }, { 255, 210, 60, 255 }, { 80, 220, 110, 255 },
    };
    int i;

    ifAll_802F3394();   /* no damage percents or stocks */
    party_hud_init();
    dg.text = party_hud_text();
    dg.line_big = party_hud_line(dg.text, 0.0f, -40.0f, 1.6f, PARTY_WHITE);
    dg.line_timer = party_hud_line(dg.text, 0.0f, -215.0f, 0.9f, PARTY_WHITE);
    for (i = 0; i < 2; i++) {
        dg.t[i].line_pump = party_hud_line(dg.text, i ? 160.0f : -160.0f, -180.0f, 1.0f,
                                           i ? PARTY_BLUE : PARTY_RED);
    }
    for (i = 0; i < PARTY_PLAYERS; i++) {
        dg.d[i].prompt = party_hud_line(dg.text, 0.0f, 0.0f, 0.8f, port[i]);
    }
    for (i = 0; i < 2; i++) {
        build_team(i);
        build_drawing(i);
    }
    dg.view = 0;
    party_draw_init(draw_dungeon);
    camera_frame();
}

static void dg_setup(StartMeleeData* start)
{
    int order[PARTY_PLAYERS] = { 0, 1, 2, 3 };
    int i;

    memset(&dg, 0, sizeof dg);
    dg.winner = -1;
    /* Random pairs, as m432 pairs its teams (main.c 3685). */
    for (i = PARTY_PLAYERS - 1; i > 0; i--) {
        int j = party_rand(i + 1);
        int t = order[i];
        order[i] = order[j];
        order[j] = t;
    }
    for (i = 0; i < 2; i++) {
        Team* t = &dg.t[i];
        int lane;
        t->cx = i ? TEAM_X : -TEAM_X;
        t->look_y = 0.0f;
        t->look_z = SCALE * -300.0f;
        t->hole_p = party_rand(5);   /* m432 main.c 3660 */
        t->hole_r = party_rand(9);
        for (lane = 0; lane < 2; lane++) {
            int k;
            for (k = 0; k < SPOTS; k++) {
                t->flag[lane][k] = lanes[lane][k].flag;
            }
        }
        t->bar[0] = 80.0f;
        t->bar[1] = 160.0f;
    }
    for (i = 0; i < PARTY_PLAYERS; i++) {
        Duo* d = &dg.d[order[i]];
        static const float rate[4] = { 10.0f, 8.0f, 6.0f, 4.0f };
        static const float scale[4] = { 0.7f, 0.8f, 0.9f, 1.0f };
        int level = party.p[order[i]].cpu_level, diff = level <= 3 ? 0 : level <= 5 ? 1 : level <= 7 ? 2 : 3;
        d->team = i >> 1;
        d->lane = i & 1;
        d->progress = P_GATES;
        d->dev = -1;
        d->lever_dir = -1.0f;
        d->back_z = 1e9f;
        d->cpu_rate = rate[diff];
        d->cpu_scale = scale[diff];
        d->cpu_pump = 0.5f;
        d->cpu_hole = -1;
        dg.slot_of[d->team][d->lane] = order[i];
    }
    for (i = 0; i < PARTY_PLAYERS; i++) {
        Duo* d = &dg.d[i];
        d->partner = dg.slot_of[d->team][d->lane ^ 1];
    }

    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.x4_0 = false;    /* no pause camera: the minigame keeps its own */
    start->rules.x1_2 = true;     /* no "Ready... GO!": the minigame shows its own start */
    start->rules.x30 = 0.0f;      /* nobody is hurt */
    start->rules.is_teams = true;
    start->rules.friendly_fire = false;
    start->rules.on_match_start = dg_start;
    start->rules.on_frame_start = dg_frame;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        start->players[i].team = (u8) dg.d[i].team;
    }
    party_log("dungeon: team 1 P%d+P%d, team 2 P%d+P%d; holes %d %d", order[0] + 1, order[1] + 1,
              order[2] + 1, order[3] + 1, dg.t[0].hole_p, dg.t[0].hole_r);
}

/* What the player asks for this frame: a human's pad, in the world (up on the stick is into the
 * screen), or the CPU's choice. */
static void read_input(Duo* d, Fighter* fp, int slot, float lx, float ly, u32 held)
{
    if (human(slot)) {
        HSD_PadStatus* pad = &HSD_PadGameStatus[fp->pad_port & 3];
        d->in_x = lx;
        d->in_z = -ly;
        d->jump = (held & HSD_PAD_XY) != 0;
        d->press_a = (held & HSD_PAD_A) && !(d->held_prev & HSD_PAD_A);
        d->press_b = (held & HSD_PAD_B) && !(d->held_prev & HSD_PAD_B);
        d->trig_l = (held & HSD_PAD_L) ? 1.0f : pad->nml_analogL;
        d->trig_r = (held & HSD_PAD_R) ? 1.0f : pad->nml_analogR;
        d->trig_l = d->trig_l < 0.1f ? 0.0f : d->trig_l;
        d->trig_r = d->trig_r < 0.1f ? 0.0f : d->trig_r;
        d->held_prev = held;
    } else {
        cpu_frame(d, fp);
    }
}

/* Fighter_procInput, after the pad or the CPU: Melee gets only a push forward (the stick's
 * strength, whichever way the fighter faces) and X/Y. */
static void dg_fighter_input(Fighter* fp)
{
    int slot = fp->player_idx;
    Duo* d;
    float mag, lx = fp->input.lstick[0].x, ly = fp->input.lstick[0].y;
    u32 held = fp->input.held_buttons[0];

    /* Melee works the collision box out from the bones: until the dungeon draws it again
     * (dg_fighter_drawn), the model stands where the fighter collides. */
    HSD_JObjSetTranslate(fp->gobj->hsd_obj, &fp->cur_pos);
    fp->input.lstick[0].x = fp->input.lstick[0].y = 0.0f;
    fp->input.cstick[0].x = fp->input.cstick[0].y = 0.0f;
    fp->input.triggers[0] = 0.0f;
    if (slot < 0 || slot >= PARTY_PLAYERS) {
        fp->input.held_buttons[0] = 0;
        return;
    }
    d = &dg.d[slot];
    if (fp->is_sub_fighter) {
        /* Nana repeats Popo's inputs a few frames late, and stands where he does. */
        fp->input.held_buttons[0] &= dg.state == ST_PLAY && d->mode == MD_WALK ? HSD_PAD_XY : 0;
        return;
    }
    fp->input.held_buttons[0] = 0;
    if (d->drop && fp->ground_or_air == GA_Ground) {
        /* It walked off its floor last frame: down it goes. */
        ftCommon_8007D5D4(fp);
        ftCo_Fall_Enter(fp->gobj);
    }
    d->drop = 0;
    if (dg.state != ST_PLAY) {
        if (dg.state == ST_WIN && dg.winner == d->team && dg.timer == 30) {
            fp->input.held_buttons[0] = HSD_PAD_DPADUP;   /* the winners taunt */
        }
        return;
    }
    read_input(d, fp, slot, lx, ly, held);

    if (d->walk_to) {
        /* m432 fn_1_ED0C: walking itself to the pump. */
        float dx = d->walk_x - d->x, dz = d->walk_z - d->z, len = sqrtf(dx * dx + dz * dz);
        d->in_x = d->in_z = 0.0f;
        d->jump = 0;
        if (len > 1.0f) {
            d->in_x = 0.6f * dx / len;
            d->in_z = 0.6f * dz / len;
        } else {
            d->walk_to = 0;
        }
    }
    if (d->pop > 0) {
        d->in_x = d->in_z = 0.0f;
        d->jump = 0;
    }

    if (d->mode == MD_WALK && fp->ground_or_air == GA_Ground && d->pop == 0) {
        int dev = device_here(d);
        if (dev >= 0) {
            u16 f = lanes[d->lane][dev].flag;
            int use = f & F_SWITCH ? d->press_b : f & F_PUMP ? 1 : d->press_a;
            if (use) {
                Team* t = &dg.t[d->team];
                d->dev = dev;
                d->idle = 0;
                d->jab = 1;
                if (f & F_SWITCH) {
                    d->mode = MD_SWITCH;
                    t->val[d->lane][dev] = 1.0f;   /* m432: the first press counts */
                } else if (f & F_PUMP) {
                    d->mode = MD_PUMP;
                    d->lever = d->stroke_from = 0.0f;
                    d->lever_dir = -1.0f;
                    d->walk_to = 0;
                } else {
                    d->mode = MD_CRANK;
                    t->spin[d->lane][dev] += 3.0f;
                }
                d->press_a = d->press_b = 0;
            }
        }
    }
    switch (d->mode) {
    case MD_SWITCH:
        switch_frame(d);
        break;
    case MD_CRANK:
        crank_frame(d);
        break;
    case MD_PUMP:
        pump_frame(d);
        break;
    default:
        break;
    }
    if (d->mode != MD_WALK) {
        /* At a device: still, facing it, and a jab for each press. */
        if (d->jab) {
            fp->input.held_buttons[0] = HSD_PAD_A;
        }
        d->jab = 0;
        return;
    }
    d->jab = 0;

    mag = sqrtf(d->in_x * d->in_x + d->in_z * d->in_z);
    if (mag > 1.0f) {
        mag = 1.0f;
    }
    if (mag > 0.2f) {
        /* m432 fn_1_40C0: the facing turns toward the stick, 0.4 of the way a frame (in the air
         * slower, as Melee's drift is). */
        float want = atan2f(d->in_x, d->in_z);
        d->yaw = wrap(d->yaw + wrap(want - d->yaw) * (fp->ground_or_air == GA_Ground ? 0.4f : 0.2f));
        fp->input.lstick[0].x = mag * fp->facing_dir;
    }
    if (d->jump) {
        fp->input.held_buttons[0] |= HSD_PAD_X;
    }
}

/* Fighter_procMap, before collision: the fighter moves through the dungeon at its own speed, and
 * Melee's floor is set to stand for the floor under it there. */
static void dg_fighter_map(Fighter* fp)
{
    int slot = fp->player_idx;
    Duo* d;
    Team* t;
    float my, feet, y, v, hx, hz;
    int on, k;

    if (slot < 0 || slot >= PARTY_PLAYERS) {
        return;
    }
    d = &dg.d[slot];
    t = &dg.t[d->team];
    fp->cur_pos.x = -45.0f + 30.0f * (float) slot;   /* where it collides on Final Destination */
    fp->cur_pos.z = 0.0f;
    if (fp->is_sub_fighter) {
        return;
    }
    if (!d->placed) {
        const char* start = d->lane ? "m1" : "m0";   /* past the holes m432 starts in */
        d->x = node_x(d->team, start);
        d->z = node_z(start);
        d->floor = 0.0f;
        d->yaw = (float) M_PI;
        d->on = ON_MAP;
        d->placed = 1;
    }

    /* Riding a turntable: it carries the player round with it. */
    for (k = 0; k < 2; k++) {
        float turn = t->bar[k] - t->bar_prev[k];
        if (d->on == ON_BAR0 + k && fp->ground_or_air == GA_Ground && turn != 0.0f) {
            float a = turn * (float) M_PI / 180.0f, px = node_x(d->team, bar_node[k]);
            float pz = node_z(bar_node[k]), dx = d->x - px, dz = d->z - pz;
            d->x = px + cosf(a) * dx + sinf(a) * dz;
            d->z = pz - sinf(a) * dx + cosf(a) * dz;
            d->yaw = wrap(d->yaw + a);
        }
    }

    my = fp->cur_pos.y;
    feet = my + d->floor;
    hx = sinf(d->yaw);
    hz = cosf(d->yaw);
    if (d->mode != MD_WALK && d->mode != MD_STOP && d->dev >= 0) {
        /* m432 fn_1_BF9C: pulled to the device's stand point, facing it. */
        const Spot* s = &lanes[d->lane][d->dev];
        float sx, sz, dx, dz, len;
        stand_point(d->team, s, &sx, &sz);
        dx = sx - d->x;
        dz = sz - d->z;
        len = sqrtf(dx * dx + dz * dz);
        if (len > 1.0f) {
            dx /= len;
            dz /= len;
            len = 1.0f;
        }
        d->x += dx * len;
        d->z += dz * len;
        d->yaw = wrap(d->yaw + wrap(stand_yaw(s) - d->yaw) * 0.4f);
    } else if (d->pop > 0) {
        d->pop--;
        d->z += d->pop_dz * 1.2f;
    } else if (d->mode != MD_STOP) {
        v = fp->ground_or_air == GA_Ground ? fp->gr_vel * fp->facing_dir
                                           : fp->self_vel.x * fp->facing_dir;
        d->x += hx * v;
        d->z += hz * v;
    }

    /* Within the camera's reach, then out of the walls and closed gates, which win (m432
     * fn_1_AA1C: a player held behind a gate holds there however far its partner goes). */
    if (dg.state == ST_PLAY && t->section < 4) {
        if (d->z < t->look_z - 75.0f) {
            d->z = t->look_z - 75.0f;
        }
        if (d->z > t->look_z + 90.0f) {
            d->z = t->look_z + 90.0f;
        }
    }
    if (d->z > d->back_z) {
        d->z = d->back_z;
    }
    collide(d->team, &d->x, &d->z, feet);
    if (d->x < t->cx - HALF_W + RADIUS) {
        d->x = t->cx - HALF_W + RADIUS;
    } else if (d->x > t->cx + HALF_W - RADIUS) {
        d->x = t->cx + HALF_W - RADIUS;
    }

    /* Melee's collision has not run yet, and lands a fighter only a few frames after its feet go
     * through the floor: the floor is looked for from a little above where its feet were. */
    on = floor_at(d->team, d->x, d->z,
                  (feet > d->last_feet ? feet : d->last_feet) +
                      (fp->ground_or_air == GA_Ground ? STEP : LANDING),
                  &y);
    if (fp->ground_or_air == GA_Ground) {
        if (on == NO_FLOOR || y < d->floor - 0.3f) {
            d->drop = 1;   /* off the edge: Fall next frame (dg_fighter_input) */
        } else {
            d->floor = y;
            d->on = on;
        }
    } else if (on != NO_FLOOR) {
        my += d->floor - y;
        d->floor = y;
        d->on = on;
    } else {
        /* Over a pit: Melee's height stays put and the fall is the floor's. */
        d->floor += my - FLOAT_Y;
        my = FLOAT_Y;
        d->on = NO_FLOOR;
    }
    fp->cur_pos.y = my;

    if (dg.state == ST_PLAY && d->mode == MD_WALK) {
        holes(d, fp);
        if (fp->cur_pos.y + d->floor < FALLEN_Y) {
            rescue(d, fp);
        }
    }
}

/* After collision: the fighter is drawn where it is in the dungeon, turned its way. Nana a step
 * behind Popo. */
static void dg_fighter_drawn(Fighter* fp)
{
    int slot = fp->player_idx;
    Duo* d;
    Vec3 pos;
    float yaw;
    if (slot < 0 || slot >= PARTY_PLAYERS) {
        return;
    }
    d = &dg.d[slot];
    pos.x = d->x;
    pos.y = fp->cur_pos.y + d->floor;
    if (!fp->is_sub_fighter) {
        d->last_feet = pos.y;
    }
    pos.z = d->z;
    yaw = d->yaw;
    if (fp->is_sub_fighter) {
        pos.x -= sinf(yaw) * 3.0f;
        pos.z -= cosf(yaw) * 3.0f;
    }
    ftPartSetRotY(fp, 0, yaw);
    HSD_JObjSetTranslate(fp->gobj->hsd_obj, &pos);
}

static float dg_knockback(Fighter* fp, float kb)
{
    (void) fp;
    (void) kb;
    return 0.0f;   /* nobody is knocked anywhere: they are all far from each other in Melee */
}

static void dg_result(s8 place[PARTY_PLAYERS])
{
    int i;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        place[i] = dg.winner < 0 ? 1 : dg.d[i].team == dg.winner ? 0 : 2;
    }
}

const PartyMinigame mg_dungeon = {
    "Dungeon Duos", "dungeon", dg_setup, dg_fighter_input, dg_result, 0, NULL,
    St_Kind_Last, -1, dg_fighter_map, dg_fighter_drawn, dg_knockback, 1,
    dg_camera_views, dg_camera_view,
};
