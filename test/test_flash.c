/* The flash: one inverted-then-clean path for every clean-up, and the two clean-up rules (an overlay left the page; idle
   with traces). SPDX-License-Identifier: MIT */
#include "check.h"
#include <stdio.h>
#include "util.h"
#include "oldsizes.h"

static void key(rig_t *r, int bit)
{
    r->now += 50;
    sk_pen_key(&r->s, 1u << bit, 1u << bit, r->now);
    sk_pen_key(&r->s, 0, 1u << bit, r->now);
}
/* Time passes: the app's loop in 100 ms steps (tick, flush); returns the whole-screen flashes seen */
static int run(rig_t *r, uint32_t ms)
{
    int fulls = 0;
    rect_t out[SK_MAX_RECTS];
    for (uint32_t t = 0; t < ms; t += 100) {
        sk_tick(&r->s, r->now + 100);
        if (rig_frame(r, out) == SK_FULL) fulls++;
    }
    return fulls;
}
static void drain(rig_t *r)
{
    rect_t out[SK_MAX_RECTS];
    while (rig_frame(r, out) != 0) {}
}
/* a rig at rest: the legend gone, nothing pending, no traces */
static void rest(rig_t *r, int orient)
{
    rig_new(r, orient);
    r->s.cleanup_s = 5;
    run(r, 45000);
    CHECK(!r->s.note_on);
}
static void ink(rig_t *r) { rig_line(r, 300, 500, 900, 560, 40000, 0); } /* a stroke, the pen lifts (stays in range) */
static void inks(rig_t *r, int n) /* n strokes, each a fast-ink push of its own box */
{
    for (int i = 0; i < n; i++) rig_line(r, 300, 500 + i * 9, 900, 560 + i * 9, 40000, 0);
}

/* sk_flash(rect): the inverted area first, then the same area as it is, flashed */
static void test_helper(void)
{
    rig_t r;
    rest(&r, APP_ORIENT_BAR_BOTTOM);
    for (int y = 0; y < TH; y += 7) cv_disc(&r.s.page, 200 + y % 300, y, 6, 0, 0);
    sk_show_page(&r.s);
    static uint8_t before[TS * TH];
    memcpy(before, r.fb, sizeof(before));
    box_t area = { 100, 200, 411, 459 };
    sk_flash(&r.s, &area);
    rect_t out[SK_MAX_RECTS];
    int n = sk_flush(&r.s, r.now += 100, out);
    CHECK(n == 1 && out[0].flash == 0 && sk_flash_busy(&r.s)); /* the inverted half, a partial push */
    CHECK(out[0].x == 96 && out[0].w == 320 && out[0].y == 200 && out[0].h == 260);
    for (int y = out[0].y; y < out[0].y + out[0].h; y++)
        for (int x = out[0].x / 8; x < (out[0].x + out[0].w) / 8; x++) CHECK(r.fb[y * TS + x] == (uint8_t)~before[y * TS + x]);
    CHECK(r.fb[10 * TS + 5] == before[10 * TS + 5]); /* nothing outside it */
    rect_t o2[SK_MAX_RECTS];
    int m = sk_flush(&r.s, r.now += 1, o2);
    CHECK(m == 1 && o2[0].flash == 1 && !memcmp(&out[0], &o2[0], sizeof(rect_t) - sizeof(int16_t)) && !sk_flash_busy(&r.s));
    CHECK(!memcmp(before, r.fb, sizeof(before))); /* the real content is back */
    CHECK(sk_flush(&r.s, r.now += 100, out) == 0);
    /* the whole screen: inverted whole, then a full refresh */
    box_t all = { 0, 0, TW - 1, TH - 1 };
    sk_flash(&r.s, &all);
    n = sk_flush(&r.s, r.now += 100, out);
    CHECK(n == 1 && out[0].w == TW && out[0].h == TH && out[0].flash == 0 && r.fb[3] == (uint8_t)~before[3]);
    CHECK(sk_flush(&r.s, r.now += 1, out) == SK_FULL && !memcmp(before, r.fb, sizeof(before)));
    rig_end(&r);
}

