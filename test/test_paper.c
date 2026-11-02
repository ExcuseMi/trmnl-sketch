/* The paper under the ink: Blank, Grid, Lines, Dots. Not part of the drawing: a per-page sidecar, drawn on the screen under the
   ink, never erased, never a wall for the bucket, in the thumbnail and the printed copy. SPDX-License-Identifier: MIT */
#include "check.h"
#include <stdio.h>
#include "gallery.h"
#include "pngio.h"
#include "util.h"

#define DOWN (APP_PEN_TIP | APP_PEN_IN_RANGE)
static void btn(rig_t *r, int n)
{
    sk_pen_key(&r->s, 1u << (n - 1), 1u << (n - 1), r->now += 50);
    sk_pen_key(&r->s, 0, 1u << (n - 1), r->now);
}
static void tap(rig_t *r, int zone) { sk_touch(&r->s, zone, APP_GESTURE_TAP, r->now += 50); }
static int px_black(const canvas_t *c, int x, int y) { return !cv_get(c, x, y); }
static int count_black(const canvas_t *c)
{
    int n = 0;
    for (int y = 0; y < c->h; y++)
        for (int x = 0; x < c->w; x++) n += px_black(c, x, y);
    return n;
}
static int read_all(rig_t *r, const char *path, uint8_t *buf, int cap)
{
    void *f = r->fs.fs.open(&r->fs, path, 0);
    if (!f) return -1;
    int n = r->fs.fs.read(&r->fs, f, buf, cap);
    r->fs.fs.close(&r->fs, f);
    return n;
}
static void settle(rig_t *r) { rig_pen(r, 10, 600, 0, 0); r->now += 3000; sk_tick(&r->s, r->now); }

static void test_patterns(void)
{
    for (int k = PP_BLANK; k < PP_N; k++)
        for (int sp = PP_SP_MIN; sp <= PP_SP_MAX; sp += 37) {
            int n = 0;
            for (int y = 0; y < 400; y++)
                for (int x = 0; x < 400; x++) n += pp_black(k, sp, x, y);
            CHECK(k == PP_BLANK ? n == 0 : n > 0 && n < 400 * 400 / 8); /* light: under 12 % of the pixels */
        }
    CHECK(pp_spacing(0) == PP_SP_DEF && pp_spacing(3) == PP_SP_MIN && pp_spacing(9999) == PP_SP_MAX && pp_spacing(60) == 60);
    CHECK(pp_from_name("grid", 4) == PP_GRID && pp_from_name("Dots", 4) == PP_DOTS && pp_from_name("lines", 5) == PP_LINES && pp_from_name("x", 1) == PP_BLANK && pp_from_name("", 0) == PP_BLANK);
}

static void test_screen_and_eraser_and_fill(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    sk_set_paper(&r.s, PP_GRID, r.now);
    r.s.paper_sp = 48;
    sk_show_page(&r.s);
    CHECK(count_black(&r.s.page) == 0); /* the paper is not in the page */
    int on_screen = fb_black(&r);
    CHECK(on_screen > 1000);
    CHECK(px_black(&(canvas_t){ r.fb, TW, TH, TS }, 3 * 16, 48 * 5 - 1) == pp_black(PP_GRID, 48, 3 * 16, 48 * 5 - 1));
    /* ink over the paper, then the eraser over both: the paper comes back, the page is white */
    rig_line(&r, 300, 500, 900, 520, 50000, 0);
    settle(&r);
    CHECK(count_black(&r.s.page) > 100);
    sk_set_brush(&r.s, BR_ERASER, r.now);
    sk_set_size(&r.s, 40, r.now);
    for (int y = 480; y <= 540; y += 20) rig_line(&r, 280, y, 920, y, 50000, 0);
    settle(&r);
    CHECK(count_black(&r.s.page) == 0);
    canvas_t fbc = { r.fb, TW, TH, TS };
    int back = 0, want = 0;
    for (int y = 480; y < 540; y++)
        for (int x = 300; x < 900; x++) want += pp_black(PP_GRID, 48, x, y), back += px_black(&fbc, x, y);
    CHECK(want > 0 && back == want); /* the eraser did not remove the paper */
    rig_end(&r);
    /* the bucket: a closed box on grid paper fills edge to edge, the grid lines are no walls */
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    sk_set_paper(&r.s, PP_GRID, r.now);
    r.s.paper_sp = 20; /* a grid line every 20 px inside the box */
    sk_set_size(&r.s, 5, r.now);
    int rc[10] = { 400, 400, 1000, 400, 1000, 800, 400, 800, 400, 400 };
    rig_stroke(&r, rc, 5, 50000, 0);
    settle(&r);
    sk_set_brush(&r.s, BR_FILL, r.now);
    sk_show_page(&r.s);
    rig_pen(&r, 700, 600, APP_PEN_IN_RANGE, 0);
    rig_pen(&r, 700, 600, DOWN, 30000);
    rig_pen(&r, 700, 600, APP_PEN_IN_RANGE, 0);
    settle(&r);
    int holes = 0;
    for (int y = 410; y < 790; y++)
        for (int x = 410; x < 990; x++) holes += !px_black(&r.s.page, x, y);
    CHECK(holes == 0); /* the whole inside is filled: the paper did not stop the fill */
    rig_end(&r);
}

