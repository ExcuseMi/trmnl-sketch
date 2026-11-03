/* The in-app Settings screen: steps and wraps, saved and read again, a failed save says so, old values still work,
   the tablet buttons and the pen reach every row. SPDX-License-Identifier: MIT */
#include "check.h"
#include <stdio.h>
#include "util.h"
#include "sksettings.h"

static void btn(rig_t *r, int n)
{
    sk_pen_key(&r->s, 1u << (n - 1), 1u << (n - 1), r->now += 50);
    sk_pen_key(&r->s, 0, 1u << (n - 1), r->now);
}
static void frames(rig_t *r)
{
    rect_t out[SK_MAX_RECTS];
    while (rig_frame(r, out) != 0) {}
}
static void pen_xy(rig_t *r, int x, int y)
{
    rig_pen_user(r, x, y, APP_PEN_IN_RANGE, 0);
    rig_pen_user(r, x, y, APP_PEN_TIP | APP_PEN_IN_RANGE, 20000);
    rig_pen_user(r, x + 2, y + 1, APP_PEN_TIP | APP_PEN_IN_RANGE, 20000);
    rig_pen_user(r, x + 2, y + 1, APP_PEN_IN_RANGE, 0);
    rig_pen_user(r, 5, 5, 0, 0);
}
/* a pen tap on the left (dir -1) or right half of list row i (0: Back, 1..: the settings) */
static void pen_half(rig_t *r, int i, int dir)
{
    int mid = (r->s.lg.x - 30 + r->s.lg.x1) / 2;
    pen_xy(r, dir < 0 ? mid - 200 : mid + 200, r->s.lg.y0 + i * LIST_PITCH - 25);
}
static void open_settings(rig_t *r)
{
    btn(r, 7);
    CHECK(r->s.mode == SK_PAGEMENU);
    btn(r, 2), btn(r, 2), btn(r, 2); /* Print, Send to phone, Settings (Back is selected first) */
    btn(r, 4);
    CHECK(r->s.mode == SK_SETTINGS);
    frames(r);
}
static const char *val(rig_t *r, int i)
{
    static char name[32], v[16];
    sks_row(&r->s, i, name, v);
    return v;
}