/* Every clean-up of the app goes through it: rig_flush counts a full refresh without its inverted half */
static void test_every_path(void)
{
    rig_t r;
    rest(&r, APP_ORIENT_BAR_BOTTOM);
    rect_t out[SK_MAX_RECTS];
    int i0 = r.inverted;
    key(&r, 3); /* the strip shows: fast ink, no flash */
    CHECK(rig_frame(&r, out) >= 1 && out[0].flash == 0 && r.inverted == i0);
    key(&r, 3); /* and goes: fast ink too */
    CHECK(rig_frame(&r, out) >= 1 && out[0].flash == 0 && r.inverted == i0);
    ink(&r);
    key(&r, 5); /* a new page */
    run(&r, 300);
    CHECK(r.inverted >= i0 + 1);
    sk_to_pagemenu(&r.s, r.now += 10); /* the page menu */
    drain(&r);
    CHECK(r.s.mode == SK_PAGEMENU);
    CHECK(r.bare_fulls == 0);
    rig_end(&r);
}

/* The drawing page never flashes by itself: the legend, the toolbar, toasts, strokes and long idle are ink only
   (owner 2026-11-02: a flash costs 1.1 s and blocks the app). Only menu and gallery screens get the idle flash. */
static void test_draw_never(void)
{
    rig_t r;
    rest(&r, APP_ORIENT_BAR_BOTTOM);
    int inv0 = r.inverted, ful0 = r.fulls;
    sk_set_size(&r.s, 2, r.now += 10); /* a toast */
    drain(&r);
    CHECK(run(&r, 5000) == 0 && !r.s.note_on);
    unsigned x, y;
    key(&r, 3); /* the toolbar up and away again */
    drain(&r);
    sk_set_brush(&r.s, BR_PENCIL, r.now += 10);
    key(&r, 3);
    CHECK(run(&r, 20000) == 0);
    inks(&r, 30); /* many strokes, then a long pause: no build-up yet */
    CHECK(run(&r, 60000) == 0 && r.s.ghost_px < (uint32_t)(SK_GHOST_SCREENS * TW * TH));
    rig_pt(&r, 400, 700, &x, &y); /* a long press */
    sk_pen(&r.s, APP_PEN_TIP | APP_PEN_IN_RANGE, x, y, 30000, r.now += 10);
    drain(&r);
    CHECK(run(&r, 20000) == 0);
    sk_pen(&r.s, APP_PEN_IN_RANGE, x, y, 0, r.now += 10);
        CHECK(run(&r, 30000) == 0 && r.inverted == inv0 && r.fulls == ful0);
    rig_end(&r);
    rig_new(&r, APP_ORIENT_BAR_BOTTOM); /* the start legend, over by itself */
    r.s.cleanup_s = 5;
    run(&r, 100); /* (the first screen is a full refresh) */
    CHECK(r.s.legend && r.s.note_on);
    inv0 = r.inverted;
    CHECK(run(&r, 41000) == 0 && r.inverted == inv0);
    rig_end(&r);
}

/* The drawing page's idle clean-up after a build-up (owner 2026-11-03): fast ink over SK_GHOST_SCREENS screens, then
   only with the pen out of range and idle for cleanup_s, once; never while the pen is near or drawing */
