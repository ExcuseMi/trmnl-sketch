/* Flashing with the toolbar up (owner 2026-11-02: "It's flashing a lot with the full toolbar on there"). A flash is the
   black/white blink of sk_flash (an inverted half, then a clean FULL refresh): rig_t.inverted counts them. A scripted
   session with the strip shown: the pen hovers 400 moves over it, picks 5 buttons (looking 3 s at each), draws 20
   strokes near it with a pause every 5. The arrow, the item frame, the pick shown inverted, the pills, the brush cursor
   and a toast change only black and white pixels: they go as fast ink and never ask for a flash. Strokes leave traces:
   the idle clean-up (rule b) flashes the page once enough of them were drawn (SK_CLEAN_TRACES), not after every pause.
   Before the fix: 11 flashes (a whole-page flash after every toast, one at every pause). SPDX-License-Identifier: MIT */
#include "check.h"
#include <stdio.h>
#include "util.h"

static void turn(rig_t *r)
{
    rect_t out[SK_MAX_RECTS];
    sk_tick(&r->s, r->now);
    rig_flush(r, r->now, out);
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

static void test_session(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    r.s.cleanup_s = 5;
    wait(&r, 45000); /* the start legend and its clean-up are over */
    CHECK(!r.s.note_on && !r.s.ghost);
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
    printf("test_tbflash: flashes show %d hover %d picks %d strokes %d end %d, total %d\n", f_show, f_hover, f_picks, f_strokes, total - f_show - f_hover - f_picks - f_strokes, total);
    CHECK(f_show == 1);   /* the strip appears clean, once */
    CHECK(f_hover == 0);  /* the arrow and the frame: ink only */
    CHECK(f_picks == 0);  /* the pick inverted and its toast: ink only */
    CHECK(f_strokes <= 2); /* 20 strokes: the idle clean-up after enough traces, not at every pause */
    CHECK(total <= 4);
    CHECK(r.s.tb_on);
    rig_end(&r);
}

int main(void)
{
    test_session();
    return CHECK_DONE("test_tbflash");
}
