/* Melee Party minigame: Volleyball. Two against two on Final Destination, a net at x = 0.
 *
 * Melee has no ball, so the ball is the Egg item (on every stage, it can be hit, TM-CE's Eggs
 * event uses it the same way) given its own copy of the egg's logic table: a hit never breaks it,
 * it cannot be picked up or nudged, and its motion is ours. Every frame end the ball's position is
 * written from our physics; a hit (the egg's damage callback) relaunches it along the attack's
 * knockback angle, away from the attacker. The ball touching the floor is a point for the other
 * side; first to VOLLEY_POINTS, or the higher score when time runs out. */
#include <math.h>
#include <string.h>

#include <melee/ft/inlines.h>
#include <melee/ft/kinds/ftCommon/ftCo_0A01.h>
#include <melee/ft/types.h>
#include <melee/gm/gmvs.h>
#include <melee/gr/forward.h>
#include <melee/it/forward.h>
#include <melee/it/inlines.h>
#include <melee/it/item.h>
#include <melee/it/kinds/types.h>
#include <melee/it/types.h>
#include <melee/pl/player.h>
#include <sysdolphin/baselib/controller.h>
#include <sysdolphin/baselib/gobj.h>
#include <sysdolphin/baselib/jobj.h>

#include "party.h"
#include "party_draw.h"
#include "party_hud.h"

#define VOLLEY_POINTS 5
#define VOLLEY_SECONDS 120

#define FLOOR_Y 0.0f
#define WALL_X 84.0f     /* Final Destination's edges are at +-85.6 */
#define CEILING_Y 140.0f
#define NET_X 0.0f
#define NET_H 26.0f
#define NET_HALF 1.0f
#define BALL_R 5.0f
#define BALL_SCALE 2.0f
#define GRAVITY 0.045f
#define FALL_MAX 1.9f
#define SERVE_DELAY 90   /* frames between a point and the next serve */

enum { SIDE_LEFT = 0, SIDE_RIGHT = 1 };

static struct {
    HSD_GObj* ball;
    ItemLogicTable logic;      /* the egg's table, with our hit callbacks */
    float x, y, vx, vy;
    int score[2];
    int serve_side;            /* the side that lost the last point serves */
    int wait;                  /* frames until the next serve, while > 0 */
    int frame;
    int over;
    s8 side[PARTY_PLAYERS];    /* each player's side */
    HSD_Text* text;
    int line_score, line_msg;
    int hits;
} vb;

/* ---- the ball ---- */

static int ball_alive(void)
{
    HSD_GObj* g;
    if (vb.ball == NULL) {
        return 0;
    }
    /* The item list, not the saved pointer: a stage or the item system can free the egg. */
    for (g = HSD_GObjPLinkHead[HSD_GOBJ_PLINK_ITEM]; g != NULL; g = g->next) {
        if (g == vb.ball) {
            return GET_ITEM(g)->kind == It_Kind_Egg;
        }
    }
    vb.ball = NULL;
    return 0;
}

static bool ball_hit(Item_GObj* gobj)
{
    Item* ip = GET_ITEM(gobj);
    float angle = (float) (ip->xCAC_angle == 361 ? 45 : ip->xCAC_angle) * (float) M_PI / 180.0f;
    float speed = 1.7f + ip->xCC8_knockback * 0.02f;
    float away = -ip->xCCC_incDamageDirection;   /* away from the attacker */
    if (speed > 3.6f) {
        speed = 3.6f;
    }
    int by = ip->xCB0_source_ply;
    float over = 0.0f;   /* toward the other court: it is volleyball, every touch is a pass */

    if (by >= 0 && by < PARTY_PLAYERS) {
        over = vb.side[by] == SIDE_LEFT ? 1.0f : -1.0f;
    }
    vb.vx = cosf(angle) * speed * away * 0.6f + over * (0.75f + speed * 0.1f);
    vb.vy = sinf(angle) * speed;
    if (sinf(angle) >= 0.0f) {
        /* anything but a spike sends the ball up, high enough to clear the net */
        vb.vy = vb.vy < 1.7f ? 1.7f : vb.vy > 2.7f ? 2.7f : vb.vy;
    } else if (vb.vy < -2.5f) {
        vb.vy = -2.5f;   /* a spike */
    }
    ip->xC9C = 0;   /* accumulated damage: never enough to matter */
    vb.hits++;
    party_log("volleyball: hit by P%d, angle %d kb %.1f -> v %.2f,%.2f", ip->xCB0_source_ply + 1,
              ip->xCAC_angle, ip->xCC8_knockback, vb.vx, vb.vy);
    return false;   /* never destroyed */
}

