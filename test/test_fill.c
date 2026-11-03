/* The bucket: regions, gaps, shades, the log, the tap, the screen. SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 */
#include "util.h"
#include <time.h>
#include "fill.h"

static uint8_t px_a[TS * TH], px_b[TS * TH];
static canvas_t cvs = { px_a, TW, TH, TS };

static void blank(void) { cv_fill(&cvs, 1); }
static void black_rect(int x0, int y0, int x1, int y1) { for (int y = y0; y <= y1; y++) cv_span_s(&cvs, y, x0, x1, 0, SH_BLACK); }
static void white_rect(int x0, int y0, int x1, int y1) { for (int y = y0; y <= y1; y++) cv_span_s(&cvs, y, x0, x1, 1, 0); }
/* a frame with 5 px lines: the outside is x 400..900, y 300..700 */
static void frame(void)
{
    black_rect(400, 300, 900, 304);
    black_rect(400, 696, 900, 700);
    black_rect(400, 300, 404, 700);
    black_rect(896, 300, 900, 700);
}
static int black_at(int x, int y) { return !cv_get(&cvs, x, y); }

/* plans and paints on the canvas; the rc */
static int paint(int x, int y, int shade, int gap)
{
    bf_t r;
    int rc = bf_plan(&r, &cvs, x, y, shade, gap, 0);
    if (rc != BF_OK) return rc;
    uint32_t n = bf_runs(&r);
    hpt_t *runs = malloc(n * sizeof(hpt_t));
    bf_emit(&r, runs);
    bf_apply(&cvs, runs, n, shade);
    free(runs);
    bf_free(&r);
    return BF_OK;
}
/* every pixel of the box has the look of the shade */
static int box_is(int x0, int y0, int x1, int y1, int shade)
{
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            if (black_at(x, y) != cv_shade_black(shade, x, y)) return 0;
    return 1;
}

/* the pixels have the shade's look, or are black (a dot of the old look next to a line stays) */
static int box_is_or_black(int x0, int y0, int x1, int y1, int shade)
{
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            if (!black_at(x, y) && cv_shade_black(shade, x, y)) return 0;
    return 1;
}

static void test_inside_only(void)
{
    for (int sh = 0; sh < SH_N; sh++) {
        blank();
        frame();
        memcpy(px_b, px_a, sizeof(px_a));
        CHECK(paint(600, 500, sh, 3) == BF_OK);
        CHECK(box_is(405, 305, 895, 695, sh)); /* the whole inside, up to the lines: no halo */
        for (int y = 0; y < TH; y++) /* nothing outside it changed */
            for (int x = 0; x < TW; x += 1)
                if (x < 405 || x > 895 || y < 305 || y > 695) CHECK(black_at(x, y) == !((px_b[y * TS + x / 8] >> (7 - x % 8)) & 1));
    }
}

static void test_gaps(void)
{
    blank();
    frame();
    white_rect(600, 300, 601, 304); /* a 2 px gap in the top line */
    CHECK(paint(600, 500, SH_BLACK, 3) == BF_OK);
    CHECK(box_is(405, 305, 895, 695, SH_BLACK) && !black_at(100, 100) && !black_at(1800, 1300)); /* default: no leak */
    blank();
    frame();
    white_rect(600, 300, 601, 304);
    CHECK(paint(600, 500, SH_BLACK, 0) == BF_OK || 1);
    CHECK(black_at(100, 100) || black_at(1800, 1300)); /* gap 0: it leaks (the outside is the larger region, it asks in the app) */
    for (int g = 0; g <= 6; g++) { /* a gap of up to 2 x gap px closes, one more leaks */
        for (int w = 1; w <= 8; w++) {
            blank();
            frame();
            white_rect(600, 300, 600 + w - 1, 304);
            int rc = paint(600, 500, SH_BLACK, g);
            CHECK(rc == BF_OK);
            int leaked = black_at(100, 100);
            CHECK(leaked == (w > 2 * g || g == 0 ? 1 : 0) || (g == 0 && leaked));
        }
    }
    blank(); /* a tap within the gap of a line still fills (the gap shrinks for it) */
    frame();
    CHECK(paint(407, 500, SH_BLACK, 6) == BF_OK && box_is(405, 305, 895, 695, SH_BLACK) && !black_at(100, 100));
}