static void test_steps_and_wrap(void)
{
    static rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    open_settings(&r);
    CHECK(r.s.list_n == SKS_N + 1 && r.s.act_sel == 1); /* Back on top, the first setting selected */
    /* Paper for new pages: Blank Grid Lines Dots, wrapping both ways */
    CHECK(!strcmp(val(&r, 0), "Blank"));
    pen_half(&r, 1, -1);
    CHECK(!strcmp(val(&r, 0), "Dots") && !strcmp(rig_cfgv(&r, "paper"), "dots") && r.s.paper_def == PP_DOTS);
    pen_half(&r, 1, 1);
    CHECK(!strcmp(val(&r, 0), "Blank") && !strcmp(rig_cfgv(&r, "paper"), "blank"));
    pen_half(&r, 1, 1);
    CHECK(!strcmp(val(&r, 0), "Grid") && !strcmp(rig_cfgv(&r, "paper"), "grid"));
    /* Paper spacing: 48 by 8 between 16 and 200, wrapping */
    CHECK(!strcmp(val(&r, 1), "48"));
    pen_half(&r, 2, 1);
    CHECK(!strcmp(val(&r, 1), "56") && !strcmp(rig_cfgv(&r, "paper_sp"), "56") && r.s.paper_sp_def == 56);
    for (int i = 0; i < 5; i++) pen_half(&r, 2, -1); /* 48 40 32 24 16 */
    CHECK(r.s.paper_sp_def == 16);
    pen_half(&r, 2, -1);
    CHECK(r.s.paper_sp_def == 200 && !strcmp(rig_cfgv(&r, "paper_sp"), "200")); /* below the least: the greatest */
    pen_half(&r, 2, 1);
    CHECK(r.s.paper_sp_def == 16); /* above the greatest: the least */
    /* Fill gap 0..6 */
    CHECK(!strcmp(val(&r, 2), "3"));
    for (int i = 0; i < 4; i++) pen_half(&r, 3, 1);
    CHECK(r.s.bk_gap == 0 && !strcmp(rig_cfgv(&r, "fill_gap"), "0")); /* 4, 5, 6, then 0 */
    pen_half(&r, 3, -1);
    CHECK(r.s.bk_gap == 6 && !strcmp(rig_cfgv(&r, "fill_gap"), "6"));
    /* Tablet turn Auto / Off, Tablet area Fill / Keep: two values, either half flips */
    CHECK(!strcmp(val(&r, 3), "Auto") && !strcmp(val(&r, 4), "Fill") && r.s.turn && r.s.fill);
    pen_half(&r, 4, 1);
    CHECK(!strcmp(val(&r, 3), "Off") && !r.s.turn && !strcmp(rig_cfgv(&r, "tablet_turn"), "off") && r.s.map.turn == 0);
    pen_half(&r, 4, -1);
    CHECK(r.s.turn && !strcmp(rig_cfgv(&r, "tablet_turn"), "auto"));
    pen_half(&r, 5, 1);
    CHECK(!strcmp(val(&r, 4), "Keep") && !r.s.fill && !strcmp(rig_cfgv(&r, "pen_area"), "keep"));
    pen_half(&r, 5, -1);
    CHECK(!strcmp(val(&r, 4), "Fill") && r.s.fill && !strcmp(rig_cfgv(&r, "pen_area"), "fill"));
    /* Clean-up after idle: Off 5 10 30 60 s; the default is 5 */
    CHECK(!strcmp(val(&r, 5), "5 s"));
    pen_half(&r, 6, 1);
    CHECK(r.s.cleanup_s == 10 && !strcmp(rig_cfgv(&r, "cleanup_s"), "10"));
    pen_half(&r, 6, 1), pen_half(&r, 6, 1);
    CHECK(r.s.cleanup_s == 60);
    pen_half(&r, 6, 1);
    CHECK(r.s.cleanup_s == 0 && !strcmp(val(&r, 5), "Off") && !strcmp(rig_cfgv(&r, "cleanup_s"), "0")); /* past 60: Off */
    pen_half(&r, 6, -1);
    CHECK(r.s.cleanup_s == 60);
    CHECK(!r.s.gnote[0]);
    rig_end(&r);
}

/* what was saved is what the next start reads */
static void test_save_and_reload(void)
{
    static rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    open_settings(&r);
    pen_half(&r, 1, 1);        /* Grid */
    pen_half(&r, 2, 1);        /* 56 */
    pen_half(&r, 3, -1);       /* 2 */
    pen_half(&r, 4, 1);        /* turn off */
    pen_half(&r, 5, 1);        /* keep */
    pen_half(&r, 6, -1);       /* 5 -> Off */
    rig_start(&r, APP_ORIENT_BAR_BOTTOM);
    CHECK(r.s.paper_def == PP_GRID && r.s.paper_sp_def == 56 && r.s.bk_gap == 2 && !r.s.turn && !r.s.fill && r.s.cleanup_s == 0);
    open_settings(&r);
    CHECK(!strcmp(val(&r, 0), "Grid") && !strcmp(val(&r, 1), "56") && !strcmp(val(&r, 5), "Off"));
    rig_end(&r);
}

/* values an earlier version saved that the steps do not produce still work and step to a neighbour */
static void test_old_values(void)
{
    static rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    rig_cfg(&r, "cleanup_s", "7");
    rig_cfg(&r, "paper_sp", "50");
    rig_start(&r, APP_ORIENT_BAR_BOTTOM);
    CHECK(r.s.cleanup_s == 7 && r.s.paper_sp_def == 50);
    open_settings(&r);
    CHECK(!strcmp(val(&r, 5), "7 s"));
    pen_half(&r, 6, 1);
    CHECK(r.s.cleanup_s == 10);
    pen_half(&r, 2, -1);
    CHECK(r.s.paper_sp_def == 48);
    rig_cfg(&r, "cleanup_s", "7");
    rig_start(&r, APP_ORIENT_BAR_BOTTOM);
    open_settings(&r);
    pen_half(&r, 6, -1);
    CHECK(r.s.cleanup_s == 5);
    rig_end(&r);
}

