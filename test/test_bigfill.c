/* Large fills (owner 2026-11-03: "Too intricate to fill" for a Grey fill over a busy black page, 44,096 runs against a
   log of SK_HIST_POINTS): a fill is kept as its delta, any size; undo and redo give the exact pages; a row of large fills
   stays inside the delta pool (the oldest go into the base, never a crash or a wrong page). SPDX-License-Identifier: MIT */
#include "util.h"
#include "fill.h"

static void tap_at(rig_t *r, int x, int y)
{
    rig_pen(r, x, y, APP_PEN_IN_RANGE, 0);
    rig_pen(r, x, y, APP_PEN_TIP | APP_PEN_IN_RANGE, 30000);
    rig_pen(r, x, y, APP_PEN_IN_RANGE, 0);
}

/* a comb of 1 px black teeth every 12 px from y 100 to 1300: the white between them is one region of 156 runs a row */
static void comb(rig_t *r)
{
    for (int y = 100; y <= 1300; y++)
        for (int x = 6; x < TW; x += 12) cv_px_s(&r->s.page, x, y, 0, SH_BLACK);
    hist_reset(&r->s.hist);
    sk_show_page(&r->s);
}

static uint8_t snap[8][TS * TH];

static void test_fill_bigger_than_the_log(void)
{
    static rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    rig_cfg(&r, "fill_gap", "0");
    sk_cfg_reload(&r.s);
    comb(&r);
    memcpy(snap[0], r.s.page.px, TS * TH);
    sk_set_brush(&r.s, BR_FILL, r.now += 50);
    sk_set_shade(&r.s, SH_GREY, r.now += 50);
    bf_t plan;
    CHECK(bf_plan(&plan, &r.s.page, 900, 700, SH_GREY, 0, 0) == BF_OK);
    uint32_t runs = bf_runs(&plan);
    bf_free(&plan);
    CHECK(runs > SK_HIST_POINTS); /* the case: more runs than the whole log */
    tap_at(&r, 900, 700);
    if (r.s.note_on && memcmp(snap[0], r.s.page.px, TS * TH) == 0) tap_at(&r, 900, 700); /* "Fill the whole page?" */
    CHECK(memcmp(snap[0], r.s.page.px, TS * TH) != 0); /* filled, not "Too intricate" */
    CHECK(!cv_get(&r.s.page, 6, 700) && cv_shade_black(SH_GREY, 900, 701) == !cv_get(&r.s.page, 900, 701));
    memcpy(snap[1], r.s.page.px, TS * TH);
    sk_undo(&r.s, r.now += 100);
    sk_settle(&r.s, r.now);
    CHECK(!memcmp(snap[0], r.s.page.px, TS * TH));
    sk_redo(&r.s, r.now += 100);
    sk_settle(&r.s, r.now);
    CHECK(!memcmp(snap[1], r.s.page.px, TS * TH));
    rig_end(&r);
}

/* Fills that each take a third of the pool, strokes between: the pool never holds more than its size, every undo that is
   offered gives the page as it was, and the ones that went into the base are simply not offered */
static void test_fills_beyond_the_pool(void)
{
    static rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    rig_cfg(&r, "fill_gap", "0");
    sk_cfg_reload(&r.s);
    static uint8_t small_pool[200 * 1024]; /* a pool halved by short memory: two Grey fills do not fit side by side */
    free(r.s.hist.pool);
    hist_set_deltas(&r.s.hist, r.s.hist.shadow.px, small_pool, sizeof(small_pool));
    comb(&r);
    memcpy(snap[0], r.s.page.px, TS * TH);
    static const int shades[] = { SH_GREY, SH_BLACK, SH_WHITE, SH_GREY, SH_BLACK, SH_WHITE, SH_GREY };
    int n = 0;
    for (int k = 0; k < 7; k++) {
        sk_set_brush(&r.s, BR_FILL, r.now += 50);
        sk_set_shade(&r.s, shades[k], r.now += 50);
        r.s.bk_ask_until = r.now + 1000; /* "Fill the whole page?" was asked: this tap confirms */
        sk_fill_at(&r.s, 900, 700, r.now += 10);
        CHECK(r.s.hist.st_cur > 0 && r.s.hist.st[r.s.hist.st_cur - 1].kind == HK_FILL);
        sk_set_brush(&r.s, BR_PEN, r.now += 50);
        rig_line(&r, 200 + k * 100, 1350, 300 + k * 100, 1380, 40000, 0);
        CHECK(hist_delta_bytes(&r.s.hist) <= r.s.hist.pool_cap);
        if (n < 7) memcpy(snap[++n], r.s.page.px, TS * TH);
    }
    int undone = 0;
    for (int k = n - 1; k >= 0 && hist_can_undo(&r.s.hist); k--) {
        sk_undo(&r.s, r.now += 100); /* the stroke */
        sk_settle(&r.s, r.now);
        if (!hist_can_undo(&r.s.hist)) break;
        sk_undo(&r.s, r.now += 100); /* the fill */
        sk_settle(&r.s, r.now);
        CHECK(!memcmp(snap[k], r.s.page.px, TS * TH));
        undone++;
    }
    CHECK(undone >= 2); /* the newest fills stay undoable */
    for (int k = 0; k < 2 * undone + 2; k++) sk_redo(&r.s, r.now += 100), sk_settle(&r.s, r.now);
    CHECK(!memcmp(snap[n], r.s.page.px, TS * TH));
    r.s.hist.pool = 0; /* the test's pool: not for sk_free */
    rig_end(&r);
}

int main(void)
{
    test_fill_bigger_than_the_log();
    test_fills_beyond_the_pool();
    return CHECK_DONE("test_bigfill");
}

