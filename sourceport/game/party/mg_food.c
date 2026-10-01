/* Melee Party minigame: Food Frenzy. Thirty seconds on Battlefield while food rains from the sky;
 * whoever eats the most wins. Fighting is allowed: knock the others away from the food.
 *
 * Food is the common Food item. Eating it (picking it up with A) is counted by the party patch's
 * hook in ftpickupitem.c, the one place that knows which fighter ate it. */
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
#include <melee/it/types.h>
#include <melee/pl/player.h>
#include <sysdolphin/baselib/controller.h>
#include <sysdolphin/baselib/gobj.h>

#include "party.h"
#include "party_hud.h"

#define FOOD_SECONDS 30
#define FOOD_EVERY 18      /* frames between drops */
#define DROP_Y 110.0f
#define DROP_HALF 62.0f    /* Battlefield's floor runs to about +-68 */

static struct {
    int eaten[PARTY_PLAYERS];
    int frame;
    HSD_Text* text;
    int line_msg;
    int line_p[PARTY_PLAYERS];
} fd;

static void show(int slot)
{
    party_hud_set(fd.text, fd.line_p[slot], "P%d  %d", slot + 1, fd.eaten[slot]);
}

static void food_eaten(int slot, int item_kind)
{
    if (item_kind != It_Kind_Foods) {
        return;
    }
    fd.eaten[slot]++;
    show(slot);
}

static void drop_food(void)
{
    SpawnItem spawn;
    Vec3 pos;
    memset(&spawn, 0, sizeof spawn);
    pos.x = -DROP_HALF + (float) party_rand((int) (DROP_HALF * 2.0f));
    pos.y = DROP_Y;
    pos.z = 0.0f;
    spawn.kind = It_Kind_Foods;
    spawn.pos = pos;
    spawn.prev_pos = pos;
    spawn.facing_dir = 1.0f;
    spawn.x44_flag.b0 = true;
    Item_80268B18(&spawn);
}

static void food_start(void)
{
    static const GXColor port[PARTY_PLAYERS] = {
        { 255, 80, 80, 255 }, { 90, 150, 255, 255 }, { 255, 210, 60, 255 }, { 80, 220, 110, 255 },
    };
    int i;
    party_hud_init();
    fd.text = party_hud_text();
    fd.line_msg = party_hud_line(fd.text, 0.0f, -110.0f, 0.7f, PARTY_GOLD);
    party_hud_set(fd.text, fd.line_msg, "FOOD FRENZY  -  eat the most!");
    for (i = 0; i < PARTY_PLAYERS; i++) {
        fd.line_p[i] = party_hud_line(fd.text, -225.0f + 150.0f * (float) i, -150.0f, 0.6f, port[i]);
        show(i);
    }
}

static void food_frame(void)
{
    fd.frame++;
    if (fd.frame == 150) {
        party_hud_set(fd.text, fd.line_msg, "");
    }
    if (fd.frame > 60 && fd.frame % FOOD_EVERY == 0) {
        drop_food();
    }
}

static void food_setup(StartMeleeData* start)
{
    memset(&fd, 0, sizeof fd);
    minigame_on_eaten = food_eaten;
    party_rules_base(&start->rules, St_Kind_Battle);
    start->rules.timer_enabled = true;
    start->rules.time_limit = FOOD_SECONDS;
    start->rules.x30 = 0.6f;   /* softer hits: fights, but few KOs */
    start->rules.on_match_start = food_start;
    start->rules.on_frame_start = food_frame;
}

/* The nearest food on the stage, or NULL. */
static Item* nearest_food(float x, float y)
{
    HSD_GObj* g;
    Item* best = NULL;
    float best_d = 1.0e9f;
    for (g = HSD_GObjPLinkHead[HSD_GOBJ_PLINK_ITEM]; g != NULL; g = g->next) {
        Item* ip = GET_ITEM(g);
        float d;
        if (ip == NULL || ip->kind != It_Kind_Foods || ip->owner != NULL) {
            continue;
        }
        d = fabsf(ip->pos.x - x) + fabsf(ip->pos.y - y) * 0.7f;
        if (d < best_d) {
            best_d = d;
            best = ip;
        }
    }
    return best;
}

/* The CPUs go for the food (the VS AI would rather fight). */
static void food_fighter_input(Fighter* fp)
{
    Item* food;
    float dx, dy;
    u32 buttons = 0;

    if (!ftCo_IsCpuControlled(fp)) {
        return;
    }
    food = nearest_food(fp->cur_pos.x, fp->cur_pos.y);
    if (food == NULL) {
        return;   /* nothing to eat: the AI plays as it likes */
    }
    dx = food->pos.x - fp->cur_pos.x;
    dy = food->pos.y - fp->cur_pos.y;
    fp->input.lstick[0].x = dx > 3.0f ? 1.0f : dx < -3.0f ? -1.0f : 0.0f;
    fp->input.lstick[0].y = 0.0f;
    fp->input.cstick[0].x = fp->input.cstick[0].y = 0.0f;
    fp->input.triggers[0] = 0.0f;
    if (fabsf(dx) < 7.0f && fabsf(dy) < 8.0f && (fd.frame & 3) == 0) {
        buttons |= HSD_PAD_A;    /* pick it up: food is eaten at once */
        fp->input.lstick[0].x = 0.0f;
    } else if (dy > 18.0f && fabsf(dx) < 25.0f && fp->ground_or_air == GA_Ground &&
               (fd.frame & 15) == 0) {
        buttons |= HSD_PAD_X;    /* up to a platform */
    } else if (dy < -10.0f && fp->ground_or_air == GA_Ground && (fd.frame & 31) == 0) {
        fp->input.lstick[0].y = -1.0f;   /* down through a platform */
    }
    fp->input.held_buttons[0] = buttons;
}

static void food_result(s8 place[PARTY_PLAYERS])
{
    int i, j;
    for (i = 0; i < PARTY_PLAYERS; i++) {
        place[i] = 0;
        for (j = 0; j < PARTY_PLAYERS; j++) {
            if (fd.eaten[j] > fd.eaten[i]) {
                place[i]++;
            }
        }
    }
    party_log("food: eaten %d %d %d %d", fd.eaten[0], fd.eaten[1], fd.eaten[2], fd.eaten[3]);
    minigame_on_eaten = NULL;
}

const PartyMinigame mg_food = {
    "Food Frenzy", "food", food_setup, food_fighter_input, food_result, 0, NULL,
    St_Kind_Battle, -1,
};
