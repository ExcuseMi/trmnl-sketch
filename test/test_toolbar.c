/* The toolbar: the same button shows and hides it, only the strip is refreshed (clean), the page under it comes back
   pixel for pixel, taps on it never ink the page, the brush cursors. SPDX-License-Identifier: MIT */
#include "check.h"
#include <stdio.h>
#include "gallery.h"
#include "util.h"
#include "oldsizes.h"

static void key(rig_t *r, int bit)
{
    r->now += 50;
    sk_pen_key(&r->s, 1u << bit, 1u << bit, r->now);
    sk_pen_key(&r->s, 0, 1u << bit, r->now);
}
static void idle(rig_t *r, uint32_t ms)
{
    r->now += ms;
    sk_tick(&r->s, r->now);
}
/* the pen leaves and the corner notes run out */
static void settle(rig_t *r)
{
    rig_pen(r, 10, 600, 0, 0);
    idle(r, 3000);
}
static int in_box(const box_t *b, int x, int y) { return x >= b->x0 && x <= b->x1 && y >= b->y0 && y <= b->y1; }
static int px_black(const rig_t *r, int x, int y) { return !cv_get(&r->s.page, x, y); }

/* a pen tap on item i (landscape: the user's frame is the framebuffer) */
static void tap(rig_t *r, int item)
{
    int x = r->s.tb.cx[item] + r->s.tb.cell / 2, y = r->s.tb.cyi[item] + r->s.tb.cell / 2;
    rig_pen(r, x, y, APP_PEN_IN_RANGE, 0);
    rig_pen(r, x, y, APP_PEN_TIP | APP_PEN_IN_RANGE, 30000);
    rig_pen(r, x + 1, y, APP_PEN_TIP | APP_PEN_IN_RANGE, 30000);
    rig_pen(r, x, y, APP_PEN_IN_RANGE, 0);
}

static void scribble(rig_t *r) /* ink across the places the strip will cover and below */
{
    rig_line(r, 100, 20, 1700, 40, 50000, 0);
    rig_line(r, 200, 90, 900, 110, 50000, 0);
    rig_line(r, 300, 60, 340, 700, 50000, 0);
    rig_line(r, 500, 400, 1500, 420, 50000, 0);
    settle(r);
}

static void test_labels_follow_tools(void)
{
    for (int b = 0; b < BR_N; b++) CHECK(!strcmp(tb_label(TI_BRUSH + b), brush_name(b))); /* the button that picks a tool carries its name */
    CHECK(!strcmp(tb_label(TI_BRUSH), "Brush") && !strcmp(tb_label(TI_MIRROR), "Mirror"));
}

static void test_toggle_and_restore(void)
{
    for (int o = 0; o < 4; o++) { /* every position of the X */
        rig_t r;
        rig_new(&r, o);
        for (int y = 0; y < TH; y += 3) /* ink all over, put on the page directly (the pen mapping of the rig is landscape only) */
            for (int x = (y * 7) % 90; x < TW; x += 91) cv_disc(&r.s.page, x, y, 5, 0, 0);
        sk_show_page(&r.s);
        sk_note_hide(&r.s);
        static uint8_t page[TS * TH];
        memcpy(page, r.s.page.px, sizeof(page));
        CHECK(same_page_and_fb(&r) && !r.s.tb_on);
        key(&r, 3); /* tablet button 4 */
        CHECK(r.s.tb_on && !same_page_and_fb(&r));
        CHECK(!memcmp(page, r.s.page.px, sizeof(page))); /* the page itself is not touched */
        const box_t *tb = &r.s.tb_box;
        int outside = 0; /* the screen differs from the page inside the strip only */
        for (int y = 0; y < TH; y++)
            for (int x = 0; x < TW; x++)
                if (cv_get(&r.s.page, x, y) != ((r.fb[y * TS + x / 8] >> (7 - x % 8)) & 1) && !in_box(tb, x, y) && !in_box(&r.s.pl_box, x, y)) outside++; /* the pills are over the page too */
        CHECK(outside == 0);
        int bw = tb->x1 - tb->x0 + 1, bh = tb->y1 - tb->y0 + 1, lng = bw > bh ? bw : bh, sht = bw > bh ? bh : bw;
        CHECK(lng >= 1404 && sht < 320 && sht > 60); /* a thin strip along one edge */
        key(&r, 3); /* the same button hides it */
        CHECK(!r.s.tb_on && same_page_and_fb(&r)); /* pixel for pixel */
        CHECK(!memcmp(page, r.s.page.px, sizeof(page)));
        rig_end(&r);
    }
}

