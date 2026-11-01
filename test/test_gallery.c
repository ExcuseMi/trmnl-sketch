/* The gallery, the action list, thumbnails, full refreshes, printing and the pen in the menus. SPDX-License-Identifier: MIT */
#include "check.h"
#include <stdio.h>
#include "util.h"

static void touch(rig_t *r, int zone, int gesture) { sk_touch(&r->s, zone, gesture, r->now += 50); }
static void tap(rig_t *r, int zone) { touch(r, zone, APP_GESTURE_TAP); }

/* n pages, each with a different line; the last one is on screen */
static void make_pages(rig_t *r, int n)
{
    for (int i = 0; i < n; i++) {
        rig_line(r, 100 + i * 40, 200, 900 + i * 30, 300 + i * 20 % 700, 50000, 0);
        r->now += 3000;
        sk_tick(&r->s, r->now);
        if (i + 1 < n) sk_pen_key(&r->s, 1u << 5, 1u << 5, r->now += 50), sk_pen_key(&r->s, 0, 1u << 5, r->now);
    }
}

static int overlap(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh)
{
    return ax < bx + bw && bx < ax + aw && ay < by + bh && by < ay + ah;
}

static void test_layout_four_positions(void)
{
    for (int o = 0; o < 4; o++) {
        int sw = o % 2 ? TH : TW, sh = o % 2 ? TW : TH;
        gal_layout_t L;
        gal_layout(&L, o, sw, sh);
        CHECK(L.per >= 6 && L.per == L.cols * L.rows);
        for (int i = 0; i < L.per; i++) {
            int x, y;
            gal_tile_xy(&L, i, &x, &y);
            CHECK(x >= L.ax0 && x + L.tw <= L.ax1 && y >= L.ay0 && y + L.th + GAL_LABEL <= L.ay1); /* inside the free area */
            CHECK(x >= 0 && y >= 0 && x + L.tw <= sw && y + L.th + GAL_LABEL <= sh);
            for (int j = i + 1; j < L.per; j++) {
                int x2, y2;
                gal_tile_xy(&L, j, &x2, &y2);
                CHECK(!overlap(x, y, L.tw, L.th + GAL_LABEL, x2, y2, L.tw, L.th + GAL_LABEL));
            }
            CHECK(gal_hit(&L, x + L.tw / 2, y + L.th / 2) == i);
        }
        /* the touch bar's strip (160 deep along its edge) stays free */
        int d = APP_TOUCHBAR_DEPTH, bx = o == APP_ORIENT_BAR_LEFT ? 0 : o == APP_ORIENT_BAR_RIGHT ? sw - d : 0,
            by = o == APP_ORIENT_BAR_BOTTOM ? sh - d : 0, bw = o % 2 ? d : sw, bh = o % 2 ? sh : d;
        for (int i = 0; i < L.per; i++) {
            int x, y;
            gal_tile_xy(&L, i, &x, &y);
            CHECK(!overlap(x, y, L.tw, L.th + GAL_LABEL, bx, by, bw, bh));
        }
        /* a framebuffer point and back: the centre of the screen maps to the centre */
        int ux, uy;
        gal_from_fb(o, TW, TH, TW / 2, TH / 2, &ux, &uy);
        CHECK(abs(ux - sw / 2) <= 1 && abs(uy - sh / 2) <= 1);
    }
}

static void test_selection_and_paging(void)
{
    static rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    make_pages(&r, 20);
    tap(&r, APP_TOUCH_MIDDLE);
    CHECK(r.s.mode == SK_GALLERY && r.s.sel == 1 && r.s.pg.n == 20); /* the page on screen is the newest: tile 1 */
    tap(&r, APP_TOUCH_LEFT);
    CHECK(r.s.sel == 0);
    tap(&r, APP_TOUCH_LEFT);
    CHECK(r.s.sel == 0); /* not before the new-page tile */
    for (int i = 0; i < 14; i++) tap(&r, APP_TOUCH_RIGHT);
    CHECK(r.s.sel == 14 && r.s.sel / r.s.L.per == 0);
    tap(&r, APP_TOUCH_RIGHT);
    CHECK(r.s.sel == 15 && r.s.sel / r.s.L.per == 1); /* the next screen of tiles */
    touch(&r, APP_TOUCH_MIDDLE, APP_GESTURE_SWIPE_RIGHT);
    CHECK(r.s.sel == 0); /* a swipe moves a whole screen */
    touch(&r, APP_TOUCH_MIDDLE, APP_GESTURE_SWIPE_LEFT);
    touch(&r, APP_TOUCH_MIDDLE, APP_GESTURE_SWIPE_LEFT);
    CHECK(r.s.sel == 15); /* no screen past the last one */
    for (int i = 0; i < 5; i++) tap(&r, APP_TOUCH_RIGHT);
    CHECK(r.s.sel == 20);
    rig_end(&r);
}

static int thumb_sum(const uint8_t *t)
{
    int sum = 0;
    for (int y = 0; y < GAL_TH; y++)
        for (int x = 0; x < GAL_TW; x++) sum += gal_bit(t, x, y);
    return sum;
}

