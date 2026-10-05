/* Melee Party minigame: Domination, after Mario Party 4's (m407 in the MP4 decompilation,
 * github.com/mariopartyrd/partyboard src/REL/m407dll), rebuilt with Melee's own things:
 * - every player holds a hammer and swings it once per A press at a Wobbuffet in front of them;
 * - every swing drops a Snorlax from the sky into the player's lane, the way every pound in
 *   Domination raises a Whomp;
 * - after 10 seconds the Snorlaxes fall on their backs one after another like dominoes while each
 *   lane counts up, and whoever made the most wins.
 * As in m407, every lane starts full of Snorlaxes, which all jump into the sky once the camera
 * reaches the players (whomp.c fn_1_2564); and at the end a Snorlax holding a parasol comes out
 * of the ground past each winner's last one (fn_1_2770), and the winners jump off, drop from the
 * sky onto it (player.c fn_1_1074, fn_1_11CC) and taunt.
 *
 * Layout, camera shots, timings and the CPUs' pounding rates are m407's. Positions here are m407's
 * scaled by 0.1 and mirrored so the lanes run away from Melee's usual camera side (into -z): a
 * player stands at x = -60 + 40 * slot, z = 0, and Snorlax n of a lane lands at z = -(60 + 20 n).
 *
 * The fighters are real (on Final Destination, nobody can move or attack: their inputs are
 * dropped); their own hammer state does the swinging. The Snorlaxes and Wobbuffets are drawn
 * from the Poke Ball Pokemon models in the common item file, one model each drawn as many times
 * as needed. The camera is the match camera's debug free mode, as on the board. */
#include <math.h>
#include <string.h>

#include <melee/cm/camera.h>
#include <melee/cm/types.h>
#include <melee/ft/fighter.h>
#include <melee/ft/forward.h>
#include <melee/ft/ftanim.h>
#include <melee/ft/ftparts.h>
#include <melee/ft/inlines.h>
#include <melee/ft/ftcommon.h>
#include <melee/ft/kinds/ftCommon/ftCo_Fall.h>
#include <melee/ft/kinds/ftCommon/ftCo_HammerWait.h>
#include <melee/ft/kinds/ftCommon/ftpickupitem.h>
#include <melee/ft/types.h>
#include <melee/gm/gmvs.h>
#include <melee/gr/forward.h>
#include <melee/if/ifall.h>
#include <melee/it/forward.h>
#include <melee/it/itdrop.h>
#include <melee/it/types.h>
#include <melee/lb/lbaudio_ax.h>
#include <melee/pl/player.h>
#include <sysdolphin/baselib/cobj.h>
#include <sysdolphin/baselib/controller.h>
#include <sysdolphin/baselib/gobj.h>
#include <sysdolphin/baselib/gobjgxlink.h>
#include <sysdolphin/baselib/jobj.h>

#include "party.h"
#include "party_draw.h"
#include "party_hud.h"

#define MAX_SNORLAX 160      /* per lane, m407's MAX_WHOMPS_PLAYER */
#define PLAY_FRAMES 600      /* 10 seconds */
#define LANE_X0 (-60.0f)
#define LANE_STEP 40.0f
#define LANE_Z0 (-60.0f)     /* the first Snorlax of a lane */
#define LANE_DZ (-20.0f)     /* and each one after it */
#define LANE_END (LANE_Z0 + LANE_DZ * (MAX_SNORLAX + 4))
#define WOBBU_Z (-16.0f)
#define DROP_Y 100.0f
#define SNORLAX_SCALE 1.47f
#define WOBBU_SCALE 0.8f
#define WOBBU_YAW 0.0f       /* facing its player */
#define TIP_STEP 4.0f        /* degrees a Snorlax tips each frame (m407 whomp.c) */
#define TIP_NEXT (-25.0f)    /* the next one starts once this one passes this angle */
#define TIP_LEAN (-68.0f)    /* resting on the one behind it; the last of a lane too, so the one
                              * before it has its head under it like every other */
#define SWING_SPEED 4.0f     /* the hammer's animation rate for one swing */
#define SWING_FRAMES 7       /* and its length: 28 frames of the animation, one blow */
#define SFX_COIN 0xAA        /* the coin's ding, for every Snorlax counted */
#define SFX_THUD 0x61A87     /* a heavy item landing */
#define SFX_GAP 4            /* frames between two of the same sound: dozens at once distort */
#define LEAP_V0 3.0f         /* the lineup's jump into the sky: its first speed */
#define LEAP_ACCEL 0.35f     /* and how fast it speeds up */
#define LEAP_GONE 300.0f     /* out of sight above the lanes */
#define BIG_RISE 30          /* frames for the winners' Snorlax to come out of the ground */
#define BIG_GAP 15.0f        /* between the last one of the lane, leaning back, and it */
#define PARASOL_SCALE 1.25f
#define PARASOL_HANDLE 5.5f  /* the parasol's model: its handle's end below its origin */
#define PARASOL_ANIM 2       /* open: the last frame of its animation 2 (it/kinds/itparasol.c) */
#define PARASOL_FRAME 44.0f
#define PARASOL_HAND_X 0.37f /* the winner's Snorlax's hand, in its heights from its middle and feet */
#define PARASOL_HAND_Y 0.53f
#define PARASOL_HAND_Z (-0.186f) /* the hand's depth: its last arm joint (-5.74 at 30.9 tall) */
#define PARASOL_TILT (-0.26f) /* about 15 degrees, the top away from the Snorlax (toward +x) */
#define FLY_UP_FRAMES 30     /* the winners' jump off the top of the screen */
#define FLY_DROP 150.0f      /* how far above the Snorlax they come back down from */
#define WIN_WAIT 200         /* frames to wait for the winners to land and taunt, at most */

