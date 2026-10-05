/* Melee Party minigame: Bag Bash. Everyone against the Sandbags on Final Destination for a minute.
 * Every point of damage dealt to a bag scores a point for whoever dealt it, and knocking it off
 * the stage scores KO_BONUS. A bag comes back after a KO (a timed match), and the players can of
 * course hit each other out of the way. The bags take half the knockback, so knocking one out is
 * hard. With GOLD_SECONDS left a yellow Sandbag drops in, worth GOLD_MUL times the points.
 *
 * (Home-Run Contest's stage and HUD were the first idea, but its distance counter and barrier
 * depend on running inside the Home-Run Contest mode itself.)
 *
 * The bags are the fifth and sixth players: the Sandbag, CPU kind 0xF as Home-Run Contest sets it
 * up. A match has six fighter slots and the party takes four, so there are two bags: the yellow
 * one sleeps (ftCo_800BFD04: hidden, out of the camera, not updated) until its time. Damage is
 * credited to a bag's last attacker (its damage source, fp->dmg.x1868_source). */
#include <math.h>
#include <string.h>

#include <melee/cm/forward.h>
#include <melee/cm/types.h>
#include <melee/ft/forward.h>
#include <melee/ft/fighter.h>
#include <melee/ft/ftcolanim.h>
#include <melee/ft/ftcommon.h>
#include <melee/ft/inlines.h>
#include <melee/ft/kinds/ftCommon/ftCo_0A01.h>
#include <melee/ft/kinds/ftCommon/ftCo_Fall.h>
#include <melee/ft/types.h>
#include <melee/gm/gm_1601.h>
#include <melee/gm/gmvs.h>
#include <melee/gr/forward.h>
#include <melee/pl/player.h>
#include <sysdolphin/baselib/controller.h>
#include <sysdolphin/baselib/gobj.h>

#include "party.h"
#include "party_hud.h"

#define BASH_SECONDS 60
#define GOLD_SECONDS 10      /* left on the clock when the yellow bag comes in */
#define GOLD_MUL 3
#define BAGS 2
#define BAG_SLOT 4           /* the first bag; the yellow one is the next slot */
#define KO_BONUS 40
#define BAG_KNOCKBACK 0.5f
#define EDGE_X 82.0f         /* a bag past this is off the stage: the CPUs wait for it */
#define OUT_X 110.0f
#define OUT_BELOW (-60.0f)
#define OUT_ABOVE 160.0f

static const GXColor gold_tint = { 255, 200, 20, 200 };

typedef struct Bag {
    float last_percent;
    int last_hitter;
    int active;
    int mul;
    CmSubjectState camera;   /* its camera framing, while it sleeps */
} Bag;

static struct {
    int score[PARTY_PLAYERS];
    Bag bag[BAGS];
    int frame;
    int msg_until;
    HSD_Text* text;
    int line_msg;
    int line_p[PARTY_PLAYERS];
} bb;

static Fighter* fighter(int slot)
{
    HSD_GObj* gobj = Player_GetEntity(slot);
    return gobj != NULL ? GET_FIGHTER(gobj) : NULL;
}

static int slot_of(HSD_GObj* gobj)
{
    int i;
    if (gobj == NULL) {
        return -1;
    }
    for (i = 0; i < PARTY_PLAYERS; i++) {
        if (Player_GetEntity(i) == gobj) {
            return i;
        }
    }
    return -1;
}

static void show(int slot)
{
    party_hud_set(bb.text, bb.line_p[slot], "P%d  %d", slot + 1, bb.score[slot]);
}

static void message(const char* text, int frames)
{
    party_hud_set(bb.text, bb.line_msg, "%s", text);
    bb.msg_until = bb.frame + frames;
}

/* Back above the stage at 0%. The position is set as the replay code's spawn correction does
 * (shim/mu_replay.c): the fighter, its collision and the player slot. */
static void bag_reset(int b, float x, float y)
{
    Fighter* fp = fighter(BAG_SLOT + b);
    Vec3 pos;
    if (fp == NULL) {
        return;
    }
    pos.x = x;
    pos.y = y;
    pos.z = 0.0f;
    fp->cur_pos = pos;
    fp->coll_data.cur_pos = pos;
    fp->coll_data.last_pos = pos;
    fp->self_vel.x = fp->self_vel.y = fp->self_vel.z = 0.0f;
    fp->x8c_kb_vel.x = fp->x8c_kb_vel.y = fp->x8c_kb_vel.z = 0.0f;
    fp->dmg.x1830_percent = 0.0f;
    Player_80032828(fp->player_idx, fp->is_sub_fighter, &pos);
    bb.bag[b].last_percent = 0.0f;
}