static void test_flash_only_strip(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    scribble(&r);
    rect_t out[SK_MAX_RECTS];
    while (rig_frame(&r, out) != 0) {} /* what is pending goes out */
    key(&r, 3);
    int n = rig_frame(&r, out);
    CHECK(n == 1 && out[0].flash == 1); /* one rectangle, clean (flashes), not the whole screen */
    CHECK(out[0].x == 0 && out[0].y == 0 && out[0].w == TW && out[0].h == r.s.tb.y1 + 1 && out[0].h < 280);
    CHECK((long)out[0].w * out[0].h < (long)TW * TH / 5);
    CHECK(rig_frame(&r, out) == 0); /* nothing else follows */
    CHECK(sk_next_due(&r.s, r.now) > 100);
    key(&r, 3); /* hiding: the strip left the page, one whole-page flash (rule a), which covers the strip */
    n = rig_frame(&r, out);
    CHECK(n == SK_FULL);
    CHECK(rig_frame(&r, out) == 0);
    /* the portrait positions: the strip is a column of the panel, byte aligned */
    rig_end(&r);
    rig_new(&r, APP_ORIENT_BAR_LEFT);
    while (rig_frame(&r, out) != 0) {}
    key(&r, 3);
    n = rig_frame(&r, out);
    CHECK(n == 1 && out[0].flash == 1 && out[0].x % 8 == 0 && out[0].w % 8 == 0 && out[0].h == TH && out[0].w < 280 + 8);
    rig_end(&r);
}

static void test_taps_never_ink(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    scribble(&r);
    key(&r, 3);
    static uint8_t page[TS * TH];
    memcpy(page, r.s.page.px, sizeof(page));
    int fulls = r.fulls;
    for (int i = 0; i < TI_N; i++) {
        tap(&r, i);
        CHECK(!memcmp(page, r.s.page.px, sizeof(page))); /* a tap on the strip never inks */
        CHECK(!r.s.stroke);
        CHECK(r.s.note_on); /* a toast names the choice ... */
        idle(&r, 3000);
        if (i == TI_MIRROR) /* the symmetry cycles through its modes and is off again after the last (test_mirror.c) */
            for (int k = 1; k < MR_N; k++) {
                tap(&r, i);
                CHECK(r.s.sty.mirror == (k + 1) % MR_N);
                idle(&r, 3000);
            }
        CHECK(i != TI_MIRROR || r.s.sty.mirror == MR_OFF);
    }
    /* ... below the strip, never over it */
    sk_set_brush(&r.s, BR_PENCIL, r.now += 50);
    CHECK(r.s.note_on && r.s.note.y0 > r.s.tb_box.y1);
    CHECK(r.fulls == fulls);
    /* a drag across the strip, and from the strip out onto the page, inks nothing */
    rig_pen(&r, 400, 50, APP_PEN_TIP | APP_PEN_IN_RANGE, 40000);
    for (int x = 400; x < 1500; x += 20) rig_pen(&r, x, 50 + (x - 400) / 4, APP_PEN_TIP | APP_PEN_IN_RANGE, 40000);
    rig_pen(&r, 1500, 700, APP_PEN_IN_RANGE, 0);
    CHECK(!memcmp(page, r.s.page.px, sizeof(page)));
    CHECK(!r.s.stroke);
    /* undo, redo and clear are not on the strip: the tablet's buttons 1, 2 and 5 only */
    key(&r, 3);
    key(&r, 3);
    sk_set_brush(&r.s, BR_PEN, r.now += 50);
    rig_line(&r, 600, 600, 900, 650, 40000, 0);
    static uint8_t drawn[TS * TH];
    memcpy(drawn, r.s.page.px, sizeof(drawn));
    key(&r, 0);
    CHECK(!memcmp(page, r.s.page.px, sizeof(page)));
    key(&r, 1);
    CHECK(!memcmp(drawn, r.s.page.px, sizeof(drawn)));
    key(&r, 4);
    CHECK(sk_is_blank(&r.s.page));
    key(&r, 0); /* and clear is one undo step */
    CHECK(!memcmp(drawn, r.s.page.px, sizeof(drawn)));
    idle(&r, 3000);
    rig_end(&r);
}