static void test_thumbnails_cache_and_edit(void)
{
    static rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    make_pages(&r, 3);
    tap(&r, APP_TOUCH_MIDDLE);
    int made = r.s.cache.made;
    CHECK(made >= 3);
    const uint8_t *t = gal_find(&r.s.cache, r.s.cur);
    CHECK(t != 0);
    int before = thumb_sum(t);
    CHECK(before < GAL_TW * GAL_TH); /* ink makes it darker than white */
    /* back and forth without an edit: nothing is made again */
    tap(&r, APP_TOUCH_MIDDLE); /* actions */
    tap(&r, APP_TOUCH_MIDDLE); /* open */
    CHECK(r.s.mode == SK_DRAW);
    tap(&r, APP_TOUCH_MIDDLE);
    CHECK(r.s.cache.made == made + 3 || r.s.cache.made == made); /* the cache was freed on leaving: made again, but not twice */
    /* an edit changes the page's thumbnail */
    tap(&r, APP_TOUCH_MIDDLE);
    tap(&r, APP_TOUCH_MIDDLE);
    CHECK(r.s.mode == SK_DRAW);
    rig_line(&r, 300, 900, 1500, 1000, 60000, 0);
    r.now += 3000;
    sk_tick(&r.s, r.now);
    tap(&r, APP_TOUCH_MIDDLE);
    const uint8_t *t2 = gal_find(&r.s.cache, r.s.cur);
    CHECK(t2 && thumb_sum(t2) < before);
    /* a save drops that page's entry (the page changed under a cache that is still up) */
    CHECK(gal_find(&r.s.cache, r.s.cur) != 0);
    r.s.dirty = 1;
    sk_save_now(&r.s);
    CHECK(gal_find(&r.s.cache, r.s.cur) == 0);
    rig_end(&r);
}

/* The panel as the glue keeps it: a full refresh takes the whole framebuffer, a partial one only its rectangles */
static void panel_push(uint8_t *panel, const rig_t *r, const rect_t *out, int n)
{
    if (n == SK_FULL) return (void)memcpy(panel, r->fb, (size_t)TS * TH);
    for (int i = 0; i < n; i++)
        for (int y = out[i].y; y < out[i].y + out[i].h; y++) memcpy(panel + y * TS + out[i].x / 8, r->fb + y * TS + out[i].x / 8, (size_t)out[i].w / 8);
}

static void test_full_refresh_only_when_a_screen_opens_or_closes(void)
{
    static rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    make_pages(&r, 3);
    rect_t out[SK_MAX_RECTS];
    rig_frame(&r, out);
    int f = r.fulls;
    tap(&r, APP_TOUCH_MIDDLE); /* open the gallery */
    CHECK(rig_frame(&r, out) == SK_FULL && r.fulls == f + 1);
    r.now += 1000;
    tap(&r, APP_TOUCH_RIGHT); /* move: only the old and the new highlight */
    int n = rig_frame(&r, out);
    CHECK(n > 0 && r.fulls == f + 1);
    tap(&r, APP_TOUCH_MIDDLE); /* a drawing tile: its action list */
    CHECK(rig_frame(&r, out) == SK_FULL && r.fulls == f + 2);
    r.now += 1000;
    tap(&r, APP_TOUCH_RIGHT); /* a row of the list */
    CHECK(rig_frame(&r, out) > 0 && r.fulls == f + 2);
    tap(&r, APP_TOUCH_LEFT);
    tap(&r, APP_TOUCH_MIDDLE); /* Open: back to drawing */
    CHECK(rig_frame(&r, out) == SK_FULL && r.fulls == f + 3 && r.s.mode == SK_DRAW);
    sk_pen_key(&r.s, 1u << 6, 1u << 6, r.now += 50); /* previous page */
    sk_pen_key(&r.s, 0, 1u << 6, r.now);
    CHECK(rig_frame(&r, out) == SK_FULL && r.fulls == f + 4); /* a page change flashes */
    sk_tick(&r.s, r.now += 3000); /* the page's toast is gone and its clean-up flash done: ink is the thing left */
    while (rig_frame(&r, out) != 0) {}
    int f2 = r.fulls;
    int parts = r.parts;
    rig_line(&r, 200, 600, 900, 700, 40000, 0);
    n = rig_frame(&r, out);
    CHECK(n > 0 && r.fulls == f2 && r.parts > parts); /* ink stays a partial refresh */
    rig_end(&r);
}

/* black pixels of the framebuffer inside the user-frame rectangle of tile `slot` */
static int tile_black(const rig_t *r, int slot, int *whites)
{
    int x, y, black = 0;
    gal_tile_xy(&r->s.L, slot, &x, &y);
    gfx_t g;
    gfx_init(&g, r->fb, TW, TH, TS, r->s.orient);
    *whites = 0;
    for (int dy = 0; dy < r->s.L.th; dy++)
        for (int dx = 0; dx < r->s.L.tw; dx++) gfx_get(&g, x + dx, y + dy) ? ++*whites : ++black;
    return black;
}

static void test_screen_has_thumbnails(void)
{
    static rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    make_pages(&r, 4);
    tap(&r, APP_TOUCH_MIDDLE);
    rect_t out[SK_MAX_RECTS];
    CHECK(rig_frame(&r, out) == SK_FULL);
    int w, b = tile_black(&r, 1, &w);
    CHECK(b > 50 && w > 5000); /* the newest page: its line in black dots on paper */
    rig_end(&r);
}

