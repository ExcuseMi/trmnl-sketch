/* Flashing with the toolbar up (owner 2026-11-02: "It's flashing a lot with the full toolbar on there"). A flash is the
   black/white blink of sk_flash (an inverted half, then a clean FULL refresh): rig_t.inverted counts them. A scripted
   session with the strip shown: the pen hovers 400 moves over it, picks 5 buttons (looking 3 s at each), draws 20
   strokes near it with a pause every 5. The arrow, the item frame, the pick shown inverted, the brush cursor
   and a toast change only black and white pixels: they go as fast ink and never ask for a flash. Strokes leave traces:
   the drawing page gets one idle clean-up a pause (owner 2026-11-03, skflash.c), never while the pen moves.
   Before the fix: 3 flashes (the strip, two idle clean-ups). SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 */
#include "check.h"
#include <stdio.h>
#include "util.h"

static int spins; /* turns where the app would not sleep twice in a row at the same instant with nothing to push */
static void turn(rig_t *r)
{
    rect_t out[SK_MAX_RECTS];
    sk_tick(&r->s, r->now);
    rig_flush(r, r->now, out);
    if (sk_wait_ms(&r->s, r->now) == 0) { /* due now: the next turn must have taken it */
        sk_tick(&r->s, r->now);
        rig_flush(r, r->now, out);
        if (sk_wait_ms(&r->s, r->now) == 0) spins++;
    }
}
/* the pen at a point, 10 ms after the last one (a tablet sends about 100 a second), and the app's turn */
static void pen(rig_t *r, int x, int y, unsigned f, unsigned p)
{
    rig_pen(r, x, y, f, p);
    r->now += 5;
    turn(r);
}
static void wait(rig_t *r, int ms)
{
    for (int t = 0; t < ms; t += 20) r->now += 20, turn(r);
}
static void key(rig_t *r, int bit)
{
    r->now += 50;
    sk_pen_key(&r->s, 1u << bit, 1u << bit, r->now);
    sk_pen_key(&r->s, 0, 1u << bit, r->now);
}

static void hover_strip(rig_t *r) /* 400 moves along the strip and back */
{
    int ym = r->s.tb.cyi[0] + r->s.tb.ch / 2, x0 = r->s.tb.x0 + 4, x1 = r->s.tb.x1 - 4;
    for (int i = 0; i < 400; i++) {
        int k = i % 200, x = i / 200 % 2 ? x1 - (x1 - x0) * k / 199 : x0 + (x1 - x0) * k / 199;
        pen(r, x, ym + (i % 7) - 3, APP_PEN_IN_RANGE, 0);
    }
}
static void picks(rig_t *r) /* Pencil, Marker, Grey, Black, Big; each looked at for 3 s, the pen over it */
{
    int items[5] = { TI_BRUSH + BR_PENCIL, TI_BRUSH + BR_MARKER, tb_item_of_shade(SH_GREY), tb_item_of_shade(SH_BLACK), TI_SMALL + 2 };
    for (int j = 0; j < 5; j++) {
        int it = items[j], x = r->s.tb.cx[it] + r->s.tb.cell / 2, y = r->s.tb.cyi[it] + r->s.tb.ch / 2;
        for (int k = 0; k < 10; k++) pen(r, x - 50 + k * 5, y, APP_PEN_IN_RANGE, 0);
        pen(r, x, y, APP_PEN_TIP | APP_PEN_IN_RANGE, 30000);
        pen(r, x + 1, y, APP_PEN_TIP | APP_PEN_IN_RANGE, 30000);
        pen(r, x, y, APP_PEN_IN_RANGE, 0);
        CHECK(r->s.tb_on && (it < TI_BLACK ? r->s.sty.brush == it - TI_BRUSH : 1));
        for (int k = 0; k < 300; k++) pen(r, x + (k % 10), y + (k % 5), APP_PEN_IN_RANGE, 0);
    }
}
static void strokes(rig_t *r) /* 20 strokes just under the strip, the pen hovering up to its edge between them */
{
    int below = r->s.tb.y1;
    for (int j = 0; j < 20; j++) {
        int sx = 200 + j * 70, sy = below + 30 + (j % 4) * 30;
        for (int k = 0; k < 8; k++) pen(r, sx, sy - 25 - k * 3, APP_PEN_IN_RANGE, 0);
        for (int k = 0; k < 20; k++) pen(r, sx + k * 6, sy + (k % 5) * 4, APP_PEN_TIP | APP_PEN_IN_RANGE, 40000);
        pen(r, sx + 120, sy, APP_PEN_IN_RANGE, 0);
        for (int k = 0; k < 30; k++) pen(r, sx + 120 + k, sy - k, APP_PEN_IN_RANGE, 0);
        if (j % 5 == 4) {
            pen(r, sx, sy, 0, 0); /* out of range: a pause to look */
            wait(r, 6000);
        }
    }
}