static void test_draw_near_strip(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    key(&r, 3);
    rig_line(&r, 500, 900, 700, 910, 40000, 0);
    idle(&r, 3000);
    int below = r.s.tb.y1 + 1;
    static uint8_t strip[TS * 300]; /* the strip is one row */
    memcpy(strip, r.fb, (size_t)TS * (size_t)below);
    /* drawing elsewhere stays fast: ink below the strip is a few small partial rectangles, no flash, no full refresh */
    rect_t out[SK_MAX_RECTS];
    while (rig_frame(&r, out) != 0) {}
    int parts = 0;
    rig_stroke(&r, (int[]){ 500, 600, 600, 650, 700, 600, 800, 680 }, 4, 40000, 0);
    int n = rig_frame(&r, out);
    CHECK(n > 0 && n != SK_FULL);
    for (int i = 0; i < n; i++) {
        CHECK(!out[i].flash);
        parts += out[i].w * out[i].h;
    }
    CHECK(parts < 300 * 300);
    /* a stroke reaching up to the strip: the ink may go right up to it, the strip is not eaten */
    rig_line(&r, 700, 700, 700, below + 3, 60000, 0);
    rig_line(&r, 900, 700, 1100, below + 14, 60000, 0);
    settle(&r);
    CHECK(!memcmp(strip, r.fb, (size_t)TS * (size_t)below)); /* the strip is as it was: ink up to its edge does not eat it */
    CHECK(px_black(&r, 700, below + 10));
    /* a hover near the strip draws no cursor into it */
    rig_pen(&r, 800, r.s.tb.y1 + 4, APP_PEN_IN_RANGE, 0);
    CHECK(!r.s.cursor);
    rig_pen(&r, 800, 400, APP_PEN_IN_RANGE, 0);
    CHECK(r.s.cursor);
    key(&r, 3);
    settle(&r);
    CHECK(same_page_and_fb(&r)); /* ink under the strip's edge, and the page is whole again */
    rig_end(&r);
}

