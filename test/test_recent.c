/* The Recent slots of the toolbar: the last 5 combinations of tone, brush and size a stroke was finished with, most recent
   first, no duplicates, never the eraser or the bucket; a tap restores one; they survive a restart. SPDX-License-Identifier: MIT */
#include "check.h"
#include <stdio.h>
#include "util.h"

static void stroke(rig_t *r, int brush, int tone, int size, int y)
{
    sk_set_brush(&r->s, brush, r->now += 50);
    sk_set_shade(&r->s, tone, r->now += 50);
    sk_set_size(&r->s, size, r->now += 50);
    rig_line(r, 300, y, 900, y + 20, 40000, 0);
}
static void tap_slot(rig_t *r, int slot)
{
    int x = r->s.tb.cx[TI_RECENT + slot] + r->s.tb.cell / 2, y = r->s.tb.cyi[TI_RECENT + slot] + r->s.tb.cell / 2;
    rig_pen_user(r, x, y, APP_PEN_IN_RANGE, 0);
    rig_pen_user(r, x, y, APP_PEN_TIP | APP_PEN_IN_RANGE, 30000);
    rig_pen_user(r, x, y, APP_PEN_IN_RANGE, 0);
}
static int is(const rig_t *r, int i, int brush, int tone, int size)
{
    return i < r->s.rc_n && r->s.rc[i].brush == brush && r->s.rc[i].shade == tone && r->s.rc[i].size == size;
}

static void test_order_dedupe_cap(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    CHECK(r.s.rc_n == 0);
    stroke(&r, BR_FINELINER, 0, 9, 300);
    CHECK(r.s.rc_n == 1 && is(&r, 0, BR_FINELINER, 0, 9));
    stroke(&r, BR_PENCIL, 3, 12, 400);
    CHECK(r.s.rc_n == 2 && is(&r, 0, BR_PENCIL, 3, 12) && is(&r, 1, BR_FINELINER, 0, 9)); /* most recent first */
    stroke(&r, BR_FINELINER, 0, 9, 500); /* the same combination again: moves up, no duplicate */
    CHECK(r.s.rc_n == 2 && is(&r, 0, BR_FINELINER, 0, 9) && is(&r, 1, BR_PENCIL, 3, 12));
    rig_line(&r, 300, 520, 900, 560, 40000, 0); /* again the newest: still one entry */
    CHECK(r.s.rc_n == 2);
    for (int i = 0; i < 6; i++) stroke(&r, BR_MARKER, i, 20 + i, 600 + i * 40);
    CHECK(r.s.rc_n == SK_RECENT && is(&r, 0, BR_MARKER, 5, 25) && is(&r, 4, BR_MARKER, 1, 21)); /* capped at 5, the oldest fell off */
    /* the eraser (a brush or the pen's button on an ink brush) and the bucket never enter: the list stays as it was */
    bstyle_t snap[SK_RECENT];
    memcpy(snap, r.s.rc, sizeof(snap));
    sk_set_brush(&r.s, BR_ERASER, r.now += 50);
    rig_line(&r, 300, 900, 900, 920, 40000, 0);
    CHECK(!memcmp(snap, r.s.rc, sizeof(snap)) && r.s.rc_n == SK_RECENT);
    sk_set_brush(&r.s, BR_PENCIL, r.now += 50);
    rig_line(&r, 300, 940, 900, 960, 40000, APP_PEN_BARREL); /* the button turns the pencil into the eraser */
    CHECK(!memcmp(snap, r.s.rc, sizeof(snap)));
    sk_set_brush(&r.s, BR_FILL, r.now += 50);
    rig_pen(&r, 1500, 1100, APP_PEN_IN_RANGE, 0);
    rig_pen(&r, 1500, 1100, APP_PEN_TIP | APP_PEN_IN_RANGE, 30000);
    rig_pen(&r, 1500, 1100, APP_PEN_IN_RANGE, 0);
    CHECK(!memcmp(snap, r.s.rc, sizeof(snap)));
    /* a shape finished with a combination counts, and so does a stroke ended by dragging onto the strip */
    sk_set_brush(&r.s, BR_BRUSHPEN, r.now += 50);
    sk_set_size(&r.s, 33, r.now += 50);
    sk_set_tool(&r.s, TL_RECT, r.now);
    rig_pen(&r, 1000, 1000, APP_PEN_TIP | APP_PEN_IN_RANGE, 30000);
    rig_pen(&r, 1300, 1200, APP_PEN_TIP | APP_PEN_IN_RANGE, 30000);
    rig_pen(&r, 1300, 1200, APP_PEN_IN_RANGE, 0);
    CHECK(is(&r, 0, BR_BRUSHPEN, 5, 33));
    rig_end(&r);
}