extern Camera game_camera;
extern ArticleSlot* it_804D6D30;   /* the Poke Ball Pokemon articles of ItCo.dat (it/iteffect.c) */
extern ArticleSlot* it_804D6D24;   /* the common items' articles, by item kind (it/it_3F14.c) */

enum State {
    ST_INTRO,   /* the camera sweeps in from the far end of the lanes */
    ST_READY,   /* "READY" then "START!" */
    ST_PLAY,    /* 10 seconds of swinging */
    ST_TURN,    /* the camera turns to look along the lanes */
    ST_COUNT,   /* the Snorlaxes fall over and the lanes count */
    ST_FINISH,  /* "FINISH!" */
    ST_WIN,     /* the winners, then the match ends */
    ST_END,
};

typedef struct Snorlax {
    float y, vy;     /* falling from the sky, then 0 */
    float tip;       /* degrees, 0 standing, down to TIP_LEAN */
    s8 tipping;
    s8 landed;
} Snorlax;

/* One of the lineup the lanes start with, until it jumps into the sky. */
typedef struct Leaper {
    float y, vy;
    s8 delay;        /* frames before it jumps */
    s8 gone;
} Leaper;

/* A winner on the way to the top of their Snorlax. */
enum Fly {
    FLY_NONE,
    FLY_UP,      /* jumping off the top of the screen */
    FLY_DOWN,    /* dropping from the sky onto the Snorlax */
    FLY_LANDED,  /* standing on it, about to taunt */
    FLY_TAUNTED,
};

/* A camera shot in m407's terms (Hu3D's Center, CRot in degrees and CZoom), in m407's units. */
typedef struct Shot {
    float cx, cy, cz;
    float rx, ry;
    float zoom;
} Shot;

typedef struct Vec3f {
    float x, y, z;
} Vec3f;

static struct {
    int state;
    int timer;       /* frames in the state */
    int frame;
    int count[PARTY_PLAYERS];
    int counted[PARTY_PLAYERS];  /* fallen over so far, during the count */
    Snorlax s[PARTY_PLAYERS][MAX_SNORLAX];
    int prev_a[PARTY_PLAYERS];
    int swing[PARTY_PLAYERS];    /* frames left of the current swing, 0 = at rest */
    int queued[PARTY_PLAYERS];   /* a press during a swing: the next swing, once this one lands */
    int squash[PARTY_PLAYERS];   /* the Wobbuffet's squash after a hit */
    int armed[PARTY_PLAYERS];    /* holding the hammer */
    /* CPUs press at m407's rates: one press every `interval` frames from `next`. */
    float cpu_next[PARTY_PLAYERS], cpu_interval[PARTY_PLAYERS];
    int thud_at, coin_at;        /* when each sound last played */
    int song;                    /* the hammer song's requests (lbAudioAx_80024FF4) in effect */
    int best;
    int winners;
    /* The camera: a move from one shot to another, eased as m407's camera does. */
    Shot from, to, cur;
    int move_len, move_t, move_mode;
    float count_front;           /* how far along the lanes the count is (m407 Center.z) */
    HSD_Text* text;
    int line_big, line_timer;
    int line_p[PARTY_PLAYERS];
    HSD_JObj* snorlax;
    HSD_JObj* wobbu[PARTY_PLAYERS];
    HSD_JObj* parasol;
    int paused;          /* the match was paused last frame */
    float snorlax_h;             /* a standing Snorlax's height, at SNORLAX_SCALE */
    Leaper lineup[PARTY_PLAYERS][MAX_SNORLAX];
    int lineup_left;             /* still in sight */
    /* The winners' Snorlaxes, rising out of the ground (y from -snorlax_h to 0). */
    float big_y[PARTY_PLAYERS];
    int fly[PARTY_PLAYERS];
    int fly_t[PARTY_PLAYERS];
    float fly_y[PARTY_PLAYERS], fly_v[PARTY_PLAYERS];   /* FLY_UP: how far above its jump it is
                                                         * drawn; FLY_DOWN: its height */
    int win_at;                  /* when "WINS!" went up, 0 = not yet */
} dm;

static Fighter* fighter(int slot)
{
    HSD_GObj* gobj = Player_GetEntity(slot);
    return gobj != NULL ? GET_FIGHTER(gobj) : NULL;
}

static float lane_x(int slot)
{
    return LANE_X0 + LANE_STEP * (float) slot;
}

static float snorlax_z(int n)
{
    return LANE_Z0 + LANE_DZ * (float) n;
}

/* Where a winner's Snorlax comes up: past the lane's last one, leaning back nearly its height. */
static float big_z(int slot)
{
    return snorlax_z(dm.count[slot] - 1) - dm.snorlax_h - BIG_GAP;
}

/* ---- the models ---- */

/* An item's model from its article: a Poke Ball Pokemon's (it_804D6D30) or a common item's
 * (it_804D6D24). */
static HSD_JObj* item_model_pose(int kind, int anim, float frame);

static HSD_JObj* item_model(int kind)
{
    return item_model_pose(kind, 0, 0.0f);
}