static int read_all(rig_t *r, const char *path, uint8_t *buf, int cap)
{
    void *f = r->fs.fs.open(&r->fs, path, 0);
    if (!f) return -1;
    int n = r->fs.fs.read(&r->fs, f, buf, cap);
    r->fs.fs.close(&r->fs, f);
    return n;
}

static void test_print_from_the_action_list(void)
{
    static rig_t r;
    static uint8_t file[SK_FILE_MAX];
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    make_pages(&r, 2);
    r.pr.ipp = 0;
    strcpy(r.cfg_printer, "10.1.2.3");
    tap(&r, APP_TOUCH_MIDDLE);
    tap(&r, APP_TOUCH_MIDDLE);
    tap(&r, APP_TOUCH_RIGHT); /* Print */
    tap(&r, APP_TOUCH_MIDDLE);
    CHECK(r.s.mode == SK_CONFIRM && r.pr.posts == 0); /* nothing is sent before the confirm */
    tap(&r, APP_TOUCH_MIDDLE); /* Print */
    CHECK(r.s.mode == SK_BUSY && r.s.print_req);
    rect_t out[SK_MAX_RECTS];
    CHECK(rig_frame(&r, out) == SK_FULL); /* "Printing..." is on the glass before the job goes out */
    sk_run_print(&r.s);
    CHECK(r.s.mode == SK_RESULT && r.pr.posts >= 1 && strstr(r.pr.url, "10.1.2.3") && r.s.pres.pages == 1);
    /* the same bytes the Print app sends for that file */
    int n = read_all(&r, "sketch/sk0002.png", file, SK_FILE_MAX);
    CHECK(n > 0);
    fprinter_t direct;
    memset(&direct, 0, sizeof(direct));
    direct.status = 200;
    pj_net_t net = { PJ_PWG, fake_post, &direct };
    pj_result_t res;
    CHECK(pj_print_png(&net, "10.1.2.3", file, (size_t)n, &res) == 0);
    CHECK(direct.posts == r.pr.posts && direct.len[0] == r.pr.len[0] && !memcmp(direct.body[0], r.pr.body[0], direct.len[0]));
    for (int i = 0; i < 4; i++) free(direct.body[i]);
    CHECK(rig_frame(&r, out) == SK_FULL);
    tap(&r, APP_TOUCH_MIDDLE); /* OK: back to the gallery */
    CHECK(r.s.mode == SK_GALLERY && rig_frame(&r, out) == SK_FULL);
    rig_end(&r);
}

static void test_print_errors(void)
{
    static rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    make_pages(&r, 1);
    r.pr.status = -1; /* PJ_NET_UNREACHABLE: the printer is off */
    tap(&r, APP_TOUCH_MIDDLE);
    tap(&r, APP_TOUCH_MIDDLE);
    tap(&r, APP_TOUCH_RIGHT);
    tap(&r, APP_TOUCH_MIDDLE);
    tap(&r, APP_TOUCH_MIDDLE);
    sk_run_print(&r.s);
    CHECK(r.s.mode == SK_RESULT && r.s.pres.msg[0] && r.s.pres.jobs == 0); /* a plain message, nothing crashed */
    tap(&r, APP_TOUCH_MIDDLE);
    CHECK(r.s.mode == SK_GALLERY);
    /* a firmware that cannot print: the item says so instead of failing */
    r.s.env.net.post = 0;
    tap(&r, APP_TOUCH_MIDDLE);
    tap(&r, APP_TOUCH_RIGHT);
    tap(&r, APP_TOUCH_MIDDLE);
    CHECK(r.s.mode == SK_RESULT && strstr(r.s.pres.msg, "not available"));
    /* a page file that is gone */
    tap(&r, APP_TOUCH_MIDDLE);
    r.s.env.net.post = fake_post;
    r.fs.fs.remove(&r.fs, "sketch/sk0001.png");
    r.s.act_num = 1;
    CHECK(sk_print_page(&r.s, 1) < 0 && strstr(r.s.pres.msg, "could not be read"));
    rig_end(&r);
}

static void tile_center(const rig_t *r, int idx, int *x, int *y)
{
    int slot = idx % r->s.L.per, tx, ty;
    gal_tile_xy(&r->s.L, slot, &tx, &ty);
    *x = tx + r->s.L.tw / 2, *y = ty + r->s.L.th / 2;
}

