/* Melee Party minigame: Bag Bash. Everyone against one Sandbag on Final Destination for 25 seconds.
 * Every point of damage dealt to the bag scores a point for whoever dealt it, and knocking it off
 * the stage scores KO_BONUS. The bag comes back after a KO (a timed match), and the players can of
 * course hit each other out of the way.
 *
 * (Home-Run Contest's stage and HUD were the first idea, but its distance counter and barrier
 * depend on running inside the Home-Run Contest mode itself.)
 *
 * The bag is a fifth player: the Sandbag, CPU kind 0xF as Home-Run Contest sets it up. Damage is
 * credited to the bag's last attacker (its damage source, fp->dmg.x1868_source). */
#include <math.h>
#include <string.h>

#include <melee/ft/forward.h>
#include <melee/ft/inlines.h>
#include <melee/ft/kinds/ftCommon/ftCo_0A01.h>
#include <melee/ft/types.h>
#include <melee/gm/gm_1601.h>
#include <melee/gm/gmvs.h>
#include <melee/gr/forward.h>
#include <melee/pl/player.h>
#include <sysdolphin/baselib/controller.h>
#include <sysdolphin/baselib/gobj.h>

#include "party.h"
#include "party_hud.h"

#define BASH_SECONDS 25
#define BAG_SLOT 4
#define KO_BONUS 40
#define EDGE_X 68.0f
#define OUT_X 110.0f
#define OUT_BELOW (-60.0f)
#define OUT_ABOVE 160.0f

static struct {
    int score[PARTY_PLAYERS];
    float last_percent;
    int last_hitter;
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

/* Back in the middle, above the stage, at 0%. The position is set as the replay code's spawn
 * correction does (shim/mu_replay.c): the fighter, its collision and the player slot. */
static void bag_reset(Fighter* bag)
{
    Vec3 pos = { 0.0f, 40.0f, 0.0f };
    bag->cur_pos = pos;
    bag->coll_data.cur_pos = pos;
    bag->coll_data.last_pos = pos;
    bag->self_vel.x = bag->self_vel.y = bag->self_vel.z = 0.0f;
    bag->x8c_kb_vel.x = bag->x8c_kb_vel.y = bag->x8c_kb_vel.z = 0.0f;
    bag->dmg.x1830_percent = 0.0f;
    Player_80032828(bag->player_idx, bag->is_sub_fighter, &pos);
    bb.last_percent = 0.0f;
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
    party_hud_set(bb.text, bb.line_msg, "BAG BASH  -  hit the Sandbag!");
    bb.msg_until = 150;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        bb.line_p[i] = party_hud_line(bb.text, -225.0f + 150.0f * (float) i, -150.0f, 0.6f, port[i]);
        show(i);
    }
}

static void bash_frame(void)
{
    Fighter* bag = fighter(BAG_SLOT);
    float pct;
    int by;

    bb.frame++;
    if (bb.frame == bb.msg_until) {
        party_hud_set(bb.text, bb.line_msg, "");
    }
    if (bag == NULL) {
        return;
    }
    if (bb.frame == 2) {
        bag_reset(bag);   /* from its spawn point on the ledge to the middle */
    }
    /* The Sandbag never dies on the blast zones; out of the stage is a KO here. */
    if (fabsf(bag->cur_pos.x) > OUT_X || bag->cur_pos.y < OUT_BELOW || bag->cur_pos.y > OUT_ABOVE) {
        if (bb.last_hitter >= 0) {
            bb.score[bb.last_hitter] += KO_BONUS;
            show(bb.last_hitter);
            party_hud_set(bb.text, bb.line_msg, "P%d KNOCKED IT OUT!", bb.last_hitter + 1);
            bb.msg_until = bb.frame + 120;
            party_log("bag bash: KO by P%d", bb.last_hitter + 1);
        }
        bb.last_hitter = -1;
        bag_reset(bag);
    }
    pct = bag->dmg.x1830_percent;
    by = slot_of(bag->dmg.x1868_source);
    if (pct > bb.last_percent && by >= 0) {
        bb.score[by] += (int) (pct - bb.last_percent + 0.5f);
        bb.last_hitter = by;
        show(by);
    }
    bb.last_percent = pct;
}

static void bash_setup(StartMeleeData* start)
{
    PlayerInitData* bag = &start->players[BAG_SLOT];

    memset(&bb, 0, sizeof bb);
    bb.last_hitter = -1;
    party_rules_base(&start->rules, St_Kind_Last);
    start->rules.timer_enabled = true;
    start->rules.time_limit = BASH_SECONDS;
    start->rules.on_match_start = bash_start;
    start->rules.on_frame_end = bash_frame;

    gm_SetupPlayerDefaults(bag);
    bag->ckind = ChKind_Sandbag;
    bag->slot_type = Gm_PKind_Cpu;
    bag->cpu_kind = 0xF;
    bag->defense_ratio = 1.0f;
    bag->team = 4;
    /* No entry animation (Home-Run Contest sets the same bit): the Sandbag has none, and its
     * entry motion's animation tree is garbage. */
    bag->xC_b1 = false;
    bag->xD_b3 = true;
}

/* CPUs go for the bag (the VS AI ignores it): jabs to build damage, then a smash. */
static void bash_fighter_input(Fighter* fp)
{
    Fighter* bag;
    float dx, toward;
    u32 buttons = 0;

    if (fp->player_idx >= PARTY_PLAYERS || !ftCo_IsCpuControlled(fp)) {
        return;
    }
    bag = fighter(BAG_SLOT);
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
}

const PartyMinigame mg_sandbag = {
    "Bag Bash", "sandbag", bash_setup, bash_fighter_input, bash_result, 0, NULL,
    St_Kind_Last, ChKind_Sandbag,
};