/* The yellow bag: asleep from the start, awake with GOLD_SECONDS left. */
static void gold_sleep(void)
{
    Fighter* fp = fighter(BAG_SLOT + 1);
    if (fp == NULL) {
        return;
    }
    bb.bag[1].camera = fp->x890_cameraBox != NULL ? fp->x890_cameraBox->state : CmSubjectState_Active;
    ftCo_800BFD04(fp->gobj);
}

/* Yellow: the tint fighters get from a sub color (team shading, ftmaterial.c). Sub color 5 is
 * one no VS fighter has (only an event flag gives it), so its entry in the table is made yellow
 * for the yellow bag while Bag Bash lasts, and put back after. */
#define GOLD_SUB_COLOR 5
static GXColor saved_sub_color;
static int sub_color_saved;

static void gold_color(Fighter* fp)
{
    GXColor* entry = &p_ftCommonData->sub_colors[GOLD_SUB_COLOR - 1];
    if (!sub_color_saved) {
        saved_sub_color = *entry;
        sub_color_saved = 1;
    }
    entry->r = gold_tint.r;
    entry->g = gold_tint.g;
    entry->b = gold_tint.b;
    entry->a = gold_tint.a;
    fp->sub_color = GOLD_SUB_COLOR;
}

static void gold_color_restore(void)
{
    if (sub_color_saved) {
        p_ftCommonData->sub_colors[GOLD_SUB_COLOR - 1] = saved_sub_color;
        sub_color_saved = 0;
    }
}

static void gold_wake(void)
{
    Fighter* fp = fighter(BAG_SLOT + 1);
    if (fp == NULL) {
        return;
    }
    /* A motion change ends the sleep (and its hiding); then it drops in from above. */
    ftCommon_8007D5D4(fp);
    ftCo_Fall_Enter(fp->gobj);
    if (fp->x890_cameraBox != NULL) {
        fp->x890_cameraBox->state = bb.bag[1].camera;
    }
    bb.bag[1].active = 1;
    gold_color(fp);
    bag_reset(1, 30.0f, 70.0f);
    message("YELLOW SANDBAG - 3x POINTS!", 150);
    party_log("bag bash: the yellow bag comes in");
}

static void bash_start(void)
{
    static const GXColor port[PARTY_PLAYERS] = {
        { 255, 80, 80, 255 }, { 90, 150, 255, 255 }, { 255, 210, 60, 255 }, { 80, 220, 110, 255 },
    };
    int i;
    party_hud_init();
    bb.text = party_hud_text();
    bb.line_msg = party_hud_line(bb.text, 0.0f, -110.0f, 0.7f, PARTY_GOLD);
    message("BAG BASH  -  hit the Sandbag!", 150);
    for (i = 0; i < PARTY_PLAYERS; i++) {
        bb.line_p[i] = party_hud_line(bb.text, -225.0f + 150.0f * (float) i, -150.0f, 0.6f, port[i]);
        show(i);
    }
}

/* One bag's frame: a KO off the stage, or the damage dealt to it since the last frame. */
static void bag_frame(int b)
{
    Bag* bag = &bb.bag[b];
    Fighter* fp = fighter(BAG_SLOT + b);
    float pct;
    int by;
    if (fp == NULL || !bag->active) {
        return;
    }
    /* The Sandbag never dies on the blast zones; out of the stage is a KO here. */
    if (fabsf(fp->cur_pos.x) > OUT_X || fp->cur_pos.y < OUT_BELOW || fp->cur_pos.y > OUT_ABOVE) {
        if (bag->last_hitter >= 0) {
            static char line[48];
            bb.score[bag->last_hitter] += KO_BONUS * bag->mul;
            show(bag->last_hitter);
            line[0] = 'P';
            line[1] = (char) ('1' + bag->last_hitter);
            memcpy(line + 2, " KNOCKED IT OUT!", 17);
            message(line, 120);
            party_log("bag bash: KO of bag %d by P%d", b, bag->last_hitter + 1);
        }
        bag->last_hitter = -1;
        bag_reset(b, b == 0 ? 0.0f : 30.0f, 40.0f);
        return;
    }
    pct = fp->dmg.x1830_percent;
    by = slot_of(fp->dmg.x1868_source);
    if (pct > bag->last_percent && by >= 0) {
        bb.score[by] += (int) ((pct - bag->last_percent) * (float) bag->mul + 0.5f);
        bag->last_hitter = by;
        show(by);
    }
    bag->last_percent = pct;
}