static void test_layout_all_positions(void)
{
    for (int o = 0; o < 4; o++) {
        tb_layout_t L;
        int sw = o == APP_ORIENT_BAR_LEFT || o == APP_ORIENT_BAR_RIGHT ? TH : TW, sh = o == APP_ORIENT_BAR_LEFT || o == APP_ORIENT_BAR_RIGHT ? TW : TH;
        tb_layout(&L, o, sw, sh);
        CHECK(L.cell >= 90 && L.ch >= 96 && L.x0 == 0 && L.x1 == sw - 1 && L.y1 - L.y0 + 1 < 140); /* one short row, in every position */
        for (int i = 1; i < TI_N; i++) CHECK(L.cyi[i] == L.cyi[0]);
        CHECK(L.top == (o != APP_ORIENT_BAR_TOP)); /* away from the touch bar */
        CHECK(L.top ? L.y0 == 0 : L.y1 == sh - 1);
        for (int i = 0; i < TI_N; i++) {
            CHECK(L.cx[i] >= 0 && L.cx[i] + L.cell <= sw && (i == 0 || L.cx[i] >= L.cx[i - 1] + L.cell)); /* inside, in order, no overlap */
            int cx = L.cx[i] + L.cell / 2, cy = L.cyi[i] + L.ch / 2, fx, fy, ux, uy;
            CHECK(tb_in(&L, cx, cy) && tb_hit(&L, cx, cy) == i);
            /* through the panel and back: what a pen on the panel hits */
            app_orient_to_fb(o, TW, TH, cx, cy, &fx, &fy);
            gal_from_fb(o, TW, TH, fx, fy, &ux, &uy);
            CHECK(ux == cx && uy == cy && tb_hit(&L, ux, uy) == i);
        }
        CHECK(!tb_in(&L, sw / 2, L.top ? L.y1 + 1 : L.y0 - 1) && tb_hit(&L, sw / 2, L.top ? L.y1 + 1 : L.y0 - 1) == -1);
        CHECK(tb_hit(&L, 2, L.cy + 2) == -1); /* the margin is not an item */
    }
}

static void test_brush_cursor_on_screen(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    rig_line(&r, 300, 500, 900, 520, 65000, 0);
    settle(&r);
    static uint8_t page[TS * TH];
    memcpy(page, r.s.page.px, sizeof(page));
    for (int b = 0; b < BR_N; b++) {
        sk_set_brush(&r.s, b, r.now += 50);
        idle(&r, 3000);
        for (int size = 0; size < 3; size++) {
            sk_set_size(&r.s, size, r.now += 50);
            idle(&r, 3000);
            rig_pen(&r, 1000, 700, APP_PEN_IN_RANGE, 0);
            rig_pen(&r, 600, 505, APP_PEN_IN_RANGE, 0);
            int sz = brush_size(b, size);
            CHECK(r.s.cursor && r.s.cb == b && r.s.cr == (brush_cursor_r)(b, sz));
            /* the screen shows the shape black, a 2 px white outline around it, and the page elsewhere */
            int r0 = r.s.cr + BR_CUR_OUT, wrong = 0, blacks = 0;
            for (int dy = -r0; dy <= r0; dy++)
                for (int dx = -r0; dx <= r0; dx++) {
                    int page_px = cv_get(&r.s.page, 600 + dx, 505 + dy), scr = (r.fb[(505 + dy) * TS + (600 + dx) / 8] >> (7 - (600 + dx) % 8)) & 1;
                    int shape = (brush_cursor_px)(b, sz, dx, dy), near = 0;
                    for (int y = -2; y <= 2; y++)
                        for (int x = -2; x <= 2; x++) near |= (brush_cursor_px)(b, sz, dx + x, dy + y);
                    wrong += scr != (shape ? 0 : near ? 1 : page_px);
                    blacks += shape && !scr;
                }
            CHECK(blacks > 20);
            CHECK(wrong == 0);
            CHECK(!memcmp(page, r.s.page.px, sizeof(page)));
            rig_pen(&r, 10, 900, 0, 0); /* the pen leaves: no mark is left */
            CHECK(!r.s.cursor && same_page_and_fb(&r));
        }
    }
    /* the pen's button shows the eraser's cursor, whatever is picked */
    sk_set_brush(&r.s, BR_PENCIL, r.now += 50);
    idle(&r, 3000);
    rig_pen(&r, 600, 505, APP_PEN_IN_RANGE | APP_PEN_BARREL, 0);
    CHECK(r.s.cursor && r.s.cb == BR_ERASER);
    rig_pen(&r, 10, 900, 0, 0);
    CHECK(same_page_and_fb(&r));
    rig_end(&r);
}