static void test_pen_in_the_menus(void)
{
    static rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    make_pages(&r, 3);
    tap(&r, APP_TOUCH_MIDDLE);
    uint8_t keep[TS * TH];
    memcpy(keep, r.s.page.px, sizeof(keep));
    int x, y, x2, y2;
    tile_center(&r, 2, &x, &y);
    tile_center(&r, 3, &x2, &y2);
    /* a drag across tiles opens nothing */
    rig_pen(&r, x, y, APP_PEN_TIP | APP_PEN_IN_RANGE, 20000);
    rig_pen(&r, (x + x2) / 2, y, APP_PEN_TIP | APP_PEN_IN_RANGE, 20000);
    rig_pen(&r, x2, y2, APP_PEN_TIP | APP_PEN_IN_RANGE, 20000);
    rig_pen(&r, x2, y2, APP_PEN_IN_RANGE, 0);
    CHECK(r.s.mode == SK_GALLERY && r.s.sel == 3); /* the pen over a tile selects it, as the touch bar does */
    /* out and back to the same tile is a drag too */
    rig_pen(&r, x, y, APP_PEN_TIP | APP_PEN_IN_RANGE, 20000);
    rig_pen(&r, x + 150, y, APP_PEN_TIP | APP_PEN_IN_RANGE, 20000);
    rig_pen(&r, x + 2, y, APP_PEN_TIP | APP_PEN_IN_RANGE, 20000);
    rig_pen(&r, x + 2, y, APP_PEN_IN_RANGE, 0);
    CHECK(r.s.mode == SK_GALLERY);
    /* a press that lifts on another tile opens nothing */
    rig_pen(&r, x, y, APP_PEN_TIP | APP_PEN_IN_RANGE, 20000);
    rig_pen(&r, x2, y2, APP_PEN_IN_RANGE, 0);
    CHECK(r.s.mode == SK_GALLERY);
    /* a press and release on one tile opens its action list */
    rig_pen(&r, x, y, APP_PEN_TIP | APP_PEN_IN_RANGE, 20000);
    rig_pen(&r, x + 3, y + 2, APP_PEN_TIP | APP_PEN_IN_RANGE, 20000);
    rig_pen(&r, x + 3, y + 2, APP_PEN_IN_RANGE, 0);
    CHECK(r.s.mode == SK_ACTIONS && r.s.act_num == r.s.pg.nums[r.s.pg.n - 2]);
    /* in the list: a tap on "Back" returns to the gallery; the page took no ink from all of this */
    int ix = r.s.lg.x + 50, iy = r.s.lg.y0 + 4 * LIST_PITCH - 30; /* Back is the fifth row since Settings */
    rig_pen(&r, ix, iy, APP_PEN_TIP | APP_PEN_IN_RANGE, 20000);
    rig_pen(&r, ix, iy, APP_PEN_IN_RANGE, 0);
    CHECK(r.s.mode == SK_GALLERY);
    CHECK(!memcmp(keep, r.s.page.px, sizeof(keep)) && !r.s.dirty && !r.s.stroke);
    rig_end(&r);
}

static void test_new_page_tile_and_empty_rule(void)
{
    static rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    make_pages(&r, 1);
    tap(&r, APP_TOUCH_MIDDLE);
    tap(&r, APP_TOUCH_LEFT); /* the new-page tile */
    tap(&r, APP_TOUCH_MIDDLE);
    CHECK(r.s.mode == SK_DRAW && r.s.pg.n == 2); /* a page with ink: a new one is made */
    tap(&r, APP_TOUCH_MIDDLE);
    tap(&r, APP_TOUCH_LEFT);
    tap(&r, APP_TOUCH_MIDDLE);
    CHECK(r.s.mode == SK_GALLERY && strstr(r.s.gnote, "still empty") && r.s.pg.n == 2); /* no run of empty pages */
    rig_end(&r);
}

static void test_orientations_render(void)
{
    for (int o = 0; o < 4; o++) {
        static rig_t r;
        rig_new(&r, o);
        make_pages(&r, 6);
        tap(&r, APP_TOUCH_MIDDLE);
        rect_t out[SK_MAX_RECTS];
        CHECK(rig_frame(&r, out) == SK_FULL);
        int w, b = tile_black(&r, 1, &w);
        CHECK(b > 50 && w > 3000);
        sk_set_orient(&r.s, (o + 1) % 4, &r.s.bar); /* turned while the gallery is up: drawn again */
        CHECK(r.s.want_full && r.s.mode == SK_GALLERY);
        rig_end(&r);
    }
}

/* The action list: a framed preview like a gallery tile, and "Settings" opens the phone's settings form (the printer) */
static void test_actions_frame_and_settings(void)
{
    static rig_t r;
    for (int o = 0; o < 4; o++) {
        rig_new(&r, o);
        make_pages(&r, 2);
        tap(&r, APP_TOUCH_MIDDLE);
        tap(&r, APP_TOUCH_MIDDLE); /* the list of the page on screen */
        CHECK(r.s.mode == SK_ACTIONS && r.s.list_n == 5);
        gfx_t g;
        gfx_init(&g, r.s.ui.px, r.s.ui.w, r.s.ui.h, r.s.ui.stride, o);
        int px = r.s.L.ax1 - r.s.L.tw - 12, py = (r.s.L.ay0 + r.s.L.ay1) / 2 - r.s.L.th / 2;
        CHECK(!gfx_get(&g, px - 9, py + r.s.L.th / 2) && !gfx_get(&g, px + r.s.L.tw + 8, py + r.s.L.th / 2)); /* the frame: left and right edge */
        CHECK(!gfx_get(&g, px + r.s.L.tw / 2, py - 9) && !gfx_get(&g, px + r.s.L.tw / 2, py + r.s.L.th + 8)); /* top and bottom */
        CHECK(gfx_get(&g, px + r.s.L.tw / 2, py + r.s.L.th / 2)); /* inside it is paper, the thumbnail goes there */
        CHECK(px + r.s.L.tw + 12 <= r.s.L.ax1 + 12 && py >= r.s.L.ay0 && py + r.s.L.th <= r.s.L.ay1); /* on the screen, level with the list */
        CHECK(py + r.s.L.th / 2 > r.s.lg.y0 && py + r.s.L.th / 2 < r.s.lg.y0 + 4 * LIST_PITCH);
        rig_end(&r);
    }
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    make_pages(&r, 2);
    tap(&r, APP_TOUCH_MIDDLE);
    tap(&r, APP_TOUCH_MIDDLE);
    for (int i = 0; i < 3; i++) tap(&r, APP_TOUCH_RIGHT); /* Open, Print, Send to phone, Settings */
    CHECK(r.s.act_sel == 3);
    tap(&r, APP_TOUCH_MIDDLE);
    CHECK(r.s.mode == SK_SHARE && r.s.share_req == 2 && r.s.share_kind == 2); /* the settings page, not the files */
    app_webshare_status_t st;
    memset(&st, 0, sizeof(st));
    st.running = 1;
    st.mode = APP_WEBSHARE_HOME;
    strcpy(st.address, "192.168.1.9");
    strcpy(st.code, "123456");
    sk_share_update(&r.s, 0, &st);
    CHECK(r.s.share.settings && r.s.share.up);
    tap(&r, APP_TOUCH_MIDDLE); /* any touch ends it */
    CHECK(r.s.mode == SK_GALLERY && r.s.share_req == -1);
    tap(&r, APP_TOUCH_MIDDLE);
    for (int i = 0; i < 2; i++) tap(&r, APP_TOUCH_RIGHT);
    tap(&r, APP_TOUCH_MIDDLE);
    CHECK(r.s.mode == SK_SHARE && r.s.share_req == 1 && r.s.share_kind == 1); /* "Send to phone" is still the files */
    sk_share_update(&r.s, 0, &st);
    CHECK(!r.s.share.settings);
    rig_end(&r);
}