/* An item's model from its article, posed at `frame` of its animation `anim`. */
static HSD_JObj* item_model_pose(int kind, int anim, float frame)
{
    Article* article;
    ItemModelDesc* model;
    ItemStateArray* states;
    HSD_JObj* jobj;
    ArticleSlot* table = kind >= It_PKind_Start ? it_804D6D30 : it_804D6D24;
    if (table == NULL) {
        return NULL;
    }
    article = DP(table[kind >= It_PKind_Start ? kind - It_PKind_Start : kind]);
    if (article == NULL || (model = DP(article->x10_modelDesc)) == NULL || DP(model->x0_joint) == NULL) {
        party_log("domination: no model for item kind 0x%X", kind);
        return NULL;
    }
    jobj = HSD_JObjLoadJoint(DP(model->x0_joint));
    states = DP(article->xC_itemStates);
    if (jobj != NULL && kind == It_PKind_Sonans) {
        /* Wobbuffet's parts start hidden (its Poke Ball entrance shows them) and its first
         * animation is that entrance: its plain model is the one that stands there. */
        HSD_JObjClearFlagsAll(jobj, JOBJ_HIDDEN);
        states = NULL;
    }
    if (jobj != NULL && states != NULL && DP(states->x0_itemStateDesc[anim].x0_anim_joint) != NULL) {
        /* A frame of one of its animations (the first: standing, rather than the bind pose). */
        HSD_JObjAddAnimAll(jobj, DP(states->x0_itemStateDesc[anim].x0_anim_joint),
                           DP(states->x0_itemStateDesc[anim].x4_matanim_joint),
                           DP(states->x0_itemStateDesc[anim].x8_parameters));
        HSD_JObjReqAnimAll(jobj, frame);
        HSD_JObjAnimAll(jobj);
    }
    return jobj;
}

static void place(HSD_JObj* jobj, float x, float y, float z, float rot_x, float rot_y, float sx,
                  float sy)
{
    Vec3 t = { x, y, z };
    Vec3 sc = { sx, sy, sx };
    HSD_JObjSetTranslate(jobj, &t);
    HSD_JObjSetRotationX(jobj, rot_x);
    HSD_JObjSetRotationY(jobj, rot_y);
    HSD_JObjSetScale(jobj, &sc);
    HSD_JObjSetMtxDirty(jobj);
}

/* The highest joint of a placed model, in the world: parents before their children. */
static float joints_top(HSD_JObj* jobj, float top)
{
    for (; jobj != NULL; jobj = HSD_JObjGetNext(jobj)) {
        HSD_JObjSetupMatrix(jobj);
        if (jobj->mtx[1][3] > top) {
            top = jobj->mtx[1][3];
        }
        top = joints_top(HSD_JObjGetChild(jobj), top);
    }
    return top;
}

/* A standing Snorlax's height at SNORLAX_SCALE: its top joint, and a little for the head above
 * it. */
static float snorlax_height(void)
{
    float top;
    if (dm.snorlax == NULL) {
        return 30.0f;
    }
    place(dm.snorlax, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, SNORLAX_SCALE, SNORLAX_SCALE);
    top = joints_top(dm.snorlax, 0.0f) * 1.1f;
    party_log("domination: snorlax %.1f tall", top);
    return top > 1.0f ? top : 30.0f;
}

/* Drawn with the items, after the stage: every Wobbuffet, then every Snorlax near the camera. */
static void draw_models(HSD_GObj* gobj, intptr_t pass)
{
    u32 flags = HSD_GObj_80390EB8(pass);
    float near_z = -0.1f * (dm.cur.cz + 500.0f);   /* what the camera looks at */
    int i, n;
    (void) gobj;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        float squash = dm.squash[i] > 0 ? 1.0f - 0.3f * (float) dm.squash[i] / 8.0f : 1.0f;
        if (dm.wobbu[i] == NULL) {
            continue;
        }
        place(dm.wobbu[i], lane_x(i), 0.0f, WOBBU_Z, 0.0f, WOBBU_YAW, WOBBU_SCALE,
              WOBBU_SCALE * squash);
        HSD_JObjDispAll(dm.wobbu[i], NULL, flags, 0);
    }
    if (dm.snorlax == NULL) {
        return;
    }
    for (i = 0; i < PARTY_PLAYERS; i++) {
        for (n = 0; n < dm.count[i]; n++) {
            const Snorlax* s = &dm.s[i][n];
            float z = snorlax_z(n);
            /* Only the ones near what the camera looks at: lanes can hold 640 of them. */
            if (z > near_z + 400.0f || z < near_z - 400.0f) {
                continue;
            }
            place(dm.snorlax, lane_x(i), s->y, z, s->tip * (float) M_PI / 180.0f, 0.0f,
                  SNORLAX_SCALE, SNORLAX_SCALE);
            HSD_JObjDispAll(dm.snorlax, NULL, flags, 0);
        }
        for (n = 0; dm.lineup_left > 0 && n < MAX_SNORLAX; n++) {
            const Leaper* l = &dm.lineup[i][n];
            float z = snorlax_z(n);
            if (l->gone || z > near_z + 400.0f || z < near_z - 400.0f) {
                continue;
            }
            place(dm.snorlax, lane_x(i), l->y, z, 0.0f, 0.0f, SNORLAX_SCALE, SNORLAX_SCALE);
            HSD_JObjDispAll(dm.snorlax, NULL, flags, 0);
        }
        if (dm.state >= ST_WIN && (dm.winners & (1 << i))) {
            /* The winner's Snorlax, coming out of the ground. */
            place(dm.snorlax, lane_x(i), dm.big_y[i], big_z(i), 0.0f, (float) M_PI,
                  SNORLAX_SCALE, SNORLAX_SCALE);   /* turned to the camera */
            HSD_JObjDispAll(dm.snorlax, NULL, flags, 0);
            if (dm.parasol != NULL) {
                /* The parasol in its hand, open and leaning out away from it, its handle's end in
                 * the hand. It turns about its origin, PARASOL_HANDLE above that end, so the
                 * origin goes where the turned handle puts the end at the hand. (Unlike the Star
                 * Rod it is all opaque, with no billboard: it draws where it is put.) */
                float h = dm.snorlax_h, len = PARASOL_HANDLE * PARASOL_SCALE;
                float hand_x = lane_x(i) + PARASOL_HAND_X * h, hand_y = dm.big_y[i] + PARASOL_HAND_Y * h;
                place(dm.parasol, hand_x - len * sinf(PARASOL_TILT), hand_y + len * cosf(PARASOL_TILT),
                      big_z(i) + PARASOL_HAND_Z * h, 0.0f, 0.0f, PARASOL_SCALE, PARASOL_SCALE);
                HSD_JObjSetRotationZ(dm.parasol, PARASOL_TILT);
                HSD_JObjDispAll(dm.parasol, NULL, flags, 0);
            }
        }
    }
}

