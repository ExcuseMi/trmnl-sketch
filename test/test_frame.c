/* The frame's thinning and the Brush's step scale (src/skframe.h), and that a frame draws the same stroke as point by
   point stamping for the tools whose stamps must touch. SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 */
#include "check.h"
#include "util.h"

static void test_thin_keeps_first_last_and_spaced_points(void)
{
    sk_qpt_t in[6] = { { 0, 0, 1, 0 }, { 1, 0, 2, 0 }, { 2, 1, 3, 0 }, { 4, 1, 4, 0 }, { 5, 1, 5, 0 }, { 5, 2, 6, 0 } };
    int keep[6];
    int k = sk_thin(in, 6, 3, keep);
    CHECK(k == 3 && keep[0] == 0 && keep[1] == 3 && keep[2] == 5); /* 4 is 4 px from 0; the last always */
    k = sk_thin(in, 6, 1, keep);
    CHECK(k == 6); /* 1 px: every point that moved */
    k = sk_thin(in, 6, 0, keep);
    CHECK(k == 6);
    k = sk_thin(in, 1, 3, keep);
    CHECK(k == 1 && keep[0] == 0);
    k = sk_thin(in, 2, 100, keep);
    CHECK(k == 2 && keep[1] == 1);
    CHECK(sk_thin(in, 0, 3, keep) == 0);
    sk_qpt_t same[4] = { { 7, 7, 1, 0 }, { 7, 7, 2, 0 }, { 7, 7, 3, 0 }, { 7, 7, 4, 0 } };
    CHECK(sk_thin(same, 4, 1, keep) == 2); /* a pen that does not move: its first and last report */
}

static void test_scale_doubles_over_budget_and_eases_back(void)
{
    int cheap = 5;
    CHECK(sk_scale_next(1, SK_FRAME_BUDGET_MS + 1, &cheap) == 2 && cheap == 0);
    CHECK(sk_scale_next(2, SK_FRAME_BUDGET_MS * 3, &cheap) == 4);
    CHECK(sk_scale_next(SK_STEP_SCALE_MAX, 1000, &cheap) == SK_STEP_SCALE_MAX);
    CHECK(sk_scale_next(4, SK_FRAME_BUDGET_MS, &cheap) == 4); /* at the budget: stays */
    int sc = 4;
    for (int i = 1; i < SK_SCALE_EASE_FRAMES; i++) CHECK((sc = sk_scale_next(sc, SK_FRAME_BUDGET_MS / 3 - 1, &cheap)) == 4);
    CHECK(sk_scale_next(sc, SK_FRAME_BUDGET_MS / 3 - 1, &cheap) == 3 && cheap == 0); /* a second of cheap frames: one step */
    sk_scale_next(3, 0, &cheap);
    CHECK(sk_scale_next(3, SK_FRAME_BUDGET_MS / 2, &cheap) == 3 && cheap == 0); /* a middling frame: the count starts over */
    CHECK(sk_scale_next(1, 0, &cheap) == 1);
    CHECK(sk_scale_next(0, 0, &cheap) == 1); /* unset: 1 */
}

/* The Pen through frames of 6 reports draws exactly what it drew report by report (its stamps touch: thin 1 px) */
static void test_pen_frames_draw_the_same_stroke(void)
{
    static uint8_t a[TS * TH];
    int xy[40];
    for (int i = 0; i < 20; i++) xy[2 * i] = 300 + i * 17, xy[2 * i + 1] = 500 + (i % 3) * 9;
    for (int frames = 0; frames < 2; frames++) {
        rig_t r;
        rig_new(&r, APP_ORIENT_BAR_BOTTOM);
        sk_set_brush(&r.s, BR_PEN, r.now += 50);
        rect_t out[SK_MAX_RECTS];
        while (rig_frame(&r, out) != 0) {}
        for (int i = 0; i < 20; i++) {
            rig_pen(&r, xy[2 * i], xy[2 * i + 1], APP_PEN_TIP | APP_PEN_IN_RANGE, 40000);
            if (frames ? i % 6 == 5 : 1) sk_frame(&r.s, r.now); /* every report, or every sixth */
        }
        rig_pen(&r, xy[38], xy[39], APP_PEN_IN_RANGE, 0);
        while (rig_frame(&r, out) != 0) {}
        CHECK(fb_black(&r) > 100);
        if (!frames) memcpy(a, r.fb, sizeof(a));
        else CHECK(!memcmp(a, r.fb, sizeof(a)));
        rig_end(&r);
    }
}

static int page_black(const rig_t *r) /* black pixels on the page canvas (the framebuffer also carries the cursor) */
{
    int n = 0;
    for (size_t i = 0; i < (size_t)TS * TH; i++)
        for (int b = 0; b < 8; b++) n += !(r->s.page.px[i] >> b & 1);
    return n;
}

/* A frame's points land on the page before the push, and a lift stamps what is left */
static void test_frame_stamps_before_the_push_and_the_lift(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    sk_set_brush(&r.s, BR_PEN, r.now += 50);
    rect_t out[SK_MAX_RECTS];
    while (rig_frame(&r, out) != 0) {}
    rig_line(&r, 300, 300, 400, 300, 40000, 0); /* the first stroke takes the start legend off the page */
    while (rig_frame(&r, out) != 0) {}
    int before = page_black(&r);
    for (int i = 0; i < 5; i++) rig_pen(&r, 400 + i * 10, 600, APP_PEN_TIP | APP_PEN_IN_RANGE, 40000);
    CHECK(r.s.frame.n == 5 && page_black(&r) == before); /* queued, nothing drawn yet */
    CHECK(rig_frame(&r, out) > 0 && r.s.frame.n == 0 && page_black(&r) > before); /* the push carries the stamps */
    int mid = page_black(&r);
    for (int i = 5; i < 8; i++) rig_pen(&r, 400 + i * 10, 600, APP_PEN_TIP | APP_PEN_IN_RANGE, 40000);
    rig_pen(&r, 470, 600, APP_PEN_IN_RANGE, 0); /* the lift: stamped at once, the history closed */
    CHECK(r.s.frame.n == 0 && page_black(&r) > mid && !r.s.stroke);
    rig_end(&r);
}

int main(void)
{
    test_thin_keeps_first_last_and_spaced_points();
    test_scale_doubles_over_budget_and_eases_back();
    test_pen_frames_draw_the_same_stroke();
    test_frame_stamps_before_the_push_and_the_lift();
    return CHECK_DONE("test_frame");
}