/* The dithered thumbnail keeps the area average of the page: per 24 x 24 block of thumbnail pixels within 3 % */
static void test_dither_keeps_the_average(void)
{
    static uint8_t px[TS * TH];
    canvas_t c = { px, TW, TH, TS };
    uint32_t seed = 12345;
    for (int y = 0; y < TH; y++)
        for (int x = 0; x < TW; x++) {
            seed = seed * 1664525u + 1013904223u;
            int dens = x * 256 / TW + (y > TH / 2 ? 0 : 0); /* black density rises left to right */
            int white = y < TH / 4 ? 1 : y < TH / 2 ? (int)((seed >> 16) & 255) >= dens : y < 3 * TH / 4 ? ((x / 3 + y / 3) & 1) : x > TW / 2;
            if (white) px[y * TS + (x >> 3)] |= (uint8_t)(0x80 >> (x & 7));
            else px[y * TS + (x >> 3)] &= (uint8_t)~(0x80 >> (x & 7));
        }
    gal_cache_t cache;
    memset(&cache, 0, sizeof(cache));
    const uint8_t *t = gal_make(&cache, 1, &c);
    CHECK(t != 0);
    double worst = 0;
    for (int by = 0; by + 24 <= GAL_TH; by += 24)
        for (int bx = 0; bx + 24 <= GAL_TW; bx += 24) {
            int dw = 0, sw = 0, sn = 0;
            for (int y = by; y < by + 24; y++)
                for (int x = bx; x < bx + 24; x++) dw += gal_bit(t, x, y);
            for (int y = by * TH / GAL_TH; y < (by + 24) * TH / GAL_TH; y++)
                for (int x = bx * TW / GAL_TW; x < (bx + 24) * TW / GAL_TW; x++) sw += cv_get(&c, x, y), sn++;
            double d = (double)dw / 576.0 - (double)sw / (double)sn;
            if (d < 0) d = -d;
            if (d > worst) worst = d;
        }
    printf("  dither: worst block error %.4f\n", worst);
    CHECK(worst < 0.03);
    gal_cache_free(&cache);
}

/* The hover pointer on the gallery: drawn and lifted with partial refreshes only, and the panel (full refresh plus the
   pushed rectangles) shows the clean screen once it is gone */
static void test_pointer_leaves_no_marks(void)
{
    static rig_t r;
    static uint8_t panel[TS * TH];
    rect_t out[SK_MAX_RECTS];
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    make_pages(&r, 8);
    rig_pen(&r, 10, 10, 0, 0); /* the pen leaves the tablet */
    tap(&r, APP_TOUCH_MIDDLE);
    panel_push(panel, &r, out, rig_frame(&r, out));
    int f = r.fulls;
    CHECK(!memcmp(panel, r.s.ui.px, sizeof(panel))); /* the screen is the clean one */
    uint32_t seed = 7;
    for (int i = 0; i < 40; i++) { /* the pen wanders over tiles, labels, gaps and the bar's strip */
        seed = seed * 1664525u + 1013904223u;
        int x = (int)((seed >> 8) % TW), y = (int)((seed >> 4) % TH);
        rig_pen(&r, x, y, APP_PEN_IN_RANGE, 0);
        r.now += 200;
        panel_push(panel, &r, out, rig_frame(&r, out));
        CHECK(r.fulls == f); /* never a full refresh while hovering */
        if (r.s.cursor) CHECK(box_empty(&r.s.cur_box) || memcmp(r.fb, r.s.ui.px, sizeof(panel)) != 0);
        CHECK(!memcmp(panel, r.fb, sizeof(panel))); /* everything drawn was pushed */
    }
    rig_pen(&r, 900, 700, APP_PEN_IN_RANGE, 0);
    r.now += 200;
    panel_push(panel, &r, out, rig_frame(&r, out));
    CHECK(r.s.cursor && memcmp(r.fb, r.s.ui.px, sizeof(panel)) != 0); /* the arrow is there */
    rig_pen(&r, 900, 700, 0, 0); /* the pen leaves the tablet */
    r.now += 200;
    panel_push(panel, &r, out, rig_frame(&r, out));
    CHECK(!r.s.cursor && r.fulls == f);
    /* the clean screen, except the highlight the pen left on the tile it last pointed at: ui has that too */
    CHECK(!memcmp(panel, r.s.ui.px, sizeof(panel)));
    rig_end(&r);
}