/* The field: grass, a lane of paving per player, and a sky far down the lanes. */
static void draw_field(void)
{
    static const GXColor grass = { 96, 170, 72, 255 };
    static const GXColor lane = { 214, 190, 140, 255 };
    static const GXColor lane_dark = { 190, 165, 118, 255 };
    static const GXColor sky = { 120, 190, 250, 255 };
    float c[8];
    int i, n;

    c[0] = -400.0f; c[1] = 40.0f;
    c[2] = 400.0f;  c[3] = 40.0f;
    c[4] = 400.0f;  c[5] = LANE_END - 400.0f;
    c[6] = -400.0f; c[7] = LANE_END - 400.0f;
    party_draw_floor_quad(grass, 0.05f, c);
    for (i = 0; i < PARTY_PLAYERS; i++) {
        float x0 = lane_x(i) - 11.0f, x1 = lane_x(i) + 11.0f;
        for (n = 0; n < MAX_SNORLAX + 2; n++) {   /* from the first Snorlax's square */
            float z0 = snorlax_z(n) + 10.0f, z1 = z0 + LANE_DZ;
            c[0] = x0; c[1] = z0;
            c[2] = x1; c[3] = z0;
            c[4] = x1; c[5] = z1;
            c[6] = x0; c[7] = z1;
            party_draw_floor_quad(n & 1 ? lane_dark : lane, 0.1f, c);
        }
    }
    party_draw_quad(sky, -2000.0f, -50.0f, 2000.0f, 1500.0f, LANE_END - 400.0f);
}

/* ---- the camera ---- */

static void shot_set(Shot* s, float cx, float cy, float cz, float rx, float ry, float zoom)
{
    s->cx = cx;
    s->cy = cy;
    s->cz = cz;
    s->rx = rx;
    s->ry = ry;
    s->zoom = zoom;
}

/* m407 camera.c fn_1_1DB0: move from where the camera is to `to` over `frames`. */
static void camera_move(const Shot* to, int frames, int mode)
{
    dm.from = dm.cur;
    dm.to = *to;
    dm.move_len = frames;
    dm.move_t = 0;
    dm.move_mode = mode;
    if (frames <= 0) {
        dm.cur = *to;
        dm.move_len = 0;
    }
}

static void camera_frame(void)
{
    Vec3f eye, look;
    float rx, ry;
    HSD_CObj* cobj;

    if (dm.move_len > 0) {
        float t = (float) ++dm.move_t / (float) dm.move_len;
        float k = dm.move_mode == 0 ? t : sinf(t * (float) M_PI * 0.5f);   /* m407 fn_1_2024 */
        dm.cur.cx = dm.from.cx + (dm.to.cx - dm.from.cx) * k;
        dm.cur.cy = dm.from.cy + (dm.to.cy - dm.from.cy) * k;
        dm.cur.cz = dm.from.cz + (dm.to.cz - dm.from.cz) * k;
        dm.cur.rx = dm.from.rx + (dm.to.rx - dm.from.rx) * k;
        dm.cur.ry = dm.from.ry + (dm.to.ry - dm.from.ry) * k;
        dm.cur.zoom = dm.from.zoom + (dm.to.zoom - dm.from.zoom) * k;
        if (dm.move_t >= dm.move_len) {
            dm.move_len = 0;
        }
    }
    /* Hu3D's camera: the eye sits zoom away from Center, turned by CRot. Then into this world:
     * x0.1, x mirrored, z mirrored about the players' line (m407 z = -500). */
    rx = dm.cur.rx * (float) M_PI / 180.0f;
    ry = dm.cur.ry * (float) M_PI / 180.0f;
    look.x = -0.1f * dm.cur.cx;
    look.y = 0.1f * dm.cur.cy;
    look.z = -0.1f * (dm.cur.cz + 500.0f);
    eye.x = -0.1f * (dm.cur.cx + sinf(ry) * cosf(rx) * dm.cur.zoom);
    eye.y = 0.1f * (dm.cur.cy - sinf(rx) * dm.cur.zoom);
    eye.z = -0.1f * (dm.cur.cz + cosf(ry) * cosf(rx) * dm.cur.zoom + 500.0f);

    if (!Camera_80030178()) {
        Camera_8003006C();
    }
    cm_80453004.free_int_pos.x = look.x;
    cm_80453004.free_int_pos.y = look.y;
    cm_80453004.free_int_pos.z = look.z;
    cm_80453004.free_eye_pos.x = eye.x;
    cm_80453004.free_eye_pos.y = eye.y;
    cm_80453004.free_eye_pos.z = eye.z;
    cm_80453004.free_fov = 45.0f;   /* Hu3DCameraPerspectiveSet(1, 45.0f, ...) */
    cobj = game_camera.gobj != NULL ? game_camera.gobj->hsd_obj : NULL;
    if (cobj != NULL) {
        HSD_CObjSetNear(cobj, 1.0f);
        HSD_CObjSetFar(cobj, 16384.0f);
    }
}