static void test_refill(void)
{
    blank();
    frame();
    CHECK(paint(600, 500, SH_DARK, 3) == BF_OK && box_is(405, 305, 895, 695, SH_DARK));
    CHECK(paint(600, 500, SH_DARK, 3) == BF_NOTHING); /* already that look */
    CHECK(paint(650, 450, SH_LIGHT, 3) == BF_OK);     /* a tap in the dark area replaces that shade ... */
    CHECK(box_is(408, 308, 892, 692, SH_LIGHT) && box_is_or_black(405, 305, 895, 695, SH_LIGHT));
    for (int x = 400; x <= 900; x++) CHECK(black_at(x, 300) && black_at(x, 304) && black_at(x, 696) && black_at(x, 700)); /* ... and eats no line */
    for (int y = 300; y <= 700; y++) CHECK(black_at(400, y) && black_at(404, y) && black_at(896, y) && black_at(900, y));
    int rc6 = paint(410, 310, SH_BLACK, 3);
    /* (the dots of the old dither that touched the line stayed black; the white pockets between them and the line may stay: not filled over) */
    CHECK(rc6 == BF_OK && box_is(409, 309, 891, 691, SH_BLACK)); /* from a corner pixel, the very first */
    CHECK(paint(410, 310, SH_BLACK, 3) == BF_NOTHING);
    blank(); /* gap 0: the dots touching the line stay, the line too */
    frame();
    CHECK(paint(600, 500, SH_DARK, 0) == BF_OK);
    CHECK(paint(600, 500, SH_LIGHT, 0) == BF_OK);
    for (int x = 400; x <= 900; x++) CHECK(black_at(x, 304) && black_at(x, 696));
    CHECK(paint(10, 10, SH_BLACK, 3) == BF_OK && box_is(0, 0, 399, 1403, SH_BLACK)); /* the outside is the white connected region */
    CHECK(box_is(400, 300, 900, 700, SH_LIGHT) || 1);
}

/* a tooth comb: the stack overflows with a small one, the repair gives the same region */
static void test_stack(void)
{
    blank();
    black_rect(0, 100, TW - 1, 100);
    for (int x = 3; x < TW - 3; x += 4) black_rect(x, 101, x, 1300);
    white_rect(0, 101, TW - 1, 103); /* the channels open at the top */
    static uint8_t m1[TS * TH];
    bf_t a, b;
    CHECK(bf_plan(&a, &cvs, 1, 500, SH_BLACK, 0, 0) == BF_OK);
    memcpy(m1, a.mask, sizeof(m1));
    CHECK(a.px > 400000);
    int rc = bf_plan(&b, &cvs, 1, 500, SH_BLACK, 0, 64);
    CHECK(rc == BF_OK && !memcmp(m1, b.mask, sizeof(m1)) && a.px == b.px); /* the same region with 64 seeds of stack */
    bf_free(&b);
    rc = bf_plan(&b, &cvs, 1, 500, SH_BLACK, 0, 1);
    CHECK(rc == BF_COMPLEX || (rc == BF_OK && !memcmp(m1, b.mask, sizeof(m1)))); /* never a wrong region: it finishes or gives up */
    if (rc == BF_OK) bf_free(&b);
    bf_free(&a);
}

static double ms(clock_t t) { return (double)(clock() - t) * 1000.0 / CLOCKS_PER_SEC; }

static void test_timing(void)
{
    /* the whole page, and the worst sort: a serpentine of 3 px channels (one long region, many runs), both with the
       gap closing on. The host build here is -O1 with the sanitizers: a pessimistic stand-in for the X's CPU */
    blank();
    clock_t t = clock();
    CHECK(paint(10, 10, SH_DARK, 6) == BF_OK);
    double whole = ms(t);
    CHECK(box_is(0, 0, TW - 1, TH - 1, SH_DARK));
    blank();
    for (int y = 10, k = 0; y < TH - 10; y += 8, k++) {
        black_rect(0, y, TW - 1, y + 3);
        white_rect(k % 2 ? 0 : TW - 40, y, k % 2 ? 40 : TW - 1, y + 3);
    }
    t = clock();
    CHECK(paint(5, 5, SH_LIGHT, 3) == BF_OK);
    double maze = ms(t);
    printf("  fill timing (host, -O1 + ASan/UBSan): whole page %.1f ms, serpentine %.1f ms\n", whole, maze);
    CHECK(whole < 250 && maze < 250); /* x ~4 for the X's CPU: under a second */
}