static int sel_slot_rect(const rig_t *r, int sel, box_t *b)
{
    gfx_t g;
    int x, y, slot = sel % r->s.L.per;
    gfx_init(&g, r->s.ui.px, TW, TH, TS, r->s.orient);
    gal_tile_xy(&r->s.L, slot, &x, &y);
    ui_fb_box(&g, x - 10, y - 10, x + r->s.L.tw + 9, y + r->s.L.th + GAL_LABEL, b);
    return 1;
}

/* Hovering a tile or a row selects it (the touch bar's selection); the partial rectangles cover the old and the new
   highlight and nothing else of size */
static void test_hover_moves_the_highlight(void)
{
    static rig_t r;
    static uint8_t panel[TS * TH];
    rect_t out[SK_MAX_RECTS];
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    make_pages(&r, 8);
    rig_pen(&r, 10, 10, 0, 0); /* the pen leaves the tablet */
    tap(&r, APP_TOUCH_MIDDLE);
    panel_push(panel, &r, out, rig_frame(&r, out));
    int x, y, old = r.s.sel;
    tile_center(&r, 4, &x, &y);
    rig_pen(&r, x, y, APP_PEN_IN_RANGE, 0);
    r.now += 200;
    int n = rig_frame(&r, out);
    CHECK(r.s.sel == 4 && n > 0 && n != SK_FULL);
    box_t a, b;
    sel_slot_rect(&r, old, &a);
    sel_slot_rect(&r, 4, &b);
    long area = 0;
    for (int i = 0; i < n; i++) {
        area += (long)out[i].w * out[i].h;
        /* every pushed rectangle lies inside the old tile, the new tile or the arrow's own box */
        int in_a = out[i].x + out[i].w > a.x0 && out[i].x <= a.x1 + 8 && out[i].y + out[i].h > a.y0 && out[i].y <= a.y1;
        int in_b = out[i].x + out[i].w > b.x0 && out[i].x <= b.x1 + 8 && out[i].y + out[i].h > b.y0 && out[i].y <= b.y1;
        CHECK(in_a || in_b);
    }
    CHECK(area < 3L * (long)(r.s.L.tw + 40) * (r.s.L.th + GAL_LABEL + 40)); /* two tiles and a pointer, not the screen */
    panel_push(panel, &r, out, n);
    CHECK(!memcmp(panel, r.fb, sizeof(panel)));
    /* the same selection from the touch bar: a partial refresh too, and the pointer stays */
    r.now += 1000;
    tap(&r, APP_TOUCH_RIGHT);
    n = rig_frame(&r, out);
    CHECK(r.s.sel == 5 && n > 0 && n != SK_FULL && r.s.cursor);
    panel_push(panel, &r, out, n);
    CHECK(!memcmp(panel, r.fb, sizeof(panel)));
    /* a list row */
    tap(&r, APP_TOUCH_MIDDLE);
    panel_push(panel, &r, out, rig_frame(&r, out));
    CHECK(r.s.mode == SK_ACTIONS);
    int f = r.fulls;
    rig_pen(&r, r.s.lg.x + 100, r.s.lg.y0 + 2 * LIST_PITCH - 25, APP_PEN_IN_RANGE, 0);
    r.now += 200;
    n = rig_frame(&r, out);
    CHECK(r.s.act_sel == 2 && n > 0 && n != SK_FULL && r.fulls == f);
    panel_push(panel, &r, out, n);
    CHECK(!memcmp(panel, r.fb, sizeof(panel)));
    rig_end(&r);
}

/* The pointer in the four positions of the X: hovering a row of the action list selects it and the glass matches */
static void test_pointer_in_every_position(void)
{
    static rig_t r;
    static uint8_t panel[TS * TH];
    rect_t out[SK_MAX_RECTS];
    for (int o = 0; o < 4; o++) {
        rig_new(&r, o);
        make_pages(&r, 3);
        rig_pen(&r, 10, 10, 0, 0);
        tap(&r, APP_TOUCH_MIDDLE);
        panel_push(panel, &r, out, rig_frame(&r, out));
        int f = r.fulls;
        int ux, uy, tx, ty;
        gal_tile_xy(&r.s.L, 2, &tx, &ty);
        ux = tx + r.s.L.tw / 2, uy = ty + r.s.L.th / 2;
        rig_pen_user(&r, ux, uy, APP_PEN_IN_RANGE, 0);
        r.now += 200;
        int n = rig_frame(&r, out);
        CHECK(r.s.sel == 2 && n > 0 && n != SK_FULL && r.s.cursor && r.fulls == f);
        panel_push(panel, &r, out, n);
        CHECK(!memcmp(panel, r.fb, sizeof(panel)));
        sk_pen(&r.s, 0, 0, 0, 0, r.now += 5); /* out of range */
        r.now += 200;
        panel_push(panel, &r, out, rig_frame(&r, out));
        CHECK(!r.s.cursor && !memcmp(panel, r.s.ui.px, sizeof(panel)));
        rig_end(&r);
    }
}

