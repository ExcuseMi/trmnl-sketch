/* The mirror picker (owner 2026-11-02: leaving a symmetry mode took up to 4 taps): a tap on Mirror opens a row of the five
   modes under the strip, one tap picks and closes it, a tap elsewhere, the pen out of range or a tablet button closes it
   unchanged. Fast ink only (no flash to open or close), never inks, in all four positions. SPDX-License-Identifier: MIT */
#include "check.h"
#include <stdio.h>
#include "font.h"
#include "util.h"

static void frames(rig_t *r)
{
    rect_t out[SK_MAX_RECTS];
    while (rig_frame(r, out) != 0) {}
}
static void key(rig_t *r, int bit)
{
    r->now += 50;
    sk_pen_key(&r->s, 1u << bit, 1u << bit, r->now);
    sk_pen_key(&r->s, 0, 1u << bit, r->now);
}
static void tap_at(rig_t *r, int ux, int uy)
{
    rig_pen_user(r, ux, uy, APP_PEN_IN_RANGE, 0);
    rig_pen_user(r, ux, uy, APP_PEN_TIP | APP_PEN_IN_RANGE, 30000);
    rig_pen_user(r, ux + 1, uy, APP_PEN_TIP | APP_PEN_IN_RANGE, 30000);
    rig_pen_user(r, ux, uy, APP_PEN_IN_RANGE, 0);
    frames(r);
}
static void open_picker(rig_t *r)
{
    tap_at(r, r->s.tb.cx[TI_MIRROR] + r->s.tb.cell / 2, r->s.tb.cyi[TI_MIRROR] + 30);
}
static void pick(rig_t *r, int m) { tap_at(r, r->s.mp.cx[m] + r->s.mp.cell / 2, r->s.mp.cy + 30); }
static void start(rig_t *r, int orient)
{
    rig_new(r, orient);
    frames(r);
    sk_note_hide(&r->s); /* the start legend */
    key(r, 3);
    frames(r);
    r->now += 4000;
    sk_tick(&r->s, r->now);
    frames(r);
}
static void scribble(rig_t *r)
{
    int sw = r->s.orient % 2 ? TH : TW;
    for (int j = 0; j < 6; j++) {
        int y = r->s.tb.top ? 400 + j * 90 : 300 + j * 90;
        for (int k = 0; k < 20; k++) rig_pen_user(r, 200 + k * (sw - 400) / 20, y + (k % 3) * 10, APP_PEN_TIP | APP_PEN_IN_RANGE, 40000);
        rig_pen_user(r, 300, y, APP_PEN_IN_RANGE, 0);
    }
    rig_pen_user(r, 5, 5, 0, 0);
    frames(r);
}
static void test_open_pick_every_mode(void)
{
    for (int o = 0; o < 4; o++) {
        static rig_t r;
        start(&r, o);
        scribble(&r);
        static uint8_t page[TS * TH];
        memcpy(page, r.s.page.px, sizeof(page));
        for (int from = 0; from < MR_N; from++)
            for (int to = 0; to < MR_N; to++) {
                sk_set_mirror(&r.s, from, r.now += 50);
                r.now += 4000;
                sk_tick(&r.s, r.now);
                frames(&r);
                rig_pen_user(&r, 5, 5, 0, 0); /* no brush cursor in the picture */
                frames(&r);
                int flashes = r.inverted;
                static uint8_t fb0[TS * TH];
                memcpy(fb0, r.fb, sizeof(fb0));
                open_picker(&r);
                CHECK(r.s.mp_on && r.s.sty.mirror == from);
                CHECK(r.inverted == flashes);          /* opening is fast ink: no flash */
                CHECK(memcmp(fb0, r.fb, sizeof(fb0))); /* the row is on the glass */
                CHECK(!memcmp(page, r.s.page.px, sizeof(page)) && !r.s.stroke); /* never inks */
                CHECK(r.s.tb_on && !r.s.note_on);
                pick(&r, to);
                CHECK(!r.s.mp_on && r.s.sty.mirror == to); /* one tap: Off from any mode too */
                CHECK(!memcmp(page, r.s.page.px, sizeof(page)) && !r.s.stroke);
                CHECK(r.s.tb_on);
                if (to == from) {
                    CHECK(r.inverted == flashes);      /* the same mode: closed, nothing else happens */
                    CHECK(!r.s.note_on);
                    rig_pen_user(&r, 5, 5, 0, 0); /* the brush cursor goes with the pen */
                    frames(&r);
                    CHECK(!memcmp(fb0, r.fb, sizeof(fb0)));
                }
            }
        rig_end(&r);
    }
}