static void test_spin(void) /* a due time nothing consumes: the second ask sleeps */
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    r.s.want_full = 1; /* pending, and no flush follows */
    CHECK(sk_wait_ms(&r.s, r.now) == 0);
    CHECK(sk_wait_ms(&r.s, r.now) > 0);
    rig_end(&r);
}

static void test_session(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    r.s.cleanup_s = 5;
    wait(&r, 45000); /* the start legend and its clean-up are over */
    CHECK(!r.s.note_on);
    int f0 = r.inverted;
    key(&r, 3);
    wait(&r, 200);
    int f_show = r.inverted - f0;
    hover_strip(&r);
    int f_hover = r.inverted - f0 - f_show;
    picks(&r);
    int f_picks = r.inverted - f0 - f_show - f_hover;
    strokes(&r);
    int f_strokes = r.inverted - f0 - f_show - f_hover - f_picks;
    wait(&r, 10000); /* the pen still: the clean-up of what is left */
    int total = r.inverted - f0;
    printf("test_tbflash: spins %d\n", spins);
    CHECK(spins == 0); /* the loop never asks for 0 twice with nothing left to do (idle CPU) */
    printf("test_tbflash: flashes show %d hover %d picks %d strokes %d end %d, total %d\n", f_show, f_hover, f_picks, f_strokes, total - f_show - f_hover - f_picks - f_strokes, total);
    CHECK(f_show == 0);   /* the strip appears as fast ink: no flash on the drawing page */
    CHECK(f_hover == 0);  /* the arrow and the frame: ink only */
    CHECK(f_picks == 0);  /* the pick inverted and its toast: ink only */
    CHECK(f_strokes == 4); /* each 6 s pause, the pen away: one clean-up of the strokes and the strip's traces */
    CHECK(total - f_show - f_hover - f_picks - f_strokes == 0); /* the last pause cleaned it already */
    CHECK(r.s.tb_on);
    wait(&r, 60000);
    CHECK(r.inverted - f0 == total); /* nothing new: never again */
    rig_end(&r);
}

/* The toolbar shown, used and hidden: its ghost may stay while you draw on, but once the pen is away and idle for the
   clean-up time one clean flash (owner 2026-11-03: "it should be gone after a while when you
   stop writing"); never while the pen is near or the strip is shown, and only once */
static void test_hidden_strip_cleaned(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    r.s.cleanup_s = 5;
    wait(&r, 45000);
    int f0 = r.inverted;
    key(&r, 3);
    wait(&r, 200);
    hover_strip(&r);
    picks(&r);
    pen(&r, 600, 600, APP_PEN_IN_RANGE, 0);
    key(&r, 3); /* hidden again */
    wait(&r, 200);
    CHECK(!r.s.tb_on);
    for (int i = 0; i < 2000; i++) pen(&r, 600 + i % 50, 600, APP_PEN_IN_RANGE, 0); /* 10 s of hovering: nothing */
    CHECK(r.inverted == f0);
    pen(&r, 600, 600, 0, 0); /* away */
    wait(&r, 3000);
    CHECK(r.inverted == f0); /* not before the clean-up time */
    wait(&r, 4000);
    CHECK(r.inverted == f0 + 1); /* one flash: the strip's area */
    CHECK(!r.s.ghost);
    wait(&r, 30000);
    CHECK(r.inverted == f0 + 1); /* once */
    rig_end(&r);
}

int main(void)
{
    test_spin();
    test_hidden_strip_cleaned();
    test_session();
    return CHECK_DONE("test_tbflash");
}
