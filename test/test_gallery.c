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
    CHECK(r.s.sel == 20); /* never past the oldest page */
    for (int i = 0; i < 5; i++) tap(&r, APP_TOUCH_RIGHT);
    CHECK(r.s.sel == 20);
    rig_end(&r);
}

static int thumb_sum(const uint8_t *t)
{
    int sum = 0;
    for (int y = 0; y < GAL_TH; y++)
        for (int x = 0; x < GAL_TW; x++) sum += gal_px(t, x, y);
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
    CHECK(before < GAL_TW * GAL_TH * 15); /* ink makes it darker than white */
    int mid = 0; /* box filtering gives grays, not only black and white */
    for (int y = 0; y < GAL_TH; y++)
        for (int x = 0; x < GAL_TW; x++) mid += gal_px(t, x, y) > 0 && gal_px(t, x, y) < 15;
    CHECK(mid > 50);
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

static void test_full_refresh_on_every_change(void)
{
    static rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    make_pages(&r, 3);
    rect_t out[SK_MAX_RECTS];
    rig_frame(&r, out);
    int f = r.fulls;
    tap(&r, APP_TOUCH_MIDDLE); /* open the gallery */
    CHECK(rig_frame(&r, out) == SK_FULL && r.fulls == f + 1 && r.gray_mode);
    tap(&r, APP_TOUCH_RIGHT); /* move */
    CHECK(rig_frame(&r, out) == SK_FULL && r.fulls == f + 2);
    tap(&r, APP_TOUCH_MIDDLE); /* a drawing tile: its action list */
    CHECK(rig_frame(&r, out) == SK_FULL && r.fulls == f + 3);
    tap(&r, APP_TOUCH_MIDDLE); /* Open: back to drawing */
    CHECK(rig_frame(&r, out) == SK_FULL && r.fulls == f + 4 && !r.gray_mode && r.s.mode == SK_DRAW);
    sk_pen_key(&r.s, 1u << 6, 1u << 6, r.now += 50); /* previous page */
    sk_pen_key(&r.s, 0, 1u << 6, r.now);
    CHECK(rig_frame(&r, out) == SK_FULL && r.fulls == f + 5); /* a page change flashes */
    int parts = r.parts;
    rig_line(&r, 200, 600, 900, 700, 40000, 0);
    int n = rig_frame(&r, out);
    CHECK(n > 0 && r.fulls == f + 5 && r.parts > parts); /* ink stays a partial refresh */
    rig_end(&r);
}

static void gray_has_midtones(const rig_t *r, int *mid, int *black)
{
    *mid = *black = 0;
    for (int i = 0; i < TW * TH / 2; i++)
        for (int k = 0; k < 2; k++) {
            int v = k ? r->gray[i] & 15 : r->gray[i] >> 4;
            *mid += v > 2 && v < 13;
            *black += v == 0;
        }
}

static void test_gray_screen_has_thumbnails(void)
{
    static rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    make_pages(&r, 4);
    tap(&r, APP_TOUCH_MIDDLE);
    rect_t out[SK_MAX_RECTS];
    CHECK(rig_frame(&r, out) == SK_FULL);
    int mid, black;
    gray_has_midtones(&r, &mid, &black);
    CHECK(mid > 500 && black > 500); /* thumbnails in grays, frames and text in black */
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
    CHECK(r.s.mode == SK_GALLERY && r.s.sel == 1);
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
    int ix = r.s.list_x + 50, iy = r.s.list_y0 + 4 * 110 - 30; /* Back is the fifth row since Settings */
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
        int mid, black;
        gray_has_midtones(&r, &mid, &black);
        CHECK(mid > 300 && black > 300);
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
        CHECK(py + r.s.L.th / 2 > r.s.list_y0 && py + r.s.L.th / 2 < r.s.list_y0 + 4 * 110);
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

int main(void)
{
    test_layout_four_positions();
    test_selection_and_paging();
    test_thumbnails_cache_and_edit();
    test_full_refresh_on_every_change();
    test_gray_screen_has_thumbnails();
    test_print_from_the_action_list();
    test_print_errors();
    test_pen_in_the_menus();
    test_new_page_tile_and_empty_rule();
    test_orientations_render();
    test_actions_frame_and_settings();
    return CHECK_DONE("test_gallery");
}