static void test_close_without_change(void)
{
    for (int o = 0; o < 4; o++) {
        static rig_t r;
        start(&r, o);
        scribble(&r);
        sk_set_mirror(&r.s, MR_4, r.now += 50);
        r.now += 4000;
        sk_tick(&r.s, r.now);
        frames(&r);
        static uint8_t page[TS * TH], fb0[TS * TH];
        rig_pen_user(&r, 5, 5, 0, 0);
        frames(&r);
        memcpy(page, r.s.page.px, sizeof(page));
        memcpy(fb0, r.fb, sizeof(fb0));
        int sw = o % 2 ? TH : TW, sh = o % 2 ? TW : TH;
        int how;
        for (how = 0; how < 4; how++) {
            open_picker(&r);
            CHECK(r.s.mp_on);
            if (how == 0) tap_at(&r, sw / 2, sh / 2 + 100);    /* a tap on the page */
            else if (how == 1) rig_pen_user(&r, 5, 5, 0, 0);   /* the pen out of range */
            else if (how == 2) key(&r, 0);                     /* a tablet button (undo of nothing: no stroke after the scribble? see below) */
            else tap_at(&r, r.s.tb.cx[TI_MIRROR] + r.s.tb.cell / 2, r.s.tb.cyi[TI_MIRROR] + 30); /* Mirror again */
            frames(&r);
            CHECK(!r.s.mp_on); /* closed by the action itself, not by the pen leaving afterwards */
            rig_pen_user(&r, 5, 5, 0, 0);
            frames(&r);
            CHECK(r.s.sty.mirror == MR_4 || how == 2);
            if (how != 2) {
                CHECK(!memcmp(page, r.s.page.px, sizeof(page))); /* the tap that closed it inked nothing */
                CHECK(!memcmp(fb0, r.fb, sizeof(fb0)));          /* and the screen is as before, pixel for pixel */
            } else {
                sk_redo(&r.s, r.now += 50);
                memcpy(page, r.s.page.px, sizeof(page));
                memcpy(fb0, r.fb, sizeof(fb0));
            }
        }
        rig_end(&r);
    }
}