/* ---- the pen on the hint pills and the paging arrows, the tablet buttons in the menus ---- */

static void ctl_center(const rig_t *r, int c, int *x, int *y)
{
    int x0, y0, x1, y1;
    if (c == CTL_PREV || c == CTL_NEXT) ui_nav_box(&r->s.L, c, &x0, &y0, &x1, &y1);
    else {
        int z = c == CTL_UP ? APP_TOUCH_LEFT : c == CTL_DOWN ? APP_TOUCH_RIGHT : APP_TOUCH_MIDDLE;
        const char *lab = c == CTL_UP ? "Up" : c == CTL_DOWN ? "Down" : r->s.mode == SK_GALLERY ? (r->s.sel == 0 ? "New page" : "Open") : "Select";
        ui_pill_box(r->s.orient % 2 ? TH : TW, r->s.orient % 2 ? TW : TH, &r->s.bar, z, lab, c == CTL_UP ? PILL_UP : c == CTL_DOWN ? PILL_DOWN : PILL_NONE, &x0, &y0, &x1, &y1);
    }
    *x = (x0 + x1) / 2, *y = (y0 + y1) / 2;
}
static void pen_tap_ctl(rig_t *r, int c)
{
    int x, y;
    ctl_center(r, c, &x, &y);
    rig_pen_user(r, x, y, APP_PEN_IN_RANGE, 0);
    rig_pen_user(r, x, y, APP_PEN_TIP | APP_PEN_IN_RANGE, 20000);
    rig_pen_user(r, x + 2, y + 1, APP_PEN_TIP | APP_PEN_IN_RANGE, 20000);
    rig_pen_user(r, x + 2, y + 1, APP_PEN_IN_RANGE, 0);
}
static void btn(rig_t *r, int n) /* tablet button n (1..8) */
{
    sk_pen_key(&r->s, 1u << (n - 1), 1u << (n - 1), r->now += 50);
    sk_pen_key(&r->s, 0, 1u << (n - 1), r->now);
}
static int sel_ok(const rig_t *r) { return r->s.sel >= 0 && r->s.sel < r->s.pg.n + 1; }

static void test_pen_pills_and_paging_arrows(void)
{
    static rig_t r;
    static uint8_t keep[TS * TH], panel[TS * TH];
    rect_t out[SK_MAX_RECTS];
    for (int o = 0; o < 4; o += 3) { /* landscape and portrait */
        rig_new(&r, o);
        make_pages(&r, 20);
        rig_pen(&r, 10, 10, 0, 0);
        tap(&r, APP_TOUCH_MIDDLE);
        panel_push(panel, &r, out, rig_frame(&r, out));
        memcpy(keep, r.s.page.px, sizeof(keep));
        int per = r.s.L.per, f = r.fulls;
        CHECK(r.s.pg.n + 1 > per && r.s.sel == 1);
        pen_tap_ctl(&r, CTL_PREV); /* no earlier screen: nothing there to hit */
        CHECK(r.s.sel == 1 && r.s.mode == SK_GALLERY);
        pen_tap_ctl(&r, CTL_DOWN);
        CHECK(r.s.sel == 2);
        pen_tap_ctl(&r, CTL_UP);
        pen_tap_ctl(&r, CTL_UP);
        CHECK(r.s.sel == 0);
        pen_tap_ctl(&r, CTL_DOWN);
        panel_push(panel, &r, out, rig_frame(&r, out));
        CHECK(r.s.sel == 1 && r.fulls == f); /* selection moves stay partial */
        pen_tap_ctl(&r, CTL_NEXT);
        CHECK(r.s.sel == 1 + per && r.s.sel / per == 1 && sel_ok(&r));
        CHECK(rig_frame(&r, out) == SK_FULL && r.fulls == f + 1); /* a new screen of tiles: full */
        pen_tap_ctl(&r, CTL_NEXT); /* the last screen has no next */
        CHECK(r.s.sel / per == 1 && rig_frame(&r, out) != SK_FULL);
        pen_tap_ctl(&r, CTL_PREV);
        CHECK(r.s.sel == 1 && rig_frame(&r, out) == SK_FULL && r.fulls == f + 2);
        pen_tap_ctl(&r, CTL_SEL);
        CHECK(r.s.mode == SK_ACTIONS);
        pen_tap_ctl(&r, CTL_DOWN);
        CHECK(r.s.act_sel == 1);
        pen_tap_ctl(&r, CTL_UP);
        CHECK(r.s.act_sel == 0);
        pen_tap_ctl(&r, CTL_SEL); /* Open: back to drawing */
        CHECK(r.s.mode == SK_DRAW);
        CHECK(!memcmp(keep, r.s.page.px, sizeof(keep)) || r.s.cur != r.s.pg.nums[r.s.pg.n - 1]); /* no ink from any tap */
        rig_end(&r);
    }
}