/* ---- the game ---- */

static void say(const char* text, GXColor color)
{
    party_hud_set(dm.text, dm.line_big, "%s", text);
    party_hud_color(dm.text, dm.line_big, color);
}

/* Only during the count: nobody sees the scores while they swing. */
static void show_count(int slot)
{
    party_hud_set(dm.text, dm.line_p[slot], "P%d  %d", slot + 1, dm.counted[slot]);
}

/* One of each sound every SFX_GAP frames at most, however many Snorlaxes land or count. */
static void sfx(int id, int vol, int* last)
{
    if (*last != 0 && dm.frame - *last < SFX_GAP) {
        return;
    }
    *last = dm.frame;
    lbAudioAx_800237A8(id, vol, 0x40);
}

/* The hammer's wait animation is the swing: play it once from the start. */
static void swing(int slot)
{
    Fighter* fp = fighter(slot);
    dm.squash[slot] = 8;
    if (fp != NULL && dm.armed[slot]) {
        Fighter_ChangeMotionState(fp->gobj, ftCo_MS_HammerWait, ftCo_800C54C4(fp), 0.0f,
                                  SWING_SPEED, 0.0f, NULL);
        ftCo_800C4E94(fp);
        dm.swing[slot] = SWING_FRAMES;
    }
}

/* One press: a Snorlax for this lane, and a swing at the Wobbuffet. A press while the hammer is
 * still coming down waits for it to land, so every swing reaches the Wobbuffet. */
static void pound(int slot)
{
    if (dm.count[slot] < MAX_SNORLAX) {
        Snorlax* s = &dm.s[slot][dm.count[slot]++];
        memset(s, 0, sizeof *s);
        s->y = DROP_Y;
    }
    if (dm.swing[slot] > 0) {
        dm.queued[slot] = 1;
    } else {
        swing(slot);
    }
}

static void give_hammer(int slot)
{
    Fighter* fp = fighter(slot);
    Item_GObj* hammer;
    Vec3 pos;
    if (fp == NULL || dm.armed[slot] || fp->ground_or_air != GA_Ground || fp->item_gobj != NULL) {
        return;
    }
    pos = fp->cur_pos;
    hammer = it_8026F5C8(NULL, It_Kind_Hammer, &pos);
    if (hammer == NULL) {
        party_log("domination: no hammer for P%d", slot + 1);
        return;
    }
    ftpickupitem_800948A8(fp->gobj, hammer);
    ftCo_800C52F4(fp->gobj);
    /* Picking it up starts the hammer song (ft_800880AC), which lasts 8 seconds from here: it
     * waits for the 10 seconds of swinging instead (hammer_song). */
    lbAudioAx_80025038(fp->x2168);
    fp->x2168 = 0;
    ftAnim_SetAnimRate(fp->gobj, 0.0f);   /* at rest until the first press */
    dm.armed[slot] = 1;
}

static int sum_counts(void)
{
    int i, n = 0;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        n += dm.count[i];
    }
    return n;
}

static void decide(void)
{
    int i;
    dm.best = 0;
    dm.winners = 0;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        if (dm.count[i] > dm.best) {
            dm.best = dm.count[i];
        }
    }
    for (i = 0; i < PARTY_PLAYERS; i++) {
        if (dm.best > 0 && dm.count[i] == dm.best) {
            dm.winners |= 1 << i;
        }
        dm.big_y[i] = -dm.snorlax_h;   /* under the ground until the end */
    }
    party_log("domination: snorlax %d %d %d %d", dm.count[0], dm.count[1], dm.count[2], dm.count[3]);
}

/* The dominoes: in every lane the next one starts falling once the one before it passes
 * TIP_NEXT, and each one counts as it settles. Returns 1 once every lane is done. */
static int count_frame(void)
{
    int i, done = 1;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        int n;
        for (n = 0; n < dm.count[i]; n++) {
            Snorlax* s = &dm.s[i][n];
            float rest = TIP_LEAN;
            if (!s->tipping) {
                if (n == 0 || dm.s[i][n - 1].tip <= TIP_NEXT) {
                    s->tipping = 1;
                } else {
                    break;
                }
            }
            if (s->tip > rest) {
                s->tip -= TIP_STEP;
                if (s->tip <= rest) {
                    s->tip = rest;
                    dm.counted[i] = n + 1;
                    show_count(i);
                    sfx(SFX_COIN, 0x7F, &dm.coin_at);
                }
            }
        }
        if (dm.counted[i] < dm.count[i]) {
            done = 0;
        }
    }
    return done;
}

/* A winner: up off the top of the screen (m407 player.c fn_1_1074), then down from the sky onto
 * their Snorlax (fn_1_11CC, whose gravity this is, x0.1). Landing is the fighter's own: its
 * collision stays on the floor below while it is drawn on top of the Snorlax (dom_fighter_map,
 * dom_fighter_drawn). */
static void fly_frame(int slot)
{
    Fighter* fp = fighter(slot);
    switch (dm.fly[slot]) {
    case FLY_UP:
        dm.fly_v[slot] += 0.6f;
        dm.fly_y[slot] += dm.fly_v[slot];
        if (++dm.fly_t[slot] >= FLY_UP_FRAMES) {
            dm.fly[slot] = FLY_DOWN;
            dm.fly_y[slot] = dm.snorlax_h + FLY_DROP;
            dm.fly_v[slot] = 0.0f;
        }
        break;
    case FLY_DOWN:
        dm.fly_v[slot] += 0.3f * 0.98f;
        dm.fly_y[slot] -= 0.2f + dm.fly_v[slot];
        if (fp != NULL && fp->ground_or_air == GA_Ground && dm.fly_y[slot] <= dm.snorlax_h) {
            dm.fly[slot] = FLY_LANDED;
        }
        break;
    default:
        break;
    }
}