static void test_no_ink_under_the_row(void)
{
    for (int o = 0; o < 4; o++) {
        static rig_t r;
        start(&r, o);
        static uint8_t page[TS * TH];
        memcpy(page, r.s.page.px, sizeof(page));
        int sw = o % 2 ? TH : TW, sh = o % 2 ? TW : TH;
        (void)sw;
        open_picker(&r);
        int y = (r.s.mp.y0 + r.s.mp.y1) / 2, x0 = r.s.mp.x0 + 2;
        /* press in the frame and drag along the row and out onto the page, then hover over the row's gaps */
        for (int k = 0; k < 30; k++) rig_pen_user(&r, x0 + k * 20, y, APP_PEN_TIP | APP_PEN_IN_RANGE, 40000);
        rig_pen_user(&r, x0 + 600, y + 200 < sh ? y + 200 : y - 200, APP_PEN_TIP | APP_PEN_IN_RANGE, 40000);
        rig_pen_user(&r, x0 + 600, y + 200 < sh ? y + 200 : y - 200, APP_PEN_IN_RANGE, 0);
        frames(&r);
        CHECK(!memcmp(page, r.s.page.px, sizeof(page)) && !r.s.stroke);
        CHECK(!r.s.mp_on); /* the press closed it (or picked) and nothing inked until the pen lifted */
        /* hovering over the open row shows the arrow, never the brush cursor; leaving puts the screen back exactly */
        open_picker(&r);
        rig_pen_user(&r, 5, 5, 0, 0); /* pen away closes: reopen with the pen kept in range */
        frames(&r);
        CHECK(!r.s.mp_on);
        open_picker(&r);
        static uint8_t fb0[TS * TH];
        memcpy(fb0, r.fb, sizeof(fb0));
        rig_pen_user(&r, r.s.mp.cx[2] + 40, r.s.mp.cy + 40, APP_PEN_IN_RANGE, 0);
        CHECK(r.s.mp_on && r.s.tb_ptr && !r.s.cursor);
        rig_pen_user(&r, r.s.mp.cx[3] + 40, r.s.mp.cy + 40, APP_PEN_IN_RANGE, 0);
        frames(&r);
        CHECK(r.s.mp_on && r.s.tb_ptr);
        rig_pen_user(&r, r.s.mp.cx[3] + 40, r.s.mp.y1 + 150 < sh ? r.s.mp.y1 + 150 : r.s.mp.y0 - 150, APP_PEN_IN_RANGE, 0);
        frames(&r);
        CHECK(r.s.mp_on && !r.s.tb_ptr && r.s.cursor); /* now the brush cursor */
        for (int yy = r.s.mp_box.y0; yy <= r.s.mp_box.y1; yy++) /* the row itself is whole again where the arrow was */
            CHECK(!memcmp(fb0 + yy * TS + r.s.mp_box.x0 / 8, r.fb + yy * TS + r.s.mp_box.x0 / 8, (size_t)(r.s.mp_box.x1 / 8 - r.s.mp_box.x0 / 8 + 1)));
        rig_pen_user(&r, r.s.mp.cx[3] + 40, r.s.mp.cy + 40, APP_PEN_IN_RANGE, 0);
        rig_pen_user(&r, 5, 5, 0, 0);
        frames(&r);
        CHECK(!r.s.mp_on && !r.s.tb_ptr);
        rig_end(&r);
    }
}

static void test_layout_every_position(void)
{
    const font_face_t *f = font_get(FONT_BOLD, 24);
    for (int o = 0; o < 4; o++) {
        rig_t r;
        start(&r, o);
        int sw = o % 2 ? TH : TW, sh = o % 2 ? TW : TH;
        open_picker(&r);
        const mp_layout_t *L = &r.s.mp;
        const tb_layout_t *T = &r.s.tb;
        CHECK(r.s.mp_on);
        CHECK(L->x0 >= 0 && L->x1 < sw && L->y0 >= 0 && L->y1 < sh);                 /* on the screen */
        CHECK(L->y0 > T->y1 || L->y1 < T->y0);                                        /* beside the strip, not over it */
        CHECK(T->top ? L->y0 > T->y1 : L->y1 < T->y0);
        CHECK(L->x1 <= T->cx[TI_MIRROR] + T->cell + 8 && L->x1 >= T->cx[TI_MIRROR]); /* under Mirror */
        for (int m = 0; m < MR_N; m++) {
            CHECK(((font_width64(f, mp_label(m), 0) + 32) >> 6) <= L->cell);        /* every label fits its cell */
            CHECK(mp_hit(L, L->cx[m] + L->cell / 2, L->cy + 10) == m);
        }
        CHECK(mp_hit(L, L->x0 - 1, L->cy) == -1 && mp_hit(L, L->cx[0] + 10, L->y1 + 1) == -1);
        rig_end(&r);
    }
}

int main(void)
{
    test_open_pick_every_mode();
    test_close_without_change();
    test_no_ink_under_the_row();
    test_layout_every_position();
    return CHECK_DONE("test_mrpick");
}