static void test_hover_on_arrows_and_pills(void)
{
    static rig_t r;
    static uint8_t panel[TS * TH];
    rect_t out[SK_MAX_RECTS];
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    make_pages(&r, 20);
    rig_pen(&r, 10, 10, 0, 0);
    tap(&r, APP_TOUCH_MIDDLE);
    panel_push(panel, &r, out, rig_frame(&r, out));
    int f = r.fulls, x, y, sel = r.s.sel;
    ctl_center(&r, CTL_NEXT, &x, &y);
    rig_pen_user(&r, x, y, APP_PEN_IN_RANGE, 0);
    r.now += 200;
    int n = rig_frame(&r, out);
    CHECK(r.s.hot == CTL_NEXT && r.s.sel == sel && n > 0 && n != SK_FULL && r.fulls == f);
    panel_push(panel, &r, out, n);
    CHECK(!memcmp(panel, r.fb, sizeof(panel)));
    int black = fb_black(&r);
    ctl_center(&r, CTL_DOWN, &x, &y);
    rig_pen_user(&r, x, y, APP_PEN_IN_RANGE, 0);
    r.now += 200;
    n = rig_frame(&r, out);
    CHECK(r.s.hot == CTL_DOWN && n > 0 && n != SK_FULL && r.s.sel == sel);
    panel_push(panel, &r, out, n);
    CHECK(!memcmp(panel, r.fb, sizeof(panel)) && fb_black(&r) != black);
    sk_pen(&r.s, 0, 0, 0, 0, r.now += 5); /* out of range: the highlight goes */
    r.now += 200;
    panel_push(panel, &r, out, rig_frame(&r, out));
    CHECK(r.s.hot == CTL_NONE && !memcmp(panel, r.s.ui.px, sizeof(panel)));
    rig_end(&r);
}

static void test_tablet_buttons_in_the_menus(void)
{
    static rig_t r;
    rect_t out[SK_MAX_RECTS];
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    make_pages(&r, 20);
    rig_pen(&r, 10, 10, 0, 0);
    tap(&r, APP_TOUCH_MIDDLE);
    rig_frame(&r, out);
    uint8_t *keep = malloc(sizeof(r.s.page.px[0]) * (size_t)TS * TH);
    memcpy(keep, r.s.page.px, (size_t)TS * TH);
    int per = r.s.L.per, f = r.fulls;
    btn(&r, 2);
    CHECK(r.s.sel == 2 && rig_frame(&r, out) != SK_FULL && r.fulls == f); /* 2 forward, partial */
    btn(&r, 1);
    btn(&r, 1);
    CHECK(r.s.sel == 0);
    btn(&r, 1);
    CHECK(r.s.sel == 0); /* not before the first tile */
    btn(&r, 8);
    CHECK(r.s.sel == per && rig_frame(&r, out) == SK_FULL); /* 8: next screen of tiles, full */
    btn(&r, 8);
    CHECK(r.s.sel / per == 1 && sel_ok(&r)); /* never past the last screen */
    btn(&r, 2);
    for (int i = 0; i < 30; i++) btn(&r, 2);
    CHECK(r.s.sel == r.s.pg.n && sel_ok(&r));
    btn(&r, 7);
    CHECK(r.s.sel / per == 0 && sel_ok(&r) && rig_frame(&r, out) == SK_FULL); /* 7: previous screen */
    btn(&r, 4);
    CHECK(r.s.mode == SK_ACTIONS); /* 4 opens the selected */
    btn(&r, 2);
    btn(&r, 8);
    CHECK(r.s.act_sel == 2); /* in lists 1/2 and 7/8 move */
    btn(&r, 1);
    CHECK(r.s.act_sel == 1);
    btn(&r, 5);
    CHECK(r.s.mode == SK_GALLERY); /* 5: back */
    btn(&r, 5);
    CHECK(r.s.mode == SK_DRAW && rig_frame(&r, out) == SK_FULL); /* in the gallery 5 closes it */
    CHECK(!memcmp(keep, r.s.page.px, (size_t)TS * TH) && !r.s.stroke);
    free(keep);
    tap(&r, APP_TOUCH_MIDDLE);
    int cur = r.s.cur;
    btn(&r, 6); /* 6: the New page tile; this page has ink, so a new page opens */
    CHECK(r.s.mode == SK_DRAW && r.s.cur != cur && sk_is_blank(&r.s.page));
    tap(&r, APP_TOUCH_MIDDLE);
    btn(&r, 6); /* the new page is empty: the rule "no run of empty pages" answers with a note */
    CHECK(r.s.mode == SK_GALLERY && r.s.sel == 0 && r.s.gnote[0]);
    rig_end(&r);
}

int main(void)
{
    test_layout_four_positions();
    test_selection_and_paging();
    test_thumbnails_cache_and_edit();
    test_full_refresh_only_when_a_screen_opens_or_closes();
    test_screen_has_thumbnails();
    test_dither_keeps_the_average();
    test_pointer_leaves_no_marks();
    test_hover_moves_the_highlight();
    test_pointer_in_every_position();
    test_print_from_the_action_list();
    test_print_errors();
    test_pen_in_the_menus();
    test_new_page_tile_and_empty_rule();
    test_orientations_render();
    test_actions_frame_and_settings();
    test_pen_pills_and_paging_arrows();
    test_hover_on_arrows_and_pills();
    test_tablet_buttons_in_the_menus();
    return CHECK_DONE("test_gallery");
}