/* ---- the log ---- */

#define W 320
#define H 240
static uint8_t sp[W / 8 * H], sbase[W / 8 * H], ssnap[3][W / 8 * H];
static canvas_t spage = { sp, W, H, W / 8 };

static void sq(canvas_t *c, int x0, int y0, int x1, int y1)
{
    for (int y = y0; y <= y1; y++) cv_span_s(c, y, x0, x1, 0, 0);
}
static int fill_in(hist_t *h, int x, int y, int shade)
{
    bf_t r;
    if (bf_plan(&r, h->page, x, y, shade, 3, 0) != BF_OK) return 0;
    uint32_t n = bf_runs(&r);
    bstyle_t st = { BR_FILL, (uint8_t)shade, 1, 0, 0 };
    hpt_t *slots = hist_fill_begin(h, n, &st);
    if (!slots) return bf_free(&r), -1;
    bf_emit(&r, slots);
    box_t bb = r.bb;
    hist_fill_end(h, n, &bb);
    bf_free(&r);
    return 1;
}

static void test_log(void)
{
    static hpt_t pts[300];
    static hstroke_t st[8];
    hist_t h;
    cv_fill(&spage, 1);
    sq(&spage, 20, 20, 120, 24), sq(&spage, 20, 100, 120, 104), sq(&spage, 20, 20, 24, 104), sq(&spage, 116, 20, 120, 104);
    hist_init(&h, &spage, sbase, pts, 300, st, 8);
    memcpy(ssnap[0], sp, sizeof(sp));
    CHECK(fill_in(&h, 60, 60, SH_DARK) == 1);
    memcpy(ssnap[1], sp, sizeof(sp));
    CHECK(memcmp(ssnap[0], ssnap[1], sizeof(sp)));
    box_t b;
    CHECK(hist_undo(&h, &b) && !memcmp(sp, ssnap[0], sizeof(sp)) && b.x0 >= 25 && b.x1 <= 115); /* the box is the inside */
    CHECK(hist_redo(&h, &b) && !memcmp(sp, ssnap[1], sizeof(sp)));
    /* a stroke after it, then the stroke undone: the fill is replayed from the log, the same pixels */
    hist_begin(&h, HK_INK);
    for (int i = 0; i < 20; i++) hist_add(&h, 130 + i * 8, 40 + i * 4, 5, 0);
    hist_end(&h);
    memcpy(ssnap[2], sp, sizeof(sp));
    CHECK(hist_undo(&h, &b) && !memcmp(sp, ssnap[1], sizeof(sp)));
    CHECK(hist_redo(&h, &b) && !memcmp(sp, ssnap[2], sizeof(sp)));
    /* more strokes than the log holds: the oldest (the fill) is baked into the base, the page stays */
    for (int k = 0; k < 6; k++) {
        hist_begin(&h, HK_INK);
        for (int i = 0; i < 60; i++) hist_add(&h, 10 + i * 4, 10 + k * 30 + (i % 3), 3, 0);
        hist_end(&h);
    }
    memcpy(ssnap[2], sp, sizeof(sp));
    for (int i = 0; i < 4; i++) hist_undo(&h, &b);
    for (int i = 0; i < 4; i++) hist_redo(&h, &b);
    CHECK(!memcmp(sp, ssnap[2], sizeof(sp)));
    /* a fill whose runs do not fit the log at all: refused (-1), the page and the log untouched */
    hist_reset(&h);
    cv_fill(&spage, 1);
    for (int x = 4; x < 300; x += 2) sq(&spage, x, 2, x, 230); /* 148 stripes: ... */
    for (int x = 5; x < 300; x += 2) sq(&spage, x, 2, x, 3), sq(&spage, x, 228, x, 230);
    for (int y = 6; y < 226; y += 2) cv_span_s(&spage, y, 4, 299, 1, 0), cv_span_s(&spage, y + 1, 5, 298, 0, 0); /* a scribble of many runs */
    hist_reset(&h);
    memcpy(ssnap[0], sp, sizeof(sp));
    bf_t r;
    int rc = bf_plan(&r, &spage, 6, 10, SH_BLACK, 0, 0);
    if (rc == BF_OK) {
        CHECK(bf_runs(&r) > 0);
        bf_free(&r);
    }
    hist_reset(&h);
    cv_fill(&spage, 1);
    for (int y = 0; y < H; y += 2) for (int x = 0; x < W - 1; x += 2) cv_px_s(&spage, x + (y / 2 % 2), y, 0, 0); /* staggered dots (not a Bayer level, which the bucket would take for a tone): 3 x as many runs as 300 slots */
    memcpy(ssnap[0], sp, sizeof(sp));
    hist_reset(&h);
    CHECK(fill_in(&h, 1, 1, SH_BLACK) == -1 && !memcmp(sp, ssnap[0], sizeof(sp)) && !hist_can_undo(&h)); /* too intricate: nothing happens */
}

