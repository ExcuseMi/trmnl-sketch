/* The toolbar lag of 2026-11-02 ("super laggy", "the toolbar causing huge lag"): a pen move over the open strip drew the
   whole strip twice (tb_draw, 0.87 ms a move on a PC, tens of ms on the X at ~100 moves a second, so the app fell behind
   the pen and the arrow pointer never kept up). The strip is drawn only when what it shows changes (src/sktbimg.c); the
   pills hide while the pen draws or hovers away from them (src/skplvis.c). SPDX-License-Identifier: MIT */
#include "check.h"
#include "util.h"

static void key(rig_t *r, int bit)
{
    r->now += 50;
    sk_pen_key(&r->s, 1u << bit, 1u << bit, r->now);
    sk_pen_key(&r->s, 0, 1u << bit, r->now);
}
static void frames(rig_t *r)
{
    rect_t out[SK_MAX_RECTS];
    while (rig_frame(r, out) != 0) {}
}
static int pills_px_black(const rig_t *r) /* black pixels of the framebuffer inside the pills' box */
{
    int n = 0;
    for (int y = r->s.pl_box.y0; y <= r->s.pl_box.y1; y++)
        for (int x = r->s.pl_box.x0; x <= r->s.pl_box.x1; x++) n += !cv_get(&r->s.fb, x, y);
    return n;
}

/* a hover across the whole strip: the strip is drawn once per item entered, never per move */
static void test_hover_over_strip_draws_it_per_item_only(void)
{
    for (int o = 0; o < 4; o++) {
        rig_t r;
        rig_new(&r, o);
        key(&r, 3); /* the toolbar */
        frames(&r);
        rig_pen_user(&r, 400, 700, APP_PEN_IN_RANGE, 0);
        frames(&r);
        static uint8_t before[TS * TH];
        memcpy(before, r.fb, sizeof(before));
        uint32_t p0 = r.s.tb_paints;
        int items = 0, last = -2, moves = 0;
        int y = r.s.tb.cyi[0] + r.s.tb.ch / 2; /* the first row's middle */
        for (int x = r.s.tb.x0 + 4; x < r.s.tb.x1 - 4; x += 4, moves++) {
            rig_pen_user(&r, x, y, APP_PEN_IN_RANGE, 0);
            if (r.s.tb_hot != last) items++, last = r.s.tb_hot;
            if (moves % 4 == 0) frames(&r);
        }
        CHECK(r.s.tb_ptr); /* the arrow is over the strip */
        CHECK(moves > 200 && (int)(r.s.tb_paints - p0) <= items + 1);
        rig_pen_user(&r, 400, 700, APP_PEN_IN_RANGE, 0); /* back to the page: the strip as it was, pixel for pixel */
        frames(&r);
        CHECK(!r.s.tb_ptr && r.s.tb_hot == -1);
        CHECK(!memcmp(before, r.fb, sizeof(before)));
        rig_end(&r);
    }
}

/* ink next to and under the open strip's edge draws no strip again */
static void test_ink_by_the_strip_draws_no_strip(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    key(&r, 3);
    rig_line(&r, 500, 900, 700, 910, 40000, 0); /* the first stroke fills a Recent slot: one paint */
    frames(&r);
    uint32_t p0 = r.s.tb_paints;
    int below = r.s.tb.y1 + 2;
    for (int i = 0; i < 5; i++) rig_line(&r, 300, below, 1500, below + 1, 50000, 0);
    CHECK(r.s.tb_paints == p0);
    rig_end(&r);
}

/* The pills: shown at the start, gone while the pen draws or hovers elsewhere, back when it hovers near them; a hover
   then a tap on Tools still opens the toolbar */
static void test_pills_hide_and_come_back(void)
{
    for (int o = 0; o < 4; o++) {
        rig_t r;
        rig_new(&r, o);
        frames(&r);
        CHECK(r.s.pl_on && pills_px_black(&r) > 0);
        int sw = o % 2 ? TH : TW, sh = o % 2 ? TW : TH;
        rig_pen_user(&r, sw / 3, sh / 2, APP_PEN_IN_RANGE, 0); /* hover mid page */
        frames(&r);
        CHECK(!r.s.pl_on && pills_px_black(&r) == 0);
        int px = (r.s.pl.x0[0] + r.s.pl.x1[0]) / 2, py = (r.s.pl.y0 + r.s.pl.y1) / 2;
        rig_pen_user(&r, px - 120, py, APP_PEN_IN_RANGE, 0); /* near them */
        frames(&r);
        CHECK(r.s.pl_on && pills_px_black(&r) > 0);
        rig_pen_user(&r, px - 120, py, APP_PEN_TIP | APP_PEN_IN_RANGE, 40000); /* a stroke through their corner */
        CHECK(!r.s.pl_on);
        rig_pen_user(&r, px, py, APP_PEN_TIP | APP_PEN_IN_RANGE, 40000);
        rig_pen_user(&r, px, py, APP_PEN_IN_RANGE, 0);
        CHECK(r.s.pl_on && r.s.mode == SK_DRAW && !r.s.tb_on); /* the stroke never pressed a pill */
        frames(&r);
        CHECK(same_page_and_fb(&r));
        rig_pen_user(&r, px, py, APP_PEN_TIP | APP_PEN_IN_RANGE, 20000); /* a tap on Tools */
        rig_pen_user(&r, px, py, APP_PEN_IN_RANGE, 0);
        CHECK(r.s.tb_on);
        rig_end(&r);
    }
}

int main(void)
{
    test_hover_over_strip_draws_it_per_item_only();
    test_ink_by_the_strip_draws_no_strip();
    test_pills_hide_and_come_back();
    return CHECK_DONE("test_lag");
}