/* a store that refuses: the value works for now, the screen says it was not saved, the next start has the old one */
static void test_save_fails(void)
{
    static rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    open_settings(&r);
    r.cfg_fail = 1;
    pen_half(&r, 3, 1);
    CHECK(r.s.bk_gap == 4 && r.s.gnote[0] && strstr(r.s.gnote, "Could not save"));
    rect_t out[SK_MAX_RECTS];
    CHECK(rig_frame(&r, out) != 0); /* the note is drawn */
    r.cfg_fail = 0;
    pen_half(&r, 3, 1);
    CHECK(r.s.bk_gap == 5 && !r.s.gnote[0] && !strcmp(rig_cfgv(&r, "fill_gap"), "5")); /* the note goes with the next good save */
    r.cfg_fail = 1;
    pen_half(&r, 3, 1);
    CHECK(r.s.gnote[0]);
    rig_start(&r, APP_ORIENT_BAR_BOTTOM);
    CHECK(r.s.bk_gap == 5); /* the failed one is gone, the last saved one is read */
    rig_end(&r);
}

/* the tablet buttons and the touch bar: 1 and 2 move, 4 and the middle zone step forward, 5 and 7 and 8 return */
static void test_buttons_and_bar(void)
{
    static rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    open_settings(&r);
    btn(&r, 2);
    CHECK(r.s.act_sel == 2);
    btn(&r, 4);
    CHECK(r.s.paper_sp_def == 56);
    btn(&r, 1), btn(&r, 1);
    CHECK(r.s.act_sel == 0);
    btn(&r, 1);
    CHECK(r.s.act_sel == 0); /* no wrap past Back */
    sk_touch(&r.s, APP_TOUCH_RIGHT, APP_GESTURE_TAP, r.now += 50);
    sk_touch(&r.s, APP_TOUCH_MIDDLE, APP_GESTURE_TAP, r.now += 50);
    CHECK(r.s.paper_def == PP_GRID && r.s.mode == SK_SETTINGS);
    sk_touch(&r.s, APP_TOUCH_LEFT, APP_GESTURE_TAP, r.now += 50);
    sk_touch(&r.s, APP_TOUCH_MIDDLE, APP_GESTURE_TAP, r.now += 50); /* Back */
    CHECK(r.s.mode == SK_PAGEMENU && r.s.act_sel == 3);
    btn(&r, 4);
    CHECK(r.s.mode == SK_SETTINGS);
    btn(&r, 5);
    CHECK(r.s.mode == SK_PAGEMENU);
    btn(&r, 4), btn(&r, 7);
    CHECK(r.s.mode == SK_PAGEMENU); /* 7 closes the menu from the page menu, it returns from a settings screen to it */
    rig_end(&r);
}

/* a setting reaches the drawing at once: tablet area and turn change the pen map, the paper the next new page */
static void test_applies_at_once(void)
{
    static rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    open_settings(&r);
    pen_half(&r, 5, 1); /* keep */
    CHECK(!r.s.fill);
    pen_half(&r, 1, 1); /* Grid */
    btn(&r, 5), btn(&r, 5);
    CHECK(r.s.mode == SK_DRAW);
    rig_line(&r, 100, 200, 900, 300, 50000, 0);
    r.now += 3000;
    sk_tick(&r.s, r.now);
    btn(&r, 6);
    frames(&r);
    CHECK(r.s.paper == PP_GRID); /* the new page takes the setting at once */
    rig_end(&r);
}

int main(void)
{
    test_steps_and_wrap();
    test_save_and_reload();
    test_old_values();
    test_save_fails();
    test_buttons_and_bar();
    test_applies_at_once();
    return CHECK_DONE("test_settings");
}