/* The hammer song for the 10 seconds of swinging: it plays while it has a request in effect and
 * its countdown (8 seconds from the last request) runs, so a second request halfway keeps it to
 * the end; then every request is taken back. */
static void hammer_song(int on)
{
    if (on) {
        lbAudioAx_80024FF4();
        dm.song++;
    } else if (dm.song > 0) {
        lbAudioAx_80025038(dm.song);
        dm.song = 0;
    }
}

static void set_state(int state)
{
    dm.state = state;
    dm.timer = 0;
}

static void dom_frame(void)
{
    static const GXColor white = { 255, 255, 255, 255 };
    static const GXColor gold = { 255, 210, 60, 255 };
    Shot shot;
    int i, n, paused = party_paused();

    if (paused != dm.paused) {
        if (!paused) {
            ifAll_802F3394();   /* unpausing brings back the damage percents and stocks */
        }
        dm.paused = paused;
    }
    if (paused) {
        return;   /* everything holds still, as the match does; the camera stays where it is */
    }
    dm.frame++;
    if (dm.frame == 1) {
        /* Start pauses only once the match's HUD is on, which "GO!" does; Domination hides that
         * HUD before it, so it turns pausing on itself. */
        gmVs_GetSceneController()->state.hud_enabled = 1;
    }
    dm.timer++;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        Fighter* fp = fighter(i);
        if (dm.squash[i] > 0) {
            dm.squash[i]--;
        }
        if (fp == NULL || !dm.armed[i]) {
            continue;
        }
        fp->x2330.x = 600;   /* the hammer never runs out, and its head never flies off */
        fp->x2330.y = 0;
        if (dm.swing[i] > 0 && --dm.swing[i] == 0) {
            if (dm.queued[i]) {
                dm.queued[i] = 0;
                swing(i);
            } else {
                ftAnim_SetAnimRate(fp->gobj, 0.0f);
            }
        }
    }
    /* Snorlaxes falling from the sky (m407 whomp gravity, x0.1). */
    for (i = 0; i < PARTY_PLAYERS; i++) {
        for (n = 0; n < dm.count[i]; n++) {
            Snorlax* s = &dm.s[i][n];
            if (s->landed) {
                continue;
            }
            s->vy += 0.3f * 0.98f;
            s->y -= 0.2f + s->vy;
            if (s->y <= 0.0f) {
                s->y = 0.0f;
                s->landed = 1;
                sfx(SFX_THUD, 0x60, &dm.thud_at);
            }
        }
    }
    /* The lineup jumping into the sky, from READY on. */
    for (i = 0; dm.state >= ST_READY && dm.lineup_left > 0 && i < PARTY_PLAYERS; i++) {
        for (n = 0; n < MAX_SNORLAX; n++) {
            Leaper* l = &dm.lineup[i][n];
            if (l->gone) {
                continue;
            }
            if (l->delay > 0) {
                l->delay--;
                continue;
            }
            l->y += l->vy;
            l->vy += LEAP_ACCEL;
            if (l->y > LEAP_GONE) {
                l->gone = 1;
                dm.lineup_left--;
            }
        }
    }

    switch (dm.state) {
    case ST_INTRO:
        if (dm.timer == 1) {
            shot_set(&shot, 437.0f, 0.0f, 5000.0f, -50.0f, -360.0f, 2290.0f);
            camera_move(&shot, 100, 1);
        } else if (dm.timer == 101) {
            shot_set(&shot, 0.0f, 0.0f, 100.0f, -45.0f, -540.0f, 1910.0f);
            camera_move(&shot, 110, 2);
        } else if (dm.timer > 212) {
            set_state(ST_READY);
            say("READY", white);
        }
        break;
    case ST_READY:
        for (i = 0; i < PARTY_PLAYERS; i++) {
            give_hammer(i);
        }
        if (dm.timer == 1) {
            sfx(SFX_THUD, 0x7F, &dm.thud_at);   /* the lineup takes off */
        } else if (dm.timer == 60) {
            say("START!", gold);
        } else if (dm.timer >= 100) {
            say("", white);
            set_state(ST_PLAY);
            hammer_song(1);
            party_hud_set(dm.text, dm.line_timer, "%d", PLAY_FRAMES / 60);
        }
        break;
    case ST_PLAY: {
        int left = PLAY_FRAMES - dm.timer;
        party_hud_set(dm.text, dm.line_timer, "%d", (left + 59) / 60);
        if (dm.timer == PLAY_FRAMES / 2) {
            hammer_song(1);   /* its countdown would end before the swinging does */
        }
        if (left <= 0) {
            hammer_song(0);
            party_hud_set(dm.text, dm.line_timer, "");
            decide();
            if (sum_counts() > 0) {
                shot_set(&shot, 0.0f, 0.0f, 100.0f, -53.0f, -680.0f, 1900.0f);
                camera_move(&shot, 180, 4);
                set_state(ST_TURN);
            } else {
                set_state(ST_FINISH);
            }
        }
        break;
    }
    case ST_TURN:
        if (dm.timer == 50) {
            /* m407 fn_1_28C: the hammers are put away. The hammer's own timer runs out. */
            for (i = 0; i < PARTY_PLAYERS; i++) {
                Fighter* fp = fighter(i);
                if (fp != NULL && dm.armed[i]) {
                    fp->x2330.x = 1;
                }
                dm.armed[i] = 0;
                dm.swing[i] = dm.queued[i] = 0;
            }
        }
        if (dm.timer >= 190) {
            dm.count_front = dm.cur.cz;
            set_state(ST_COUNT);
            for (i = 0; i < PARTY_PLAYERS; i++) {
                show_count(i);
            }
        }
        break;
    case ST_COUNT:
        /* The camera keeps pace with the dominoes (m407: Center.z += 33.5 a frame). */
        if (count_frame()) {
            if (dm.timer > 15) {
                set_state(ST_FINISH);
            }
        } else {
            dm.timer = 0;
            dm.cur.cz += 33.5f;
        }
        break;
    case ST_FINISH:
        if (dm.timer == 1) {
            say("FINISH!", gold);
        } else if (dm.timer >= 90) {
            /* m407 fn_1_5804: frame the winners' lanes, at the end of the longest: on the
             * winners' Snorlaxes (m407's Center.z from this world's z). */
            float lo = -1.0f, hi = -1.0f, z = 0.0f;
            static const float zoom_by_spread[4] = { 850.0f, 900.0f, 1300.0f, 1500.0f };
            for (i = 0; i < PARTY_PLAYERS; i++) {
                if (dm.winners & (1 << i)) {
                    if (lo < 0.0f) {
                        lo = 400.0f * (float) i;
                        z = big_z(i);
                    }
                    hi = 400.0f * (float) i;
                }
            }
            if (lo >= 0.0f) {
                /* m407's height and distance are for its 30-tall Whomp: scaled to the Snorlax. */
                float k = dm.snorlax_h / 30.0f < 1.0f ? 1.0f : dm.snorlax_h / 30.0f;
                shot_set(&shot, 600.0f - 0.5f * (lo + hi), 250.0f * k, -10.0f * z - 500.0f,
                         -42.0f, -720.0f, k * zoom_by_spread[(int) ((hi - lo) / 400.0f)]);
                camera_move(&shot, 60, 4);
            }
            say("", white);
            set_state(ST_WIN);
        }
        break;
    case ST_WIN: {
        /* m407 fn_1_53B8: at 40 the winners' Snorlaxes come up and the winners jump off; once
         * they have landed on them and taunted, the result. */
        int ready = 1;
        if (dm.timer == 40) {
            for (i = 0; i < PARTY_PLAYERS; i++) {
                if (dm.winners & (1 << i)) {
                    dm.fly[i] = FLY_UP;
                }
            }
            if (dm.winners != 0) {
                sfx(SFX_THUD, 0x7F, &dm.thud_at);
            }
        }
        for (i = 0; dm.timer > 40 && i < PARTY_PLAYERS; i++) {
            if (!(dm.winners & (1 << i))) {
                continue;
            }
            if (dm.big_y[i] < 0.0f) {
                dm.big_y[i] += dm.snorlax_h / (float) BIG_RISE;
                if (dm.big_y[i] > 0.0f) {
                    dm.big_y[i] = 0.0f;
                }
            }
            fly_frame(i);
            if (dm.fly[i] != FLY_TAUNTED) {
                ready = 0;
            }
        }
        if (dm.win_at == 0 && dm.timer >= 40 && (ready || dm.timer >= 40 + WIN_WAIT)) {
            char line[64];
            int len = 0;
            dm.win_at = dm.timer;
            if (dm.winners == 0) {
                say("DRAW", white);
            } else {
                for (i = 0; i < PARTY_PLAYERS; i++) {
                    if (dm.winners & (1 << i)) {
                        line[len++] = 'P';
                        line[len++] = (char) ('1' + i);
                        line[len++] = ' ';
                    }
                }
                memcpy(line + len, "WINS!", 6);
                say(line, gold);
            }
        } else if (dm.win_at != 0 && dm.timer >= dm.win_at + 170) {
            set_state(ST_END);
            gm_8016B328();
        }
        break;
    }
    default:
        break;
    }
    camera_frame();
}

