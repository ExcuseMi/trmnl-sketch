/* The shape tools: Line, Box, Oval. The rubber band is on the screen only (never in the page, never saved), the
   constraint (barrel button or Shift) snaps lines to 0, 45 and 90 degrees and boxes and ovals to squares and circles,
   one undo step per shape, mirrored, the eraser erases. SPDX-License-Identifier: MIT */
#include "check.h"
#include <math.h>
#include <stdio.h>
#include "util.h"

#define DOWN (APP_PEN_TIP | APP_PEN_IN_RANGE)
static int blk(const rig_t *r, int x, int y) { return !cv_get(&r->s.page, x, y); }
static int pages_equal(const rig_t *r, const uint8_t *copy) { return !memcmp(r->s.page.px, copy, (size_t)TS * TH); }

/* a drag from (x0, y0) to (x1, y1) in 6 moves; `hold` stays down (no lift) */
static void drag(rig_t *r, int x0, int y0, int x1, int y1, unsigned extra, int lift)
{
    for (int k = 0; k <= 6; k++) rig_pen(r, x0 + (x1 - x0) * k / 6, y0 + (y1 - y0) * k / 6, DOWN | extra, 30000);
    if (lift) rig_pen(r, x1, y1, APP_PEN_IN_RANGE | extra, 0);
}
static void settle(rig_t *r) { rig_pen(r, 10, 600, 0, 0); r->now += 3000; sk_tick(&r->s, r->now); }

static void test_constrain(void)
{
    for (int a = 0; a < 360; a += 3)
        for (int len = 20; len <= 400; len += 95) {
            float rad = (float)a * 3.14159265f / 180;
            int dx = (int)(len * cosf(rad)), dy = (int)(len * sinf(rad)), x = 500 + dx, y = 500 + dy;
            sk_shape_end(TL_LINE, 500, 500, &x, &y, 1);
            int ax = abs(x - 500), ay = abs(y - 500);
            CHECK(ax == 0 || ay == 0 || ax == ay); /* 0, 45 or 90 degrees */
            x = 500 + dx, y = 500 + dy;
            sk_shape_end(TL_LINE, 500, 500, &x, &y, 0);
            CHECK(x == 500 + dx && y == 500 + dy); /* off: untouched */
            x = 500 + dx, y = 500 + dy;
            sk_shape_end(TL_OVAL, 500, 500, &x, &y, 1);
            CHECK(abs(x - 500) == abs(y - 500)); /* a circle */
        }
    int x = 700, y = 520; /* nearly flat: flat */
    sk_shape_end(TL_LINE, 500, 500, &x, &y, 1);
    CHECK(y == 500 && x == 700);
    x = 700, y = 690; /* near 45 */
    sk_shape_end(TL_LINE, 500, 500, &x, &y, 1);
    CHECK(x - 500 == y - 500);
}

static void test_pts(void)
{
    int16_t pt[SK_SHAPE_PTS][2];
    CHECK(sk_shape_pts(TL_LINE, 1, 2, 30, 40, pt, SK_SHAPE_PTS) == 2);
    CHECK(sk_shape_pts(TL_RECT, 10, 10, 50, 40, pt, SK_SHAPE_PTS) == 5 && pt[4][0] == 10 && pt[4][1] == 10 && pt[2][0] == 50 && pt[2][1] == 40);
    int n = sk_shape_pts(TL_OVAL, 100, 100, 700, 500, pt, SK_SHAPE_PTS);
    CHECK(n > 24 && n <= SK_SHAPE_PTS && pt[0][0] == 700 && pt[n - 1][0] == 700); /* closed */
    for (int i = 0; i < n; i++) { /* on the ellipse */
        float u = (pt[i][0] - 400) / 300.0f, v = (pt[i][1] - 300) / 200.0f, e = u * u + v * v;
        CHECK(e > 0.98f && e < 1.02f);
    }
    CHECK(sk_shape_pts(TL_OVAL, 0, 0, 5000, 5000, pt, SK_SHAPE_PTS) <= SK_SHAPE_PTS); /* bounded */
}