static void big_strokes(rig_t *r, int n)
{
    for (int i = 0; i < n; i++) {
        int y = 60 + (i * 37) % (TH - 120);
        rig_line(r, 40, y, TW - 40, TH - 40 - y, 60000, 0); /* the pen lifts and stays in range */
        drain(r);
    }
}
static void test_draw_built_up(void)
{
    rig_t r;
    rest(&r, APP_ORIENT_BAR_BOTTOM);
    int inv0 = r.inverted;
    big_strokes(&r, 200);
    CHECK(r.s.ghost_px >= (uint32_t)(SK_GHOST_SCREENS * TW * TH));
    CHECK(run(&r, 20000) == 0 && r.inverted == inv0); /* the pen in range above the page: never */
    unsigned x, y;
    rig_pt(&r, 400, 700, &x, &y);
    sk_pen(&r.s, APP_PEN_TIP | APP_PEN_IN_RANGE, x, y, 30000, r.now += 10); /* down and held: never */
    CHECK(run(&r, 20000) == 0);
    sk_pen(&r.s, APP_PEN_IN_RANGE, x, y, 0, r.now += 10);
    sk_pen(&r.s, 0, x, y, 0, r.now += 10); /* away */
    CHECK(run(&r, 4000) == 0); /* not before cleanup_s */
    CHECK(run(&r, 2000) == 1 && r.inverted == inv0 + 1 && r.s.mode == SK_DRAW); /* once, the inverted half first */
    CHECK(r.s.ghost_px == 0 && run(&r, 60000) == 0); /* the build-up starts again: no second one */
    big_strokes(&r, 10);
    sk_pen(&r.s, 0, x, y, 0, r.now += 10);
    CHECK(run(&r, 30000) == 0); /* a few strokes after it: no flash */
    r.s.cleanup_s = 0; /* the setting off: never */
    big_strokes(&r, 200);
    sk_pen(&r.s, 0, x, y, 0, r.now += 10);
    CHECK(run(&r, 30000) == 0);
    rig_end(&r);
}

/* Rule b in the gallery, a menu and with the toolbar */
static void test_idle_screens(void)
{
    rig_t r;
    rest(&r, APP_ORIENT_BAR_BOTTOM);
    ink(&r);
    run(&r, 400);
    sk_to_pagemenu(&r.s, r.now += 10);
    run(&r, 1000);
    CHECK(r.s.mode == SK_PAGEMENU);
    unsigned x, y;
    rig_pt(&r, 300, 300, &x, &y);
    sk_pen(&r.s, APP_PEN_IN_RANGE, x, y, 0, r.now += 10); /* the pointer: a partial refresh */
    run(&r, 300);
    CHECK(r.s.ghost);
    CHECK(run(&r, 6000) == 1 && r.s.mode == SK_PAGEMENU);
    CHECK(run(&r, 20000) == 0);
    /* the tip held down on a menu screen: no flash however long (the stroke flag is not set there) */
    rig_pt(&r, 40, 40, &x, &y);
    sk_pen(&r.s, APP_PEN_IN_RANGE, x, y, 0, r.now += 10);
    sk_pen(&r.s, APP_PEN_TIP | APP_PEN_IN_RANGE, x, y, 30000, r.now += 10);
    run(&r, 300);
    CHECK(r.s.tip && r.s.ghost && run(&r, 20000) == 0);
    rig_end(&r);
    rest(&r, APP_ORIENT_BAR_BOTTOM);
    key(&r, 3); /* the toolbar up, a hover on it */
    drain(&r);
    rig_pt(&r, r.s.tb.cx[2] + 10, r.s.tb.cyi[2] + 10, &x, &y);
    sk_pen(&r.s, APP_PEN_IN_RANGE, x, y, 0, r.now += 10);
    run(&r, 300);
    CHECK(r.s.ghost && r.s.tb_on);
    CHECK(run(&r, 6000) == 0 && r.s.tb_on); /* the arrow and the frame are ink only (test_tbflash.c) */
    inks(&r, 10);
    CHECK(run(&r, 6000) == 0 && r.s.tb_on); /* strokes under the strip: still no flash */
    rig_end(&r);
}

int main(void)
{
    test_helper();
    test_every_path();
    test_draw_never();
    test_draw_built_up();
    test_idle_screens();
    return CHECK_DONE("test_flash");
}