static void dom_start(void)
{
    static const GXColor port[PARTY_PLAYERS] = {
        { 255, 80, 80, 255 }, { 90, 150, 255, 255 }, { 255, 210, 60, 255 }, { 80, 220, 110, 255 },
    };
    Shot shot;
    HSD_GObj* gobj;
    int i;

    ifAll_802F3394();   /* no damage percents or stocks */
    party_hud_init();
    dm.text = party_hud_text();
    dm.line_big = party_hud_line(dm.text, 0.0f, -40.0f, 1.6f, PARTY_WHITE);
    dm.line_timer = party_hud_line(dm.text, 0.0f, -200.0f, 1.2f, PARTY_WHITE);
    for (i = 0; i < PARTY_PLAYERS; i++) {
        dm.line_p[i] = party_hud_line(dm.text, -225.0f + 150.0f * (float) i, 200.0f, 0.6f, port[i]);
    }

    dm.snorlax = item_model(It_PKind_Kabigon);
    dm.snorlax_h = snorlax_height();
    for (i = 0; i < PARTY_PLAYERS; i++) {
        dm.wobbu[i] = item_model(It_PKind_Sonans);
    }
    dm.parasol = item_model_pose(It_Kind_Parasol, PARASOL_ANIM, PARASOL_FRAME);
    /* The lanes start full, as m407's do; each one jumps a few frames from its neighbours. */
    for (i = 0; i < PARTY_PLAYERS; i++) {
        int n;
        for (n = 0; n < MAX_SNORLAX; n++) {
            dm.lineup[i][n].vy = LEAP_V0;
            dm.lineup[i][n].delay = (s8) ((7 * n + 3 * i) % 9);
        }
    }
    dm.lineup_left = PARTY_PLAYERS * MAX_SNORLAX;
    gobj = GObj_Create(14, 15, 0);
    if (gobj != NULL) {
        GObj_SetupGXLink(gobj, draw_models, 6, 0);
    }
    party_draw_init(draw_field);

    shot_set(&shot, 540.0f, 0.0f, 10000.0f, -50.0f, -360.0f, 2670.0f);   /* m407 main.c */
    camera_move(&shot, 0, 0);
    camera_frame();
}