static void test_preview_and_commit(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    sk_set_tool(&r.s, TL_LINE, r.now);
    settle(&r);
    static uint8_t before[TS * TH];
    memcpy(before, r.s.page.px, sizeof(before));
    drag(&r, 300, 300, 900, 300, 0, 0); /* the tip is still down */
    CHECK(pages_equal(&r, before) && !r.s.dirty); /* the preview is on the screen only */
    CHECK(fb_black(&r) > 100 && !same_page_and_fb(&r));
    CHECK(r.s.hist.st_cur == 0);
    rig_pen(&r, 900, 300, APP_PEN_IN_RANGE, 0); /* lift */
    CHECK(blk(&r, 600, 300) && blk(&r, 300, 300) && blk(&r, 900, 300) && !blk(&r, 600, 330));
    CHECK(r.s.hist.st_cur == 1 && r.s.dirty);
    settle(&r);
    CHECK(same_page_and_fb(&r)); /* the rubber band left no mark */
    sk_undo(&r.s, r.now);
    CHECK(pages_equal(&r, before) && r.s.hist.st_cur == 0); /* one undo step */
    sk_redo(&r.s, r.now);
    CHECK(blk(&r, 600, 300));
    /* a tap makes no shape */
    int n = r.s.hist.st_cur;
    rig_pen(&r, 400, 700, DOWN, 30000);
    rig_pen(&r, 401, 700, APP_PEN_IN_RANGE, 0);
    CHECK(r.s.hist.st_cur == n);
    /* a drag that ends on the toolbar cancels, the page stays */
    sk_toolbar(&r.s, r.now);
    memcpy(before, r.s.page.px, sizeof(before));
    drag(&r, 500, 900, 800, 1000, 0, 0);
    int bx = r.s.tb.cx[TI_UNDO] + 20, by = r.s.tb.cyi[TI_UNDO] + 20;
    rig_pen_user(&r, bx, by, DOWN, 30000);
    rig_pen_user(&r, bx, by, APP_PEN_IN_RANGE, 0);
    CHECK(!r.s.sh_on && box_empty(&r.s.sh_box));
    rig_end(&r);
}

static void test_shapes_draw(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    sk_set_tool(&r.s, TL_RECT, r.now);
    drag(&r, 300, 300, 800, 600, 0, 1);
    CHECK(blk(&r, 300, 450) && blk(&r, 800, 450) && blk(&r, 550, 300) && blk(&r, 550, 600) && !blk(&r, 550, 450));
    sk_set_tool(&r.s, TL_OVAL, r.now);
    drag(&r, 1000, 300, 1400, 700, 0, 1);
    CHECK(blk(&r, 1200, 300) && blk(&r, 1200, 700) && blk(&r, 1000, 500) && blk(&r, 1400, 500) && !blk(&r, 1200, 500) && !blk(&r, 1010, 310));
    CHECK(r.s.hist.st_cur == 2);
    /* a circle from a wide drag with the barrel button held */
    sk_set_tool(&r.s, TL_OVAL, r.now);
    drag(&r, 200, 900, 700, 1050, APP_PEN_BARREL, 1);
    CHECK(blk(&r, 450, 900) && blk(&r, 450, 1398) && blk(&r, 200, 1150) && blk(&r, 700, 1150)); /* a circle of 500 px: bottom at 1400 */
    CHECK(!blk(&r, 450, 1150)); /* hollow */
    CHECK(r.s.hist.st_cur == 3);
    /* the line constrained flat */
    sk_set_tool(&r.s, TL_LINE, r.now);
    drag(&r, 900, 900, 1500, 940, APP_PEN_BARREL, 1);
    CHECK(blk(&r, 1500, 900) && !blk(&r, 1500, 940) && blk(&r, 1200, 900));
    /* Shift on a keyboard does the same */
    r.s.mods = APP_MOD_SHIFT;
    drag(&r, 900, 1100, 1500, 1140, 0, 1);
    r.s.mods = 0;
    CHECK(blk(&r, 1500, 1100) && !blk(&r, 1500, 1140));
    rig_end(&r);
}

static void test_mirror_and_eraser(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    sk_set_mirror(&r.s, MR_LR, r.now);
    sk_set_tool(&r.s, TL_RECT, r.now);
    drag(&r, 200, 300, 500, 600, 0, 1);
    CHECK(blk(&r, 200, 450) && blk(&r, TW - 1 - 200, 450) && blk(&r, TW - 1 - 500, 450)); /* its mirror image too */
    CHECK(r.s.hist.st_cur == 1); /* one undo step for both */
    sk_set_mirror(&r.s, MR_OFF, r.now);
    sk_set_brush(&r.s, BR_ERASER, r.now);
    sk_set_tool(&r.s, TL_LINE, r.now);
    drag(&r, 100, 450, 600, 450, 0, 1); /* an erasing line cuts the box's sides */
    CHECK(!blk(&r, 200, 450) && !blk(&r, 500, 450) && blk(&r, 350, 300));
    rig_end(&r);
}

int main(void)
{
    test_constrain();
    test_pts();
    test_preview_and_commit();
    test_shapes_draw();
    test_mirror_and_eraser();
    return CHECK_DONE("test_shape");
}