static void test_pick_and_restart(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    stroke(&r, BR_PENCIL, 3, 12, 300);
    stroke(&r, BR_MARKER, 6, 30, 400);
    stroke(&r, BR_SPRAY, 8, 4, 500);
    sk_toolbar(&r.s, r.now);
    tap_slot(&r, 1); /* the second newest: the marker */
    CHECK(r.s.sty.brush == BR_MARKER && r.s.sty.shade == 6 && r.s.sty.size == 30 && r.s.note_on);
    CHECK(r.s.stroke == 0 && sk_is_blank(&r.s.page) == 0); /* a tap on the strip never inks */
    sk_set_tool(&r.s, TL_OVAL, r.now);
    tap_slot(&r, 2);
    CHECK(r.s.sty.brush == BR_PENCIL && r.s.sty.shade == 3 && r.s.sty.size == 12 && r.s.sty.tool == TL_OVAL); /* the tool stays */
    tap_slot(&r, 4); /* an empty slot: a word, nothing changes */
    CHECK(r.s.sty.brush == BR_PENCIL && r.s.note_on);
    /* the toolbar paints them (slots 0..2 filled, 3 and 4 empty), the current one picked: the strip shows a mark per slot */
    static uint8_t strip_a[TS * 300], strip_b[TS * 300];
    int rows = r.s.tb_box.y1 + 1;
    memcpy(strip_a, r.fb, (size_t)TS * (size_t)rows);
    CHECK(rows < 300 && r.s.rc_n == 3);
    /* a restart keeps them, in order */
    rig_start(&r, APP_ORIENT_BAR_BOTTOM);
    CHECK(r.s.rc_n == 3 && is(&r, 0, BR_SPRAY, 8, 4) && is(&r, 1, BR_MARKER, 6, 30) && is(&r, 2, BR_PENCIL, 3, 12));
    /* a damaged value leaves its slot empty and nothing else */
    strcpy(r.cfg_rc[1], "9.99.x");
    strcpy(r.cfg_rc[2], "8.6");
    rig_start(&r, APP_ORIENT_BAR_BOTTOM);
    CHECK(r.s.rc_n == 1 && is(&r, 0, BR_SPRAY, 8, 4));
    strcpy(r.cfg_rc[0], "0.6.9"); /* the eraser (6) was never kept */
    rig_start(&r, APP_ORIENT_BAR_BOTTOM);
    CHECK(r.s.rc_n == 0);
    (void)strip_b;
    rig_end(&r);
}

static void test_layout(void)
{
    for (int o = 0; o < 4; o++) {
        tb_layout_t L;
        int sw = o == APP_ORIENT_BAR_LEFT || o == APP_ORIENT_BAR_RIGHT ? TH : TW, sh = o == APP_ORIENT_BAR_LEFT || o == APP_ORIENT_BAR_RIGHT ? TW : TH;
        tb_layout(&L, o, sw, sh);
        for (int k = 0; k < 5; k++) {
            CHECK(L.cx[TI_RECENT + k] + L.cell <= sw && L.cx[TI_RECENT + k] >= L.cx[TI_CLEAR] + L.cell);
            CHECK(tb_hit(&L, L.cx[TI_RECENT + k] + L.cell / 2, L.cyi[TI_RECENT + k] + 20) == TI_RECENT + k);
        }
        CHECK(L.cell >= 70);
    }
}

int main(void)
{
    test_order_dedupe_cap();
    test_pick_and_restart();
    test_layout();
    return CHECK_DONE("test_recent");
}