/* every brush: a stroke, undo, redo and a restart of the app give the same pixels */
static void test_brushes_through_the_app(void)
{
    for (int b = 0; b < BR_N; b++) /* (the bucket draws no stroke: test_fill.c) */
        for (int k = 0; k < 3 && b != BR_FILL; k++) {
            int sh = k == 0 ? SH_BLACK : k == 1 ? SH_GREY : SH_WHITE;
            rig_t r;
            rig_new(&r, APP_ORIENT_BAR_BOTTOM);
            rig_line(&r, 200, 400, 1400, 420, 65000, 0);
            settle(&r);
            static uint8_t before[TS * TH], after[TS * TH];
            memcpy(before, r.s.page.px, sizeof(before));
            sk_set_brush(&r.s, b, r.now += 50);
            sk_set_shade(&r.s, sh, r.now += 50);
            sk_set_size(&r.s, b % 3, r.now += 50);
            int p[] = { 300, 380, 500, 450, 700, 380, 900, 460, 1100, 380, 1300, 450 };
            for (int i = 0; i < 6; i++) rig_pen(&r, p[2 * i], p[2 * i + 1], APP_PEN_TIP | APP_PEN_IN_RANGE, 8000 + 10000 * (unsigned)i);
            rig_pen(&r, 1300, 450, APP_PEN_IN_RANGE, 0);
            settle(&r);
            memcpy(after, r.s.page.px, sizeof(after));
            CHECK(memcmp(before, after, sizeof(before))); /* it drew */
            CHECK(same_page_and_fb(&r));
            sk_undo(&r.s, r.now += 50);
            CHECK(!memcmp(before, r.s.page.px, sizeof(before)));
            sk_redo(&r.s, r.now += 50);
            CHECK(!memcmp(after, r.s.page.px, sizeof(after))); /* the same pixels */
            sk_undo(&r.s, r.now += 50);
            sk_redo(&r.s, r.now += 50);
            CHECK(!memcmp(after, r.s.page.px, sizeof(after)));
            sk_save_now(&r.s);
            r.keep_tool = 1;
            rig_start(&r, APP_ORIENT_BAR_BOTTOM); /* the page file comes back as it was */
            CHECK(!memcmp(after, r.s.page.px, sizeof(after)));
            CHECK(r.s.sty.brush == BR_BRUSH && r.s.sty.shade == sh); /* the colour is kept; the tool is the Brush again (owner 2026-11-02) */
            rig_end(&r);
        }
}

static void test_tail_and_eraser_toggle(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    sk_set_brush(&r.s, BR_PEN, r.now += 50);
    idle(&r, 3000);
    for (int x = 400; x <= 800; x += 20) rig_pen(&r, x, 500, APP_PEN_TIP | APP_PEN_IN_RANGE, 50000);
    rig_pen(&r, 800, 500, APP_PEN_IN_RANGE, 0);
    int reach = 0; /* the stroke ends tapering out beyond its last point */
    for (int x = 801; x < 830; x++)
        if (px_black(&r, x, 500)) reach = x - 800;
    CHECK(reach >= 8);
    int start_w = 0, mid_w = 0; /* and swells from nothing at the start */
    for (int y = 480; y < 520; y++) start_w += px_black(&r, 402, y), mid_w += px_black(&r, 600, y);
    CHECK(start_w >= 1 && mid_w > start_w * 2);
    /* the eraser button: eraser brush and back to the brush that was */
    key(&r, 2);
    CHECK(r.s.sty.brush == BR_ERASER);
    key(&r, 2);
    CHECK(r.s.sty.brush == BR_PEN);
    rig_end(&r);
}

/* ---- the arrow pointer and the hover frame on the open strip ---- */