static void dom_setup(StartMeleeData* start)
{
    int i;
    memset(&dm, 0, sizeof dm);
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.x4_0 = false;   /* no pause camera: Domination keeps its own */
    start->rules.x1_2 = true;  /* no "Ready... GO!": the minigame shows its own start */
    start->rules.x30 = 0.0f;   /* nobody is hurt by a stray hammer */
    start->rules.on_match_start = dom_start;
    start->rules.on_frame_start = dom_frame;
    /* m407 player.c: a CPU of difficulty d pounds (rate[d] - rand(spread[d])) times in 10 s. */
    for (i = 0; i < PARTY_PLAYERS; i++) {
        static const u8 rate[4][2] = { { 60, 15 }, { 80, 15 }, { 100, 20 }, { 120, 20 } };
        int level = party.p[i].cpu_level, d = level <= 3 ? 0 : level <= 5 ? 1 : level <= 7 ? 2 : 3;
        dm.cpu_interval[i] = 600.0f / (float) (rate[d][0] - party_rand(rate[d][1]));
        dm.cpu_next[i] = dm.cpu_interval[i];
    }
}

/* Nobody moves or attacks: every input is dropped. A press of A during the 10 seconds is a
 * swing (a CPU "presses" at its rate). */
static void dom_fighter_input(Fighter* fp)
{
    int slot = fp->player_idx;
    int a = (fp->input.held_buttons[0] & HSD_PAD_A) != 0;

    fp->input.lstick[0].x = fp->input.lstick[0].y = 0.0f;
    fp->input.cstick[0].x = fp->input.cstick[0].y = 0.0f;
    fp->input.triggers[0] = 0.0f;
    fp->input.held_buttons[0] = 0;
    /* Nana (the Ice Climbers' second climber) shares Popo's slot and repeats his presses a few
     * frames late: only Popo's count. */
    if (slot < 0 || slot >= PARTY_PLAYERS || fp->is_sub_fighter) {
        return;
    }
    if (dm.state == ST_PLAY) {
        if (party.p[slot].slot_type == Gm_PKind_Cpu) {
            if ((float) dm.timer >= dm.cpu_next[slot]) {
                dm.cpu_next[slot] += dm.cpu_interval[slot];
                pound(slot);
            }
        } else if (a && !dm.prev_a[slot]) {
            pound(slot);
        }
    }
    dm.prev_a[slot] = a;
    switch (dm.fly[slot]) {
    case FLY_UP:
        if (dm.fly_t[slot] <= 4) {
            fp->input.held_buttons[0] = HSD_PAD_X;   /* a jump, off the top of the screen */
        }
        break;
    case FLY_DOWN:
        if (fp->ground_or_air == GA_Ground) {   /* back down from that jump already */
            ftCommon_8007D5D4(fp);
            ftCo_Fall_Enter(fp->gobj);
        }
        break;
    case FLY_LANDED:
        if (fp->ground_or_air == GA_Ground && fp->motion_id == ftCo_MS_Wait) {
            fp->input.held_buttons[0] = HSD_PAD_DPADUP;   /* the taunt */
            dm.fly[slot] = FLY_TAUNTED;
        }
        break;
    default:
        break;
    }
}

/* Before collision: everyone stands at their lane, facing down it, away from the camera. */
static void dom_fighter_map(Fighter* fp)
{
    int slot = fp->player_idx;
    if (slot < 0 || slot >= PARTY_PLAYERS) {
        return;
    }
    fp->cur_pos.x = lane_x(slot);
    fp->cur_pos.z = dm.fly[slot] >= FLY_DOWN ? big_z(slot) : 0.0f;
    ftPartSetRotY(fp, 0, (float) M_PI);
    if (dm.fly[slot] == FLY_DOWN && fp->ground_or_air == GA_Air) {
        /* Its collision drops to the floor as its model drops to the top of the Snorlax. */
        fp->cur_pos.y = dm.fly_y[slot] - dm.snorlax_h;
        fp->self_vel.x = 0.0f;
        fp->self_vel.y = -(0.2f + dm.fly_v[slot]);
    }
}

/* After collision: a winner is drawn above where it collides, off the top of the screen and then
 * on top of its Snorlax. */
static void dom_fighter_drawn(Fighter* fp)
{
    int slot = fp->player_idx;
    Vec3 pos;
    if (slot < 0 || slot >= PARTY_PLAYERS || dm.fly[slot] == FLY_NONE) {
        return;
    }
    pos = fp->cur_pos;
    pos.y += dm.fly[slot] == FLY_UP ? dm.fly_y[slot] : dm.snorlax_h;
    HSD_JObjSetTranslate(fp->gobj->hsd_obj, &pos);
}

static void dom_result(s8 place[PARTY_PLAYERS])
{
    int i, j;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        place[i] = 0;
        for (j = 0; j < PARTY_PLAYERS; j++) {
            if (dm.count[j] > dm.count[i]) {
                place[i]++;
            }
        }
    }
}

const PartyMinigame mg_domination = {
    "Domination", "domination", dom_setup, dom_fighter_input, dom_result, 0, NULL,
    St_Kind_Last, -1, dom_fighter_map, dom_fighter_drawn, NULL, 1,
};