static bool ball_keep(Item_GObj* gobj)
{
    (void) gobj;
    return false;
}

static void ball_place(void)
{
    Item* ip;
    Vec3 pos;
    if (!ball_alive()) {
        return;
    }
    ip = GET_ITEM(vb.ball);
    pos.x = vb.x;
    pos.y = vb.y - BALL_R;   /* the egg's origin is at its bottom */
    pos.z = 0.0f;
    ip->pos = pos;
    ip->x40_vel.x = ip->x40_vel.y = ip->x40_vel.z = 0.0f;
    ip->xD44_lifeTimer = 1.0e6f;
    ip->xDC8_word.flags.x15 = 0;   /* cannot be held */
    ip->xDC8_word.flags.x1C = 0;   /* cannot be nudged */
    ip->xB8_itemLogicTable = &vb.logic;
    HSD_JObjSetTranslate(GET_JOBJ(vb.ball), &pos);
}

static void ball_remove(void)
{
    if (ball_alive()) {
        Item_8026A8EC(vb.ball);
    }
    vb.ball = NULL;
}

static void ball_spawn(void)
{
    SpawnItem spawn;
    Vec3 pos;
    Item* ip;
    Vec3 scale = { BALL_SCALE, BALL_SCALE, BALL_SCALE };

    vb.x = vb.serve_side == SIDE_LEFT ? -40.0f : 40.0f;
    vb.y = 70.0f;
    vb.vx = 0.0f;
    vb.vy = 0.6f;
    if (ball_alive()) {
        ball_place();
        return;
    }

    memset(&spawn, 0, sizeof spawn);
    pos.x = vb.x;
    pos.y = vb.y - BALL_R;
    pos.z = 0.0f;
    spawn.kind = It_Kind_Egg;
    spawn.pos = pos;
    spawn.prev_pos = pos;
    spawn.facing_dir = 1.0f;
    spawn.x44_flag.b0 = true;
    vb.ball = Item_80268B18(&spawn);
    if (vb.ball == NULL) {
        party_log("volleyball: the egg did not spawn");
        return;
    }
    ip = GET_ITEM(vb.ball);
    vb.logic = *ip->xB8_itemLogicTable;
    vb.logic.dmg_received = ball_hit;
    vb.logic.dmg_dealt = ball_keep;
    vb.logic.reflected = ball_keep;
    vb.logic.clanked = ball_keep;
    vb.logic.hit_shield = ball_keep;
    vb.logic.shield_bounced = ball_keep;
    vb.logic.absorbed = ball_keep;
    ip->scl = BALL_SCALE;
    HSD_JObjSetScale(GET_JOBJ(vb.ball), &scale);
    ball_place();
}

/* One frame of ball motion. Returns the side that lost the point, or -1. */
static int ball_step(void)
{
    float px = vb.x;
    Item* ip = GET_ITEM(vb.ball);

    if (ip->xCBC_hitlagFrames > 0.0f) {
        return -1;   /* the hit pause holds the ball too */
    }
    vb.vy -= GRAVITY;
    if (vb.vy < -FALL_MAX) {
        vb.vy = -FALL_MAX;
    }
    vb.x += vb.vx;
    vb.y += vb.vy;

    if (vb.x < -WALL_X + BALL_R) {
        vb.x = -WALL_X + BALL_R;
        vb.vx = fabsf(vb.vx) * 0.8f;
    } else if (vb.x > WALL_X - BALL_R) {
        vb.x = WALL_X - BALL_R;
        vb.vx = -fabsf(vb.vx) * 0.8f;
    }
    if (vb.y > CEILING_Y - BALL_R) {
        vb.y = CEILING_Y - BALL_R;
        vb.vy = -fabsf(vb.vy) * 0.5f;
    }

    /* The net: a thin wall from the floor to NET_H. */
    if (vb.y - BALL_R < FLOOR_Y + NET_H) {
        float edge = NET_HALF + BALL_R;
        if (px <= NET_X - edge && vb.x > NET_X - edge) {
            vb.x = NET_X - edge;
            vb.vx = -fabsf(vb.vx) * 0.6f;
        } else if (px >= NET_X + edge && vb.x < NET_X + edge) {
            vb.x = NET_X + edge;
            vb.vx = fabsf(vb.vx) * 0.6f;
        } else if (fabsf(vb.x - NET_X) < edge && vb.vy < 0.0f) {
            /* on the tape: roll off to the side it leans to */
            vb.y = FLOOR_Y + NET_H + BALL_R;
            vb.vy = fabsf(vb.vy) * 0.4f;
            vb.vx += vb.x < NET_X ? -0.3f : 0.3f;
        }
    }

    if (vb.y - BALL_R <= FLOOR_Y) {
        return vb.x < NET_X ? SIDE_LEFT : SIDE_RIGHT;
    }
    return -1;
}