static void push(uint8_t *panel, const rig_t *r, const rect_t *out, int n)
{
    if (n == SK_FULL) return (void)memcpy(panel, r->fb, (size_t)TS * TH);
    for (int i = 0; i < n; i++)
        for (int y = out[i].y; y < out[i].y + out[i].h; y++) memcpy(panel + y * TS + out[i].x / 8, r->fb + y * TS + out[i].x / 8, (size_t)out[i].w / 8);
}
static int rect_hits(const rect_t *q, const box_t *b)
{
    return !box_empty(b) && q->x <= b->x1 && q->x + q->w > b->x0 && q->y <= b->y1 && q->y + q->h > b->y0;
}
static void item_fb_box(const rig_t *r, int i, box_t *b)
{
    *b = (box_t){ r->s.tb.cx[i], r->s.tb.cyi[i], r->s.tb.cx[i] + r->s.tb.cell - 1, r->s.tb.cyi[i] + r->s.tb.ch - 1 };
}

static void test_pointer_over_the_strip(void)
{
    static rig_t r;
    static uint8_t panel[TS * TH], clean[TS * TH];
    rect_t out[SK_MAX_RECTS];
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    scribble(&r);
    key(&r, 3);
    settle(&r);
    push(panel, &r, out, rig_frame(&r, out));
    memcpy(clean, r.fb, sizeof(clean));
    CHECK(!r.s.tb_ptr && r.s.tb_hot == -1);
    int xa = r.s.tb.cx[2] + r.s.tb.cell / 2, xb = r.s.tb.cx[4] + r.s.tb.cell / 2, y = r.s.tb.cyi[2] + r.s.tb.cell / 2;
    rig_pen(&r, xa, y, APP_PEN_IN_RANGE, 0);
    r.now += 200;
    int n = rig_frame(&r, out);
    CHECK(r.s.tb_ptr && r.s.tb_hot == 2 && !r.s.cursor && n > 0 && n != SK_FULL);
    push(panel, &r, out, n);
    CHECK(!memcmp(panel, r.fb, sizeof(panel)) && memcmp(clean, r.fb, sizeof(clean))); /* arrow and frame are drawn */
    /* onto another item: only the old and the new item and the arrow's two boxes are pushed */
    box_t oldp = r.s.tb_ptr_box, ia, ib;
    item_fb_box(&r, 2, &ia);
    item_fb_box(&r, 4, &ib);
    rig_pen(&r, xb, y, APP_PEN_IN_RANGE, 0);
    r.now += 200;
    n = rig_frame(&r, out);
    CHECK(r.s.tb_hot == 4 && n > 0 && n != SK_FULL);
    for (int i = 0; i < n; i++) CHECK(rect_hits(&out[i], &ia) || rect_hits(&out[i], &ib) || rect_hits(&out[i], &oldp) || rect_hits(&out[i], &r.s.tb_ptr_box));
    long area = 0;
    for (int i = 0; i < n; i++) area += (long)out[i].w * out[i].h;
    CHECK(area < (long)(r.s.tb_box.x1 - r.s.tb_box.x0) * (r.s.tb_box.y1 - r.s.tb_box.y0) / 3); /* not the strip */
    push(panel, &r, out, n);
    CHECK(!memcmp(panel, r.fb, sizeof(panel)));
    /* the pen leaves the strip for the page: the brush cursor is back and the strip is as it was */
    rig_pen(&r, 600, 700, APP_PEN_IN_RANGE, 0);
    r.now += 200;
    n = rig_frame(&r, out);
    CHECK(!r.s.tb_ptr && r.s.tb_hot == -1 && r.s.cursor && n > 0 && n != SK_FULL);
    push(panel, &r, out, n);
    CHECK(!memcmp(panel, r.fb, sizeof(panel)));
    rig_pen(&r, 10, 1000, 0, 0); /* and off the tablet: every mark gone */
    r.now += 200;
    push(panel, &r, out, rig_frame(&r, out));
    CHECK(!r.s.cursor && !memcmp(clean, r.fb, sizeof(clean)) && !memcmp(panel, clean, sizeof(clean)));
    /* leaving straight from the strip, over black ink and white paper, also leaves nothing */
    rig_pen(&r, xa, y, APP_PEN_IN_RANGE, 0);
    rig_pen(&r, 10, 1000, 0, 0);
    r.now += 200;
    push(panel, &r, out, rig_frame(&r, out));
    CHECK(!r.s.tb_ptr && !memcmp(clean, r.fb, sizeof(clean)) && !memcmp(panel, clean, sizeof(clean)));
    /* the arrow at the strip's lower edge hangs over the page: leaving restores that part too */
    rig_pen(&r, xa, r.s.tb.y1 - 6, APP_PEN_IN_RANGE, 0);
    r.now += 200;
    push(panel, &r, out, rig_frame(&r, out));
    CHECK(r.s.tb_ptr && r.s.tb_ptr_box.y1 > r.s.tb.y1 && memcmp(clean, r.fb, sizeof(clean)));
    rig_pen(&r, 10, 1000, 0, 0);
    r.now += 200;
    push(panel, &r, out, rig_frame(&r, out));
    CHECK(!r.s.tb_ptr && !memcmp(clean, r.fb, sizeof(clean)) && !memcmp(panel, clean, sizeof(clean)));
    CHECK(same_page_and_fb(&r) || r.s.tb_on); /* the strip is on top of the page, nothing inked */
    rig_end(&r);
}

