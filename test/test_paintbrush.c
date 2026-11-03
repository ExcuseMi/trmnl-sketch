/* The paintbrush: picked from the toolbar; a light touch lays fewer dots than a hard one, full pressure is solid in the
   core and the rim is sparse; black darkens, white lays white over black, grey never goes past its own dots; its
   own sizes; undo and redo give back the stroke. SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 */
#include "check.h"
#include "util.h"

static void clear_page(rig_t *r, int colour)
{
    cv_fill(&r->s.page, colour);
    hist_reset(&r->s.hist);
}
static int count(const canvas_t *c, int colour)
{
    int n = 0;
    for (int y = 0; y < c->h; y++)
        for (int x = 0; x < c->w; x++) n += cv_get(c, x, y) == colour;
    return n;
}
static int ink_at(rig_t *r, int shade, unsigned pressure, int cls)
{
    clear_page(r, shade == SH_WHITE ? 0 : 1);
    sk_set_brush(&r->s, BR_BRUSH, r->now += 50);
    sk_set_shade(&r->s, shade, r->now += 50);
    sk_set_size(&r->s, cls, r->now += 50);
    rig_line(r, 400, 800, 1200, 800, pressure, 0);
    return count(&r->s.page, shade == SH_WHITE ? 1 : 0);
}

static void test_toolbar_pick(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    sk_toolbar(&r.s, r.now);
    CHECK(TI_BRUSH + BR_PEN == 0 && BR_BRUSH == 1 && !strcmp(brush_name(BR_BRUSH), "Brush")); /* the Pen is first on the toolbar (owner 2026-11-03), the Brush second */
    int x = r.s.tb.cx[TI_BRUSH + BR_BRUSH] + 20, y = r.s.tb.cy + 20;
    rig_pen_user(&r, x, y, APP_PEN_IN_RANGE, 0);
    rig_pen_user(&r, x, y, APP_PEN_TIP | APP_PEN_IN_RANGE, 30000);
    rig_pen_user(&r, x, y, APP_PEN_IN_RANGE, 0);
    CHECK(r.s.sty.brush == BR_BRUSH && r.s.note_on);
    CHECK(r.s.sty.size == brush_size(BR_BRUSH, r.s.size_cls));
    rig_end(&r);
}

static void test_pressure(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    int light = ink_at(&r, SH_BLACK, 4000, 1), mid = ink_at(&r, SH_BLACK, 30000, 1), hard = ink_at(&r, SH_BLACK, 65000, 1);
    CHECK(light > 200 && mid > light * 3 / 2 && hard > mid * 3 / 2); /* fewer dots for a light touch, more for a hard one */
    /* full pressure is solid in the core of the stroke, a light touch is not; the rim is sparse at full pressure */
    int solid = 0, core_light = 0, rim = 0;
    for (int x = 500; x < 1100; x++) solid += !cv_get(&r.s.page, x, 800) && !cv_get(&r.s.page, x, 796) && !cv_get(&r.s.page, x, 804);
    CHECK(solid == 600);
    for (int x = 500; x < 1100; x++) rim += !cv_get(&r.s.page, x, 800 + brush_size(BR_BRUSH, 1) / 2 * 120 / 100 - 2);
    CHECK(rim > 0 && rim < 450); /* near the rim: dots, not a line */
    ink_at(&r, SH_BLACK, 4000, 1);
    for (int x = 500; x < 1100; x++) core_light += !cv_get(&r.s.page, x, 800);
    CHECK(core_light < 560);
    /* pressure widens it a little */
    int wl = 0, wh = 0;
    ink_at(&r, SH_BLACK, 4000, 2);
    for (int y = 700; y < 900; y++) wl += !cv_get(&r.s.page, 800, y);
    ink_at(&r, SH_BLACK, 65000, 2);
    for (int y = 700; y < 900; y++) wh += !cv_get(&r.s.page, 800, y);
    CHECK(wh > wl);
    /* its own sizes */
    int s0 = ink_at(&r, SH_BLACK, 65000, 0), s1 = ink_at(&r, SH_BLACK, 65000, 1), s2 = ink_at(&r, SH_BLACK, 65000, 2);
    CHECK(s0 < s1 && s1 < s2 && brush_size(BR_BRUSH, 0) != brush_size(BR_PEN, 0));
    rig_end(&r);
}

static void test_colours(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    /* white over black: the same dots as black over white, white, and nothing else changes */
    int blk = ink_at(&r, SH_BLACK, 30000, 1);
    int wht = ink_at(&r, SH_WHITE, 30000, 1);
    CHECK(wht == blk && wht > 200);
    int wl = ink_at(&r, SH_WHITE, 4000, 1), wh = ink_at(&r, SH_WHITE, 65000, 1);
    CHECK(wl < wh);
    /* grey: never past its own dots whatever the pressure; darkens with pressure up to that cap */
    int g_light = ink_at(&r, SH_GREY, 4000, 1), g_hard = ink_at(&r, SH_GREY, 65000, 1);
    CHECK(g_light < g_hard);
    for (int y = 0; y < TH; y++)
        for (int x = 0; x < TW; x++)
            if (!cv_get(&r.s.page, x, y)) CHECK(cv_shade_black(SH_GREY, x, y));
    CHECK(g_hard < ink_at(&r, SH_BLACK, 65000, 1) * 6 / 10);
    rig_end(&r);
}

static void test_undo_redo_cursor(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    ink_at(&r, SH_BLACK, 40000, 1);
    static uint8_t done[TS * TH];
    memcpy(done, r.s.page.px, sizeof(done));
    sk_undo(&r.s, r.now += 50);
    CHECK(count(&r.s.page, 0) == 0);
    sk_redo(&r.s, r.now += 50);
    CHECK(!memcmp(done, r.s.page.px, sizeof(done)));
    for (int c = 0; c < BR_SIZES; c++) { /* the cursor is a ring of the size */
        int n = 0;
        for (int dy = -60; dy <= 60; dy++)
            for (int dx = -60; dx <= 60; dx++) n += brush_cursor_px(BR_BRUSH, brush_size(BR_BRUSH, c), dx, dy);
        CHECK(n > 40 && brush_cursor_r(BR_BRUSH, brush_size(BR_BRUSH, c)) >= 16);
    }
    rig_end(&r);
}

int main(void)
{
    test_toolbar_pick();
    test_pressure();
    test_colours();
    test_undo_redo_cursor();
    return CHECK_DONE("test_paintbrush");
}