/* ---- the match ---- */

static void show_score(void)
{
    party_hud_set(vb.text, vb.line_score, "%d  -  %d", vb.score[SIDE_LEFT], vb.score[SIDE_RIGHT]);
}

static void draw_net(void)
{
    static const GXColor post = { 230, 230, 230, 255 };
    static const GXColor mesh = { 200, 200, 210, 255 };
    float y;
    static const GXColor tape = { 255, 255, 255, 255 };
    for (y = FLOOR_Y + 6.0f; y < FLOOR_Y + NET_H - 2.0f; y += 4.0f) {
        party_draw_quad(mesh, NET_X - NET_HALF - 0.2f, y, NET_X + NET_HALF + 0.2f, y + 0.6f, 0.3f);
    }
    party_draw_quad(post, NET_X - 0.5f, FLOOR_Y, NET_X + 0.5f, FLOOR_Y + NET_H, 0.5f);
    party_draw_quad(tape, NET_X - NET_HALF - 0.3f, FLOOR_Y + NET_H - 1.5f, NET_X + NET_HALF + 0.3f,
                    FLOOR_Y + NET_H, 0.6f);
}

static void volley_start(void)
{
    party_hud_init();
    vb.text = party_hud_text();
    vb.line_score = party_hud_line(vb.text, 0.0f, -150.0f, 0.9f, PARTY_WHITE);
    vb.line_msg = party_hud_line(vb.text, 0.0f, -110.0f, 0.7f, PARTY_GOLD);
    party_draw_init(draw_net);
    show_score();
    party_hud_set(vb.text, vb.line_msg, "VOLLEYBALL  -  first to %d", VOLLEY_POINTS);
    vb.wait = SERVE_DELAY + 60;   /* past READY / GO */
}

static void volley_frame_end(void)
{
    int lost;
    vb.frame++;
    if (vb.over) {
        return;
    }
    if (vb.wait > 0) {
        if (--vb.wait == 0) {
            party_hud_set(vb.text, vb.line_msg, "");
            ball_spawn();
        }
        return;
    }
    if (!ball_alive()) {
        ball_spawn();
        return;
    }
    lost = ball_step();
    ball_place();
    if (lost < 0) {
        return;
    }

    vb.score[!lost]++;
    vb.serve_side = lost;
    show_score();
    party_log("volleyball: point %s, %d-%d", lost == SIDE_LEFT ? "right" : "left",
              vb.score[SIDE_LEFT], vb.score[SIDE_RIGHT]);
    if (vb.score[!lost] >= VOLLEY_POINTS) {
        vb.over = 1;
        party_hud_set(vb.text, vb.line_msg, "%s WINS!", lost == SIDE_LEFT ? "RIGHT" : "LEFT");
        party_hud_color(vb.text, vb.line_msg, lost == SIDE_LEFT ? PARTY_BLUE : PARTY_RED);
        ball_remove();
        gm_8016B33C(0);
        gm_8016B328();
        return;
    }
    party_hud_set(vb.text, vb.line_msg, "POINT %s", lost == SIDE_LEFT ? "RIGHT" : "LEFT");
    party_hud_color(vb.text, vb.line_msg, lost == SIDE_LEFT ? PARTY_BLUE : PARTY_RED);
    ball_remove();
    vb.wait = SERVE_DELAY;
}