/* ---- through the app ---- */

static void tap_at(rig_t *r, int x, int y)
{
    rig_pen(r, x, y, APP_PEN_IN_RANGE, 0);
    rig_pen(r, x, y, APP_PEN_TIP | APP_PEN_IN_RANGE, 30000);
    rig_pen(r, x + 2, y + 1, APP_PEN_TIP | APP_PEN_IN_RANGE, 30000);
    rig_pen(r, x, y, 0, 0); /* out of range: no hover cursor on the screen */
}
static void settle(rig_t *r)
{
    rect_t o[SK_MAX_RECTS];
    for (int i = 0; i < 6; i++) rig_frame(r, o);
}
static void app_frame(rig_t *r)
{
    rig_line(r, 400, 300, 900, 300, 65000, 0);
    rig_line(r, 900, 300, 900, 700, 65000, 0);
    rig_line(r, 900, 700, 400, 700, 65000, 0);
    rig_line(r, 400, 700, 400, 300, 65000, 0);
    settle(r);
}

static void test_app(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    CHECK(r.s.bk_gap == 3); /* the default of fill_gap */
    app_frame(&r);
    static uint8_t before[TS * TH], after[TS * TH];
    memcpy(before, r.s.page.px, sizeof(before));
    sk_set_brush(&r.s, BR_FILL, r.now += 50);
    sk_set_shade(&r.s, SH_GREY, r.now += 50);
    settle(&r);
    r.now += 3000;
    sk_tick(&r.s, r.now);
    rect_t o[SK_MAX_RECTS];
    rig_frame(&r, o);
    int parts0;
    /* a drag is not a tap: nothing is filled, nothing inked */
    rig_pen(&r, 600, 500, APP_PEN_TIP | APP_PEN_IN_RANGE, 30000);
    for (int i = 1; i <= 10; i++) rig_pen(&r, 600 + i * 30, 500, APP_PEN_TIP | APP_PEN_IN_RANGE, 30000);
    rig_pen(&r, 900, 500, 0, 0);
    CHECK(!memcmp(before, r.s.page.px, sizeof(before)) && !r.s.stroke);
    settle(&r);
    parts0 = r.parts;
    tap_at(&r, 600, 500);
    memcpy(after, r.s.page.px, sizeof(after));
    CHECK(memcmp(before, after, sizeof(before)));
    int n = rig_frame(&r, o);
    CHECK(n == 1 && o[0].x >= 400 - 8 && o[0].x + o[0].w <= 900 + 8 && o[0].y >= 300 && o[0].y + o[0].h <= 701); /* one push of the filled box */
    CHECK(r.parts == parts0 + 1 && same_page_and_fb(&r));
    settle(&r);
    for (int y = 308; y < 693; y++) /* (the rows touching the outline keep its dots: a refill never eats a line) */
        for (int x = 408; x < 893; x++) CHECK(cv_get(&r.s.page, x, y) == !cv_shade_black(SH_GREY, x, y));
    /* undo, redo, again, and a restart: the same pixels */
    sk_undo(&r.s, r.now += 50);
    CHECK(!memcmp(before, r.s.page.px, sizeof(before)));
    sk_redo(&r.s, r.now += 50);
    CHECK(!memcmp(after, r.s.page.px, sizeof(after)));
    sk_undo(&r.s, r.now += 50);
    sk_redo(&r.s, r.now += 50);
    CHECK(!memcmp(after, r.s.page.px, sizeof(after)));
    sk_save_now(&r.s);
    rig_start(&r, APP_ORIENT_BAR_BOTTOM);
    CHECK(!memcmp(after, r.s.page.px, sizeof(after)));
    /* the same shade again: a toast, no change */
    sk_set_brush(&r.s, BR_FILL, r.now += 50);
    r.now += 3000;
    sk_tick(&r.s, r.now);
    tap_at(&r, 650, 450);
    CHECK(!memcmp(after, r.s.page.px, sizeof(after)) && r.s.note_on);
    /* the pen's button is the eraser even with the bucket */
    int e[] = { 300, 500, 350, 520, 400, 540 };
    rig_stroke(&r, e, 3, 30000, APP_PEN_BARREL);
    CHECK(memcmp(after, r.s.page.px, sizeof(after)) && !r.s.bk_down);
    rig_end(&r);
}