static void test_sidecar_and_old_pages(void)
{
    static uint8_t buf[SK_FILE_MAX], plain[SK_FILE_MAX];
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    sk_set_paper(&r.s, PP_LINES, r.now);
    r.s.paper_sp = 40;
    rig_line(&r, 300, 500, 900, 520, 50000, 0);
    settle(&r);
    sk_save_now(&r.s);
    CHECK(read_all(&r, "sketch/sk0001.pap", buf, 15) > 0);
    int n = read_all(&r, "sketch/sk0001.png", plain, SK_FILE_MAX), m = read_all(&r, "sketch/sk0001-paper.png", buf, SK_FILE_MAX);
    CHECK(n > 0 && m > 0 && n != m);
    static uint8_t px[TS * TH];
    CHECK(png_decode(buf, (size_t)m, px, TW, TH, TS) == 0);
    canvas_t c = { px, TW, TH, TS };
    CHECK(px_black(&c, 6, 40 * 3 - 1) && !px_black(&c, 6 + 1, 40 * 3 - 1) && px_black(&c, 3, 40 * 3 - 1)); /* a printed line pixel */
    CHECK(png_decode(plain, (size_t)n, px, TW, TH, TS) == 0);
    CHECK(!px_black(&c, 3, 40 * 3 - 1)); /* the page file itself has no paper */
    /* a restart keeps it */
    rig_start(&r, APP_ORIENT_BAR_BOTTOM);
    CHECK(r.s.paper == PP_LINES && r.s.paper_sp == 40);
    /* an old page (no sidecar) is Blank, and so is a damaged sidecar */
    memfs_put(&r.fs, "sketch/sk0001.pap", "garbage!", 8);
    rig_start(&r, APP_ORIENT_BAR_BOTTOM);
    CHECK(r.s.paper == PP_BLANK);
    memfile_t *f = memfs_find(&r.fs, "sketch/sk0001.pap");
    if (f) f->used = 0;
    rig_start(&r, APP_ORIENT_BAR_BOTTOM);
    CHECK(r.s.paper == PP_BLANK);
    /* going back to Blank removes the sidecar and the printed copy */
    sk_set_paper(&r.s, PP_DOTS, r.now);
    rig_line(&r, 300, 700, 900, 720, 50000, 0);
    settle(&r);
    sk_save_now(&r.s);
    CHECK(read_all(&r, "sketch/sk0001-paper.png", buf, 100) > 0);
    sk_set_paper(&r.s, PP_BLANK, r.now);
    sk_save_now(&r.s);
    CHECK(read_all(&r, "sketch/sk0001-paper.png", buf, 100) < 0 && read_all(&r, "sketch/sk0001.pap", buf, 15) < 0);
    rig_end(&r);
}

static void test_default_and_menu(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    rig_cfg(&r, "paper", "grid");
    rig_cfg(&r, "paper_sp", "32");
    rig_start(&r, APP_ORIENT_BAR_BOTTOM);
    CHECK(r.s.paper == PP_GRID && r.s.paper_sp == 32); /* a new page takes the settings' default */
    rig_line(&r, 300, 500, 900, 520, 50000, 0);
    settle(&r);
    sk_new_page(&r.s, r.now);
    CHECK(r.s.paper == PP_GRID && r.s.cur == 2);
    /* the page menu: Paper cycles in place and only this page changes */
    btn(&r, 7);
    CHECK(r.s.mode == SK_PAGEMENU && r.s.list_n == 8);
    for (int i = 0; i < 3; i++) btn(&r, 2);
    btn(&r, 4);
    CHECK(r.s.mode == SK_PAGEMENU && r.s.paper == PP_LINES && r.s.act_sel == 4); /* the Paper row (Back on top) */
    btn(&r, 4);
    btn(&r, 4);
    CHECK(r.s.paper == PP_BLANK);
    btn(&r, 4);
    CHECK(r.s.paper == PP_GRID);
    btn(&r, 4);
    CHECK(r.s.paper == PP_LINES);
    rig_end(&r);
    (void)tap;
}

static void test_thumbnail_and_print(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    rig_line(&r, 300, 500, 900, 520, 50000, 0);
    settle(&r);
    gal_cache_t cache = { 0 };
    const uint8_t *t0 = gal_make(&cache, 1, &r.s.page);
    int plain = 0, with = 0;
    for (int y = 0; y < GAL_TH; y++)
        for (int x = 0; x < GAL_TW; x++) plain += !gal_bit(t0, x, y);
    gal_cache_free(&cache);
    sk_set_paper(&r.s, PP_GRID, r.now);
    r.s.paper_sp = 48;
    sk_save_now(&r.s);
    btn(&r, 7); /* the menu: thumbnails are made with the paper */
    const uint8_t *t1 = gal_find(&r.s.cache, 1);
    if (!t1) {
        btn(&r, 5);
        btn(&r, 7);
        t1 = gal_find(&r.s.cache, 1);
    }
    CHECK(t1 != 0);
    if (t1)
        for (int y = 0; y < GAL_TH; y++)
            for (int x = 0; x < GAL_TW; x++) with += !gal_bit(t1, x, y);
    CHECK(with > plain && with < plain * 3 + 4000); /* there, and light */
    /* Print sends the copy with the paper */
    CHECK(sk_print_check(&r.s) == 0);
    int rc = sk_print_page(&r.s, 1);
    CHECK(rc == 0 || r.pr.posts == 1);
    size_t with_len = r.pr.len[0];
    rig_end(&r);
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    rig_line(&r, 300, 500, 900, 520, 50000, 0);
    settle(&r);
    sk_save_now(&r.s);
    CHECK(sk_print_check(&r.s) == 0);
    sk_print_page(&r.s, 1);
    CHECK(r.pr.posts == 1 && r.pr.len[0] != with_len); /* the plain page prints differently */
    rig_end(&r);
}

int main(void)
{
    test_patterns();
    test_screen_and_eraser_and_fill();
    test_sidecar_and_old_pages();
    test_default_and_menu();
    test_thumbnail_and_print();
    return CHECK_DONE("test_paper");
}