/* Each button picks what it shows: a tool, a colour, a size; kept in the settings; the pick is shown inverted */
static void test_each_button_picks(void)
{
    for (int o = 0; o < 4; o += 3) { /* landscape with the bar at the bottom, and portrait */
        static rig_t r;
        rig_new(&r, o);
        key(&r, 3);
        for (int i = 0; i < TI_MIRROR; i++) {
            int x = r.s.tb.cx[i] + r.s.tb.cell / 2, y = r.s.tb.cyi[i] + r.s.tb.cell / 2;
            rig_pen_user(&r, x, y, APP_PEN_IN_RANGE, 0);
            rig_pen_user(&r, x, y, APP_PEN_TIP | APP_PEN_IN_RANGE, 30000);
            rig_pen_user(&r, x, y, APP_PEN_IN_RANGE, 0);
            if (i < TI_BLACK) CHECK(r.s.sty.brush == i && r.s.sty.size == brush_size(i, r.s.size_cls));
            else if (i < TI_SMALL) CHECK(r.s.sty.shade == tb_shade_of(i) && tb_item_of_shade(r.s.sty.shade) == i);
            else CHECK(r.s.size_cls == i - TI_SMALL && r.s.sty.size == brush_size(r.s.sty.brush, i - TI_SMALL));
            CHECK(!r.s.stroke && sk_is_blank(&r.s.page));
            idle(&r, 3000);
        }
        /* the choices are kept over a restart */
        sk_set_brush(&r.s, BR_SPRAY, r.now += 50);
        sk_set_shade(&r.s, SH_GREY, r.now += 50);
        sk_set_size(&r.s, 2, r.now += 50);
        r.keep_tool = 1;
        rig_start(&r, o);
        CHECK(r.s.sty.brush == BR_BRUSH && r.s.sty.shade == SH_GREY && r.s.size_cls == 2 && r.s.sty.size == brush_size(BR_BRUSH, 2)); /* colour and size kept, the Brush picked */
        /* a size button gives each tool its own measure: Small < Medium < Big */
        for (int b = 0; b < BR_N; b++) CHECK(brush_size(b, 0) < brush_size(b, 1) && brush_size(b, 1) < brush_size(b, 2) && brush_size_cls(b, brush_size(b, 1)) == 1);
        /* the strip shows exactly one tool, one colour and one size picked (inverted cells differ from the plain cell) */
        rig_end(&r);
    }
}

int main(void)
{
    test_labels_follow_tools();
    test_each_button_picks();
    test_toggle_and_restore();
    test_flash_only_strip();
    test_taps_never_ink();
    test_draw_near_strip();
    test_layout_all_positions();
    test_brush_cursor_on_screen();
    test_pointer_over_the_strip();
    test_brushes_through_the_app();
    test_tail_and_eraser_toggle();
    return CHECK_DONE("test_toolbar");
}