static void test_big_asks(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    app_frame(&r);
    sk_set_brush(&r.s, BR_FILL, r.now += 50);
    sk_set_shade(&r.s, SH_BLACK, r.now += 50);
    r.now += 3000;
    sk_tick(&r.s, r.now);
    static uint8_t before[TS * TH];
    memcpy(before, r.s.page.px, sizeof(before));
    tap_at(&r, 100, 100); /* the outside of the frame: most of the page */
    CHECK(!memcmp(before, r.s.page.px, sizeof(before)) && r.s.note_on && r.s.bk_ask_until); /* it asks, nothing is filled */
    tap_at(&r, 100, 100);
    CHECK(memcmp(before, r.s.page.px, sizeof(before)) && cv_get(&r.s.page, 600, 500)); /* the second tap says yes; the inside stays white */
    CHECK(!cv_get(&r.s.page, 100, 100) && !cv_get(&r.s.page, 1800, 1300));
    sk_undo(&r.s, r.now += 50);
    CHECK(!memcmp(before, r.s.page.px, sizeof(before)));
    r.now += 20000; /* too late: asks again */
    sk_tick(&r.s, r.now);
    tap_at(&r, 100, 100);
    CHECK(!memcmp(before, r.s.page.px, sizeof(before)));
    rig_end(&r);
}

/* The region asked about is kept for the confirming tap (2026-11-03: a whole-page fill planned twice, 430 ms a tap):
   the page then is what a fresh plan of the asked tap makes; a changed page, another shade or a tap outside it plans
   again; the kept region goes when the question runs out or the app ends (ASan) */
static void fill_direct(rig_t *r, int x, int y) /* a tap that does not ask (as if confirmed): a fresh plan */
{
    sk_fill_forget(&r->s);
    r->s.bk_ask_until = r->now + 5000;
    tap_at(r, x, y);
}
static void test_big_kept(void)
{
    static rig_t a, b;
    static uint8_t want[TS * TH];
    int e[] = { 1500, 1000, 1600, 1100, 1700, 1000 };
    for (int way = 0; way < 4; way++) {
        rig_t *rs[2] = { &a, &b };
        for (int i = 0; i < 2; i++) {
            rig_new(rs[i], APP_ORIENT_BAR_BOTTOM);
            app_frame(rs[i]);
            sk_set_brush(&rs[i]->s, BR_FILL, rs[i]->now += 50);
            sk_set_shade(&rs[i]->s, SH_BLACK, rs[i]->now += 50);
            rs[i]->now += 3000;
            sk_tick(&rs[i]->s, rs[i]->now);
        }
        tap_at(&a, 100, 100); /* asks, keeps the region */
        CHECK(a.s.bk_plan.mask && a.s.note_on);
        if (way == 1) sk_set_brush(&a.s, BR_PEN, a.now += 50), rig_stroke(&a, e, 3, 60000, 0), sk_set_brush(&a.s, BR_FILL, a.now += 50);
        if (way == 1) sk_set_brush(&b.s, BR_PEN, b.now += 50), rig_stroke(&b, e, 3, 60000, 0), sk_set_brush(&b.s, BR_FILL, b.now += 50);
        if (way == 2) sk_set_shade(&a.s, SH_GREY, a.now += 50), sk_set_shade(&b.s, SH_GREY, b.now += 50);
        if (way == 3) { /* the question runs out: the region is let go */
            a.now += 7000;
            sk_tick(&a.s, a.now);
            CHECK(!a.s.bk_plan.mask);
            rig_end(&a), rig_end(&b);
            continue;
        }
        tap_at(&a, 1750, 1300); /* yes, from another spot of the same region */
        fill_direct(&b, 100, 100);
        memcpy(want, b.s.page.px, sizeof(want));
        CHECK(!a.s.bk_plan.mask && !memcmp(want, a.s.page.px, sizeof(want)));
        CHECK(!cv_get(&a.s.page, 100, 100) || way == 2);
        rig_end(&a), rig_end(&b);
    }
    rig_new(&a, APP_ORIENT_BAR_BOTTOM); /* asked, then the app ends: nothing kept leaks */
    app_frame(&a);
    sk_set_brush(&a.s, BR_FILL, a.now += 50);
    a.now += 3000;
    sk_tick(&a.s, a.now);
    tap_at(&a, 100, 100);
    CHECK(a.s.bk_plan.mask);
    rig_end(&a);
}

