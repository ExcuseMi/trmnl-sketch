/* The size slider of the toolbar: a hit or drag maps to sizes monotonically, a tap jumps, the knob is as wide as the size,
   a drag refreshes only the slider and never inks, the eraser keeps its own size. SPDX-License-Identifier: MIT */
#include "check.h"
#include <stdio.h>
#include "util.h"

static void key(rig_t *r, int bit)
{
    r->now += 50;
    sk_pen_key(&r->s, 1u << bit, 1u << bit, r->now);
    sk_pen_key(&r->s, 0, 1u << bit, r->now);
}

static void test_mapping(void)
{
    for (int o = 0; o < 4; o++) {
        tb_layout_t L;
        int turned = o == APP_ORIENT_BAR_LEFT || o == APP_ORIENT_BAR_RIGHT;
        tb_layout(&L, o, turned ? TH : TW, turned ? TW : TH);
        int prev = 0, prev_x = -1;
        for (int x = L.sl_x0 - 40; x <= L.sl_x1 + 40; x++) {
            int s = tb_slider_size(&L, x);
            CHECK(s >= SK_SIZE_MIN && s <= SK_SIZE_MAX && s >= prev); /* monotonic along the track, clamped at its ends */
            prev = s;
        }
        CHECK(tb_slider_size(&L, L.sl_x0 - 50) == SK_SIZE_MIN && tb_slider_size(&L, L.sl_x1 + 50) == SK_SIZE_MAX);
        for (int s = SK_SIZE_MIN; s <= SK_SIZE_MAX; s++) {
            int x = tb_slider_x(&L, s);
            CHECK(tb_slider_size(&L, x) == s && x >= L.sl_x0 && x <= L.sl_x1 && x > prev_x); /* every size has its spot, in order */
            prev_x = x;
        }
        CHECK(L.sl_x0 > L.cx[TI_SIZE] + 20 && L.sl_x1 < L.cx[TI_SIZE + 2] + L.cell - 20); /* the widest knob stays in its cells */
    }
}

static int run_black(const rig_t *r, int x, int y) /* the black run down from y at column x, inclusive of y */
{
    int n = 0;
    for (int yy = y; yy < TH && !cv_get(&r->s.fb, x, yy); yy++) n++;
    for (int yy = y - 1; yy >= 0 && !cv_get(&r->s.fb, x, yy); yy--) n++;
    return n;
}

static void test_tap_and_knob(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    key(&r, 3);
    CHECK(r.s.tb_on);
    int sizes[] = { 20, 30, 48 };
    for (int i = 0; i < 3; i++) {
        int x = tb_slider_x(&r.s.tb, sizes[i]), y = r.s.tb.sl_y;
        rig_pen(&r, x, y, APP_PEN_IN_RANGE, 0);
        rig_pen(&r, x, y, APP_PEN_TIP | APP_PEN_IN_RANGE, 30000); /* a tap on the track jumps there */
        rig_pen(&r, x, y, APP_PEN_IN_RANGE, 0);
        CHECK(r.s.sty.size == sizes[i]);
        char want[8];
        snprintf(want, sizeof(want), "%d", sizes[i]);
        CHECK(!strcmp(r.cfg_size, want)); /* kept like before */
        rig_pen(&r, 10, 1000, 0, 0); /* away from the strip: the footprint preview is gone */
        r.now += 200;
        rect_t out[SK_MAX_RECTS];
        while (rig_frame(&r, out) != 0) {}
        int run = run_black(&r, x, y);
        CHECK(run >= sizes[i] - 1 && run <= sizes[i] + 1); /* the knob is as wide as the size (the track is 5 px) */
    }
    rig_end(&r);
}

static void test_drag(void)
{
    for (int o = APP_ORIENT_BAR_BOTTOM; o <= APP_ORIENT_BAR_TOP; o++) {
        if (o != APP_ORIENT_BAR_BOTTOM && o != APP_ORIENT_BAR_TOP) continue;
        rig_t r;
        rig_new(&r, o);
        rig_line(&r, 300, 600, 900, 620, 50000, 0); /* some ink to be kept */
        rig_pen(&r, 10, 1000, 0, 0);
        r.now += 3000;
        sk_tick(&r.s, r.now);
        key(&r, 3);
        static uint8_t page[TS * TH];
        memcpy(page, r.s.page.px, sizeof(page));
        rect_t out[SK_MAX_RECTS];
        while (rig_frame(&r, out) != 0) {}
        int sx0, sy0, sx1, sy1;
        tb_slider_box(&r.s.tb, &sx0, &sy0, &sx1, &sy1);
        int y = r.s.tb.sl_y, x0 = r.s.tb.sl_x0, x1 = r.s.tb.sl_x1, prev = 0;
        rig_pen(&r, x0 - 20, y, APP_PEN_IN_RANGE, 0);
        rig_pen(&r, x0 - 20, y, APP_PEN_TIP | APP_PEN_IN_RANGE, 30000); /* down left of the track: size 1 */
        CHECK(r.s.sty.size == SK_SIZE_MIN && !r.s.stroke);
        for (int x = x0; x <= x1 + 30; x += 4) {
            rig_pen(&r, x, y + (x & 8 ? 20 : -20), APP_PEN_TIP | APP_PEN_IN_RANGE, 30000); /* up and down off the track: only x counts */
            CHECK(r.s.sty.size >= prev && r.s.sty.size == tb_slider_size(&r.s.tb, x) && !r.s.stroke);
            prev = r.s.sty.size;
            r.now += 100;
            int n = rig_flush(&r, r.now, out);
            for (int i = 0; i < n && n != SK_FULL; i++) { /* a drag pushes the slider's cells only, never a flash */
                CHECK(!out[i].flash && out[i].w <= (sx1 - sx0 + 1) + 16 && out[i].y >= r.s.tb_box.y0 && out[i].y + out[i].h - 1 <= r.s.tb_box.y1);
                if (o == APP_ORIENT_BAR_BOTTOM) { CHECK(out[i].x >= sx0 - 16 && out[i].x + out[i].w - 1 <= sx1 + 16); }
            }
            CHECK(n != SK_FULL);
        }
        CHECK(r.s.sty.size == SK_SIZE_MAX);
        rig_pen(&r, x1 + 30, y, APP_PEN_IN_RANGE, 0); /* lifted */
        CHECK(!r.s.tb_drag && !strcmp(r.cfg_size, "48"));
        CHECK(!memcmp(page, r.s.page.px, sizeof(page))); /* no ink while dragging, the old ink stays */
        rig_end(&r);
    }
}

static void test_eraser_has_its_own(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    sk_set_size(&r.s, 30, r.now += 50);
    sk_set_brush(&r.s, BR_ERASER, r.now += 50);
    CHECK(r.s.sty.size == SK_SIZE_DEF); /* not the ink brushes' 30 */
    sk_set_size(&r.s, 41, r.now += 50);
    CHECK(!strcmp(r.cfg_size, "30")); /* the ink size in "size" is untouched */
    sk_set_brush(&r.s, BR_PENCIL, r.now += 50);
    CHECK(r.s.sty.size == 30);
    sk_set_brush(&r.s, BR_ERASER, r.now += 50);
    CHECK(r.s.sty.size == 41);
    sk_set_size(&r.s, 0, r.now += 50);
    sk_set_size(&r.s, 49, r.now += 50);
    CHECK(r.s.sty.size == 41); /* out of range: refused */
    rig_end(&r);
}

int main(void)
{
    test_mapping();
    test_tap_and_knob();
    test_drag();
    test_eraser_has_its_own();
    return CHECK_DONE("test_slider");
}