static void bash_frame(void)
{
    int b;
    if (party_paused()) {
        return;   /* everything holds still while the match is paused */
    }
    bb.frame++;
    if (bb.frame == bb.msg_until) {
        party_hud_set(bb.text, bb.line_msg, "");
    }
    if (bb.frame == 2) {
        bag_reset(0, 0.0f, 40.0f);   /* from its spawn point on the ledge to the middle */
        gold_sleep();
    }
    if (bb.frame > 2 && !bb.bag[1].active && gm_8016AEEC() <= GOLD_SECONDS) {
        gold_wake();
    }
    for (b = 0; b < BAGS; b++) {
        bag_frame(b);
    }
}

static void bag_player(PlayerInitData* bag, int team)
{
    gm_SetupPlayerDefaults(bag);
    bag->ckind = ChKind_Sandbag;
    bag->slot_type = Gm_PKind_Cpu;
    bag->cpu_kind = 0xF;
    bag->defense_ratio = 1.0f;
    bag->team = (u8) team;
    /* No entry animation (Home-Run Contest sets the same bit): the Sandbag has none, and its
     * entry motion's animation tree is garbage. */
    bag->xC_b1 = false;
    bag->xD_b3 = true;
}

static void bash_setup(StartMeleeData* start)
{
    int b;
    gold_color_restore();
    memset(&bb, 0, sizeof bb);
    for (b = 0; b < BAGS; b++) {
        bb.bag[b].last_hitter = -1;
        bb.bag[b].mul = b == 1 ? GOLD_MUL : 1;
        bag_player(&start->players[BAG_SLOT + b], 4);
    }
    bb.bag[0].active = 1;
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.timer_enabled = true;
    start->rules.time_limit = BASH_SECONDS;
    start->rules.on_match_start = bash_start;
    start->rules.on_frame_end = bash_frame;
}

/* The bags take half of what a hit would give them. */
static float bash_knockback(Fighter* fp, float kb)
{
    return fp->player_idx >= BAG_SLOT ? kb * BAG_KNOCKBACK : kb;
}

/* CPUs go for a bag (the VS AI ignores them), the yellow one once it is in and on the stage: jabs
 * to build damage, then a smash. */
static void bash_fighter_input(Fighter* fp)
{
    Fighter* bag;
    float dx, toward;
    u32 buttons = 0;

    if (fp->player_idx >= PARTY_PLAYERS || !ftCo_IsCpuControlled(fp)) {
        return;
    }
    bag = fighter(BAG_SLOT);
    if (bb.bag[1].active && fighter(BAG_SLOT + 1) != NULL &&
        fabsf(fighter(BAG_SLOT + 1)->cur_pos.x) <= EDGE_X) {
        bag = fighter(BAG_SLOT + 1);
    }
    if (bag == NULL) {
        return;
    }
    dx = bag->cur_pos.x - fp->cur_pos.x;
    toward = dx < 0.0f ? -1.0f : 1.0f;
    fp->input.lstick[0].x = fp->input.lstick[0].y = 0.0f;
    fp->input.cstick[0].x = fp->input.cstick[0].y = 0.0f;
    fp->input.triggers[0] = 0.0f;
    if (fabsf(bag->cur_pos.x) > EDGE_X || bag->cur_pos.y < -5.0f || bag->cur_pos.y > 60.0f) {
        fp->input.lstick[0].x = fp->cur_pos.x > 0.0f ? -0.6f : 0.6f;   /* wait in the middle */
    } else if (fabsf(dx) > 12.0f || fabsf(bag->cur_pos.y - fp->cur_pos.y) > 20.0f) {
        fp->input.lstick[0].x = toward;
    } else if (bag->dmg.x1830_percent < 70.0f) {
        int t = (bb.frame + fp->player_idx * 3) % 10;
        if (fp->facing_dir * toward < 0.0f) {
            fp->input.lstick[0].x = toward * 0.4f;   /* turn to it */
        } else if (t == 0) {
            buttons |= HSD_PAD_A;                     /* forward tilt */
            fp->input.lstick[0].x = toward * 0.5f;
        }
    } else if (((bb.frame + fp->player_idx * 5) % 20) == 0) {
        fp->input.lstick[0].x = toward;   /* a smash: neutral to full with A */
        buttons |= HSD_PAD_A;
    }
    fp->input.held_buttons[0] = buttons;
}

static void bash_result(s8 place[PARTY_PLAYERS])
{
    int i, j;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        place[i] = 0;
        for (j = 0; j < PARTY_PLAYERS; j++) {
            if (bb.score[j] > bb.score[i]) {
                place[i]++;
            }
        }
    }
    party_log("bag bash: scores %d %d %d %d", bb.score[0], bb.score[1], bb.score[2], bb.score[3]);
    gold_color_restore();
}

const PartyMinigame mg_sandbag = {
    "Bag Bash", "sandbag", bash_setup, bash_fighter_input, bash_result, 0, NULL,
    St_Kind_Last, ChKind_Sandbag, NULL, NULL, bash_knockback,
};