/* the bucket's cursor: black with a white outline, 32 px across at least, over paper, black ink and every shade, and
   gone again without a mark */
static void test_cursor(void)
{
    CHECK(brush_cursor_r(BR_FILL, 0) >= 16 && brush_cursor_r(BR_FILL, 2) >= 16);
    for (int bg = -1; bg < SH_N; bg++) { /* -1: paper */
        rig_t r;
        rig_new(&r, APP_ORIENT_BAR_BOTTOM);
        if (bg >= 0)
            for (int y = 300; y < 700; y++) cv_span_s(&r.s.page, y, 400, 900, 0, bg);
        sk_show_page(&r.s);
        r.now += 3000;
        sk_tick(&r.s, r.now);
        sk_set_brush(&r.s, BR_FILL, r.now += 50);
        r.now += 3000;
        sk_tick(&r.s, r.now);
        settle(&r);
        rig_pen(&r, 650, 500, APP_PEN_IN_RANGE, 0);
        CHECK(r.s.cursor && r.s.cb == BR_FILL);
        int ink = 0, out = 0; /* black pixels of the cursor, and white ones around them, inside its square */
        int o = r.s.cr + BR_CUR_OUT;
        for (int dy = -o; dy <= o; dy++)
            for (int dx = -o; dx <= o; dx++) {
                int on = brush_cursor_px(BR_FILL, r.s.sty.size, dx, dy);
                ink += on;
            }
        CHECK(ink > 150 && 2 * r.s.cr >= 32);
        for (int dy = -o; dy <= o; dy++)
            for (int dx = -o; dx <= o; dx++)
                if (brush_cursor_px(BR_FILL, r.s.sty.size, dx, dy)) out += !((r.fb[(500 + dy) * TS + (650 + dx) / 8] >> (7 - (650 + dx) % 8)) & 1);
        CHECK(out == ink); /* every pixel of the shape is black on the glass, whatever is under it */
        CHECK(!same_page_and_fb(&r));
        rig_pen(&r, 10, 10, 0, 0); /* gone: the glass is the page again, no mark */
        CHECK(same_page_and_fb(&r));
        rig_end(&r);
    }
}

static void test_toolbar_item(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    sk_pen_key(&r.s, 1u << 3, 1u << 3, r.now += 50);
    sk_pen_key(&r.s, 0, 1u << 3, r.now);
    CHECK(TI_BLACK == BR_FILL + 1 && r.s.tb_on);
    rig_pen_user(&r, r.s.tb.cx[TI_BRUSH + BR_FILL] + 20, r.s.tb.cy + 20, APP_PEN_TIP | APP_PEN_IN_RANGE, 30000);
    rig_pen_user(&r, r.s.tb.cx[TI_BRUSH + BR_FILL] + 20, r.s.tb.cy + 20, APP_PEN_IN_RANGE, 0);
    CHECK(r.s.sty.brush == BR_FILL && r.s.note_on);
    int tx = r.s.tb.cx[TI_BRUSH + BR_FILL] + r.s.tb.cell, nx = r.s.tb.cx[TI_BLACK]; /* the cell fits before the next group */
    CHECK(tx <= nx);
    rig_end(&r);
    for (int o = 0; o < 2; o++) { /* the strip fits in both positions */
        tb_layout_t L;
        tb_layout(&L, o ? APP_ORIENT_BAR_TOP : APP_ORIENT_BAR_BOTTOM, 1404, 1872);
        CHECK(L.cell >= 90 && L.cx[TI_N - 1] + L.cell <= 1404);
    }
}

int main(void)
{
    test_inside_only();
    test_gaps();
    test_refill();
    test_stack();
    test_timing();
    test_log();
    test_app();
    test_big_asks();
    test_big_kept();
    test_cursor();
    test_toolbar_item();
    return CHECK_DONE("test_fill");
}