static void volley_setup(StartMeleeData* start)
{
    int order[PARTY_PLAYERS] = { 0, 1, 2, 3 };
    int i;

    memset(&vb, 0, sizeof vb);
    vb.serve_side = party_rand(2);
    /* Random pairs. */
    for (i = PARTY_PLAYERS - 1; i > 0; i--) {
        int j = party_rand(i + 1);
        int t = order[i];
        order[i] = order[j];
        order[j] = t;
    }
    for (i = 0; i < PARTY_PLAYERS; i++) {
        vb.side[order[i]] = (s8) (i < 2 ? SIDE_LEFT : SIDE_RIGHT);
    }

    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.timer_enabled = true;
    start->rules.time_limit = VOLLEY_SECONDS;
    start->rules.is_teams = true;
    start->rules.friendly_fire = false;
    start->rules.on_match_start = volley_start;
    start->rules.on_frame_end = volley_frame_end;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        start->players[i].team = (u8) vb.side[i];
        /* Spawn on their own side: points 0/1 are left of centre on FD, 2/3 right. */
        start->players[i].spawn_pos = -1;
    }
    party_log("volleyball: left P%d+P%d, right P%d+P%d", order[0] + 1, order[1] + 1, order[2] + 1,
              order[3] + 1);
}

/* Keep everyone on their side of the net, and play the CPUs' volleyball. */
static void volley_fighter_input(Fighter* fp)
{
    int slot = fp->player_idx;
    int side;
    float x = fp->cur_pos.x;
    float toward_net;

    if (slot < 0 || slot >= PARTY_PLAYERS) {
        return;
    }
    side = vb.side[slot];
    toward_net = side == SIDE_LEFT ? 1.0f : -1.0f;

    if (ftCo_IsCpuControlled(fp) && !vb.over) {
        float home = side == SIDE_LEFT ? -40.0f : 40.0f;
        float tx = home;
        float dx, dy;
        u32 buttons = 0;
        int ball_on_side = (side == SIDE_LEFT) == (vb.x < NET_X);

        if (vb.wait == 0 && ball_alive() && ball_on_side) {
            /* Where the ball comes down: a rough look ahead. */
            tx = vb.x + vb.vx * 12.0f;
        }
        /* Their own half, away from the ledges. */
        if (side == SIDE_LEFT) {
            tx = tx < -72.0f ? -72.0f : tx > -6.0f ? -6.0f : tx;
        } else {
            tx = tx > 72.0f ? 72.0f : tx < 6.0f ? 6.0f : tx;
        }
        dx = tx - x;
        dy = vb.y - fp->cur_pos.y;
        fp->input.lstick[0].x = dx > 3.0f ? 1.0f : dx < -3.0f ? -1.0f : 0.0f;
        fp->input.lstick[0].y = 0.0f;
        if (vb.wait == 0 && ball_alive() && ball_on_side && fabsf(vb.x - x) < 14.0f) {
            if (dy > 6.0f && dy < 30.0f && (vb.frame & 7) == 0) {
                buttons |= HSD_PAD_A;           /* up-tilt or up-air under the ball */
                fp->input.lstick[0].x = toward_net * 0.3f;
                fp->input.lstick[0].y = 1.0f;
            } else if (dy >= 30.0f && dy < 60.0f && fp->ground_or_air == GA_Ground &&
                       (vb.frame & 15) == 0) {
                buttons |= HSD_PAD_X;           /* jump to meet it */
            }
        }
        fp->input.held_buttons[0] = buttons;
        fp->input.cstick[0].x = fp->input.cstick[0].y = 0.0f;
        fp->input.triggers[0] = 0.0f;
    }

    /* The net: no walking or drifting through it. */
    if (side == SIDE_LEFT ? x > NET_X - 4.0f : x < NET_X + 4.0f) {
        if (fp->input.lstick[0].x * toward_net > 0.0f) {
            fp->input.lstick[0].x = -toward_net;
        }
    }
}

static void volley_result(s8 place[PARTY_PLAYERS])
{
    int i;
    int winner = vb.score[SIDE_LEFT] > vb.score[SIDE_RIGHT]   ? SIDE_LEFT
                 : vb.score[SIDE_RIGHT] > vb.score[SIDE_LEFT] ? SIDE_RIGHT
                                                               : -1;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        place[i] = winner < 0 ? 1 : vb.side[i] == winner ? 0 : 2;
    }
    vb.ball = NULL;
}

const PartyMinigame mg_volleyball = {
    "Volleyball", "volley", volley_setup, volley_fighter_input, volley_result, 0, NULL,
    St_Kind_Last, -1,
};
