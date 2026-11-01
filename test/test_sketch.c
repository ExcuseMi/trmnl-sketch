/* The app without the firmware: pen, tablet buttons, touch bar, pages, autosave, a restart. SPDX-License-Identifier: MIT */
#include "check.h"
#include <stdio.h>
#include "util.h"
#include "pngio.h"

static int px_black(const rig_t *r, int x, int y) { return !cv_get(&r->s.page, x, y); }
static int col_thickness(const rig_t *r, int x, int y0, int y1)
{
    int n = 0;
    for (int y = y0; y < y1; y++) n += px_black(r, x, y);
    return n;
}
static void key(rig_t *r, int bit)
{
    r->now += 50;
    sk_pen_key(&r->s, 1u << bit, 1u << bit, r->now);
    sk_pen_key(&r->s, 0, 1u << bit, r->now);
}
static void touch(rig_t *r, int zone, int gesture)
{
    r->now += 50;
    sk_touch(&r->s, zone, gesture, r->now);
}
static void idle(rig_t *r, uint32_t ms)
{
    r->now += ms;
    sk_tick(&r->s, r->now);
}
/* the pen leaves and the corner notes run out: the screen shows the page and nothing else */
static void settle(rig_t *r)
{
    rig_pen(r, 10, 10, 0, 0);
    idle(r, 3000);
}
static int page_is(const rig_t *r, const uint8_t *snap) { return !memcmp(r->s.page.px, snap, (size_t)TS * TH); }

static void test_legend_and_ink(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    CHECK(r.s.legend && r.s.note_on && !same_page_and_fb(&r)); /* the button legend is on the screen at first */
    CHECK(fb_black(&r) > 500);
    rig_line(&r, 900, 800, 1100, 900, 40000, 0);
    CHECK(!r.s.legend && !r.s.note_on); /* the first stroke takes it away, nothing of it is left */
    settle(&r);
    CHECK(same_page_and_fb(&r));
    CHECK(px_black(&r, 1000, 850)); /* the ink is where the pen was */
    CHECK(!px_black(&r, 1000, 600));
    rig_end(&r);
}

static void test_pressure_and_size(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    rig_line(&r, 200, 300, 600, 300, 3000, 0);
    rig_line(&r, 200, 500, 600, 500, 65000, 0);
    int light = col_thickness(&r, 400, 280, 320), hard = col_thickness(&r, 400, 480, 520);
    CHECK(light >= 1 && hard > light * 2); /* a harder press, a thicker line */
    sk_set_size(&r.s, 2, r.now += 50); /* size: medium -> thick (the toolbar's thick; tablet button 4 is the toolbar now) */
    rig_line(&r, 200, 700, 600, 700, 65000, 0);
    int thick = col_thickness(&r, 400, 680, 720);
    CHECK(thick > hard);
    sk_set_size(&r.s, 0, r.now += 50); /* thick -> thin */
    rig_line(&r, 200, 900, 600, 900, 65000, 0);
    int thin = col_thickness(&r, 400, 880, 920);
    CHECK(thin < hard && thin >= 1);
    CHECK(!strcmp(r.cfg_size, "0")); /* the size is kept */
    rig_end(&r);
}

static void test_eraser(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    rig_line(&r, 300, 500, 900, 500, 65000, 0);
    CHECK(px_black(&r, 600, 500));
    rig_line(&r, 500, 500, 700, 500, 30000, APP_PEN_BARREL); /* the pen's button held: the eraser */
    CHECK(!px_black(&r, 600, 500) && px_black(&r, 350, 500) && px_black(&r, 850, 500));
    settle(&r);
    CHECK(same_page_and_fb(&r));
    key(&r, 2); /* the eraser on the tablet button */
    rig_line(&r, 300, 500, 400, 500, 30000, 0);
    CHECK(!px_black(&r, 350, 500));
    rig_line(&r, 300, 500, 400, 500, 30000, APP_PEN_BARREL2); /* the button inverts it: ink again */
    CHECK(px_black(&r, 350, 500));
    key(&r, 2);
    rig_line(&r, 300, 600, 400, 600, 30000, 0);
    CHECK(px_black(&r, 350, 600)); /* off again */
    rig_end(&r);
}

static void test_hover_cursor(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    rig_line(&r, 300, 500, 900, 500, 65000, 0);
    uint8_t page[TS * TH];
    memcpy(page, r.s.page.px, sizeof(page));
    for (int i = 0; i < 60; i++) rig_pen(&r, 200 + i * 20, 400 + (i % 7) * 40, APP_PEN_IN_RANGE, 0); /* hover over ink and paper */
    CHECK(r.s.cursor && !same_page_and_fb(&r)); /* the cursor is drawn */
    CHECK(page_is(&r, page)); /* and the page is not touched by it */
    rig_pen(&r, 700, 500, APP_PEN_IN_RANGE | APP_PEN_BARREL, 0); /* the eraser's cursor is bigger */
    int r1 = r.s.cr;
    rig_pen(&r, 710, 500, APP_PEN_IN_RANGE, 0);
    CHECK(r1 > r.s.cr);
    rig_pen(&r, 1400, 900, 0, 0); /* the pen leaves the tablet */
    CHECK(!r.s.cursor && same_page_and_fb(&r)); /* no mark is left */
    for (int i = 0; i < 40; i++) rig_pen(&r, 300 + i * 30, 500, APP_PEN_IN_RANGE, 0);
    rig_pen(&r, 800, 800, APP_PEN_TIP | APP_PEN_IN_RANGE, 20000); /* ink starts: the cursor goes first */
    CHECK(!r.s.cursor);
    rig_pen(&r, 800, 800, 0, 0);
    CHECK(same_page_and_fb(&r));
    /* cursor and note never fight: a toast shows, a cursor beside it is cleared with it */
    sk_set_size(&r.s, 2, r.now += 50);
    rig_pen(&r, 60, 60, APP_PEN_IN_RANGE, 0); /* under the toast */
    CHECK(!r.s.cursor);
    idle(&r, 3000);
    CHECK(!r.s.note_on);
    rig_pen(&r, 60, 60, 0, 0);
    CHECK(same_page_and_fb(&r));
    rig_end(&r);
}

static void test_undo_redo_buttons(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    static uint8_t snap[4][TS * TH];
    memcpy(snap[0], r.s.page.px, sizeof(snap[0]));
    for (int i = 1; i <= 3; i++) {
        rig_line(&r, 200, 200 + i * 150, 900, 250 + i * 150, 50000, 0);
        memcpy(snap[i], r.s.page.px, sizeof(snap[0]));
    }
    key(&r, 0); /* tablet button 1: undo */
    CHECK(page_is(&r, snap[2]));
    touch(&r, APP_TOUCH_LEFT, APP_GESTURE_TAP); /* the touch bar: undo */
    CHECK(page_is(&r, snap[1]));
    key(&r, 1); /* button 2: redo */
    CHECK(page_is(&r, snap[2]));
    touch(&r, APP_TOUCH_RIGHT, APP_GESTURE_TAP);
    CHECK(page_is(&r, snap[3]));
    settle(&r);
    CHECK(same_page_and_fb(&r));
    key(&r, 1); /* nothing to redo: a note, no change */
    CHECK(page_is(&r, snap[3]));
    for (int i = 0; i < 5; i++) key(&r, 0);
    CHECK(page_is(&r, snap[0])); /* undoing past the start stops there */
    rig_end(&r);
}

static void test_clear(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    rig_line(&r, 200, 300, 1200, 800, 50000, 0);
    rig_line(&r, 300, 900, 1500, 1000, 50000, 0);
    uint8_t drawn[TS * TH], white[TS * TH];
    memcpy(drawn, r.s.page.px, sizeof(drawn));
    memset(white, 0xff, sizeof(white));
    rect_t out[SK_MAX_RECTS];
    sk_flush(&r.s, r.now, out);
    key(&r, 4); /* button 5: clear */
    /* the page was saved before clearing: the flash holds the drawing twice (the page, and a copy that stays) */
    canvas_t chk = { calloc(1, sizeof(drawn)), TW, TH, TS };
    pages_t p;
    pg_init(&p, &r.fs.fs);
    CHECK(pg_load(&p, 1, &chk) == PG_OK && !memcmp(chk.px, drawn, sizeof(drawn)));
    memfile_t *c = memfs_find(&r.fs, "sketch/sk0001-cleared.png");
    CHECK(c != 0);
    memset(chk.px, 0, sizeof(drawn));
    {
        uint8_t *buf = malloc(SK_FILE_MAX);
        memcpy(buf, c->data, c->n);
        CHECK(png_decode(buf, c->n, chk.px, TW, TH, TS) == 0 && !memcmp(chk.px, drawn, sizeof(drawn)));
        free(buf);
    }
    settle(&r);
    CHECK(page_is(&r, white) && same_page_and_fb(&r));
    CHECK(sk_flush(&r.s, r.now + 100, out) == SK_FULL); /* a clean full refresh */
    CHECK(r.fs.removes == 0);
    touch(&r, APP_TOUCH_LEFT, APP_GESTURE_TAP); /* undo the clear */
    CHECK(page_is(&r, drawn) && same_page_and_fb(&r)); /* the exact pixels */
    touch(&r, APP_TOUCH_MIDDLE, APP_GESTURE_HOLD); /* hold the middle: clear */
    CHECK(page_is(&r, white));
    key(&r, 4); /* clearing an empty page is a note, not a stroke */
    touch(&r, APP_TOUCH_LEFT, APP_GESTURE_TAP);
    CHECK(page_is(&r, drawn));
    free(chk.px);
    rig_end(&r);
}

static void test_pages_and_autosave(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    CHECK(memfs_find(&r.fs, "sketch/sk0001.png")); /* page 1 exists from the start */
    rig_line(&r, 200, 300, 1000, 500, 50000, 0);
    uint8_t p1[TS * TH];
    memcpy(p1, r.s.page.px, sizeof(p1));
    memfile_t *f1 = memfs_find(&r.fs, "sketch/sk0001.png");
    size_t blank_size = f1->n;
    idle(&r, 500);
    f1 = memfs_find(&r.fs, "sketch/sk0001.png");
    CHECK(f1->n == blank_size && r.s.dirty); /* not yet: the pen has just left */
    idle(&r, SK_AUTOSAVE_MS);
    f1 = memfs_find(&r.fs, "sketch/sk0001.png");
    CHECK(f1->n != blank_size && !r.s.dirty); /* saved after the pause */
    CHECK(!memfs_find(&r.fs, "sketch/sk0001.tmp"));
    key(&r, 5); /* new page */
    CHECK(r.s.cur == 2 && memfs_find(&r.fs, "sketch/sk0002.png"));
    key(&r, 5); /* again on the empty page: refused, no run of empty pages (owner 2026-11-01) */
    key(&r, 5);
    CHECK(r.s.cur == 2 && !memfs_find(&r.fs, "sketch/sk0003.png"));
    uint8_t white[TS * TH];
    memset(white, 0xff, sizeof(white));
    settle(&r);
    CHECK(page_is(&r, white) && same_page_and_fb(&r));
    rect_t out[SK_MAX_RECTS];
    CHECK(sk_flush(&r.s, r.now + 100, out) == SK_FULL); /* a page change is a clean full refresh */
    rig_line(&r, 600, 700, 1300, 1100, 50000, 0);
    uint8_t p2[TS * TH];
    memcpy(p2, r.s.page.px, sizeof(p2));
    key(&r, 6); /* previous: saves page 2 first */
    settle(&r);
    CHECK(r.s.cur == 1 && page_is(&r, p1) && same_page_and_fb(&r));
    key(&r, 7); /* next */
    CHECK(r.s.cur == 2 && page_is(&r, p2));
    touch(&r, APP_TOUCH_RIGHT, APP_GESTURE_SWIPE_RIGHT); /* a swipe right: the previous page */
    CHECK(r.s.cur == 1);
    touch(&r, APP_TOUCH_RIGHT, APP_GESTURE_SWIPE_LEFT); /* left: the next */
    CHECK(r.s.cur == 2);
    key(&r, 7); /* no page after the last */
    CHECK(r.s.cur == 2 && page_is(&r, p2));
    /* a restart (power off) comes back on the same page with the same pixels; undo does not reach into the old run */
    idle(&r, 3000);
    rig_start(&r, APP_ORIENT_BAR_BOTTOM);
    CHECK(r.s.cur == 2 && page_is(&r, p2) && !hist_can_undo(&r.s.hist));
    key(&r, 6);
    CHECK(r.s.cur == 1 && page_is(&r, p1));
    /* a stroke not yet saved when the power goes is lost, the page before it is whole */
    rig_line(&r, 100, 100, 400, 1300, 50000, 0);
    rig_start(&r, APP_ORIENT_BAR_BOTTOM); /* no tick, no save */
    CHECK(r.s.cur == 1 && page_is(&r, p1));
    /* the page list follows the files: a page removed from the phone is skipped */
    pages_t pp;
    pg_init(&pp, &r.fs.fs);
    pg_scan(&pp);
    CHECK(pp.n == 2);
    rig_end(&r);
}

static void test_corrupt_start(void)
{
    rig_t r;
    memset(&r, 0, sizeof(r));
    memfs_init(&r.fs);
    r.now = 1000;
    memfs_put(&r.fs, "sketch/sk0001.png", "\x89PNG garbage that is not a page", 31);
    rig_start(&r, APP_ORIENT_BAR_BOTTOM);
    uint8_t white[TS * TH];
    memset(white, 0xff, sizeof(white));
    CHECK(r.s.cur == 1 && page_is(&r, white)); /* an empty page */
    CHECK(memfs_find(&r.fs, "sketch/sk0001.png")->n == 31 && r.fs.removes == 0); /* the file is still there */
    rig_line(&r, 200, 300, 1000, 500, 50000, 0);
    idle(&r, 3000); /* the first save moves the unreadable file aside, then writes the page */
    memfile_t *bad = memfs_find(&r.fs, "sketch/sk0001-bad.png");
    CHECK(bad && bad->n == 31 && !memcmp(bad->data, "\x89PNG garbage", 12));
    pages_t p;
    pg_init(&p, &r.fs.fs);
    canvas_t chk = { calloc(1, TS * TH), TW, TH, TS };
    CHECK(pg_load(&p, 1, &chk) == PG_OK && !memcmp(chk.px, r.s.page.px, TS * TH));
    CHECK(r.fs.removes == 0);
    free(chk.px);
    /* a save that fails (full disk) keeps the page dirty and retries; the drawing stays on screen */
    r.fs.fail_write = 1;
    rig_line(&r, 200, 900, 1000, 1000, 50000, 0);
    idle(&r, 3000);
    CHECK(r.s.dirty && r.s.save_failed);
    r.fs.fail_write = 0;
    idle(&r, 6000);
    CHECK(!r.s.dirty && !r.s.save_failed);
    rig_end(&r);
}

static void test_menu_and_share(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    rig_line(&r, 200, 300, 1000, 500, 50000, 0);
    touch(&r, APP_TOUCH_MIDDLE, APP_GESTURE_TAP);
    CHECK(r.s.mode == SK_GALLERY && sk_screen_gray(&r.s));
    rig_pen(&r, 800, 800, APP_PEN_TIP | APP_PEN_IN_RANGE, 30000); /* the pen draws nothing in the menu */
    rig_pen(&r, 900, 900, 0, 0);
    touch(&r, APP_TOUCH_MIDDLE, APP_GESTURE_TAP); /* the page on the flash: its action list */
    CHECK(r.s.mode == SK_ACTIONS && r.s.act_sel == 0);
    touch(&r, APP_TOUCH_RIGHT, APP_GESTURE_TAP);
    touch(&r, APP_TOUCH_RIGHT, APP_GESTURE_TAP);
    touch(&r, APP_TOUCH_MIDDLE, APP_GESTURE_TAP); /* Send to phone */
    CHECK(r.s.mode == SK_SHARE && r.s.share_req == 1);
    app_webshare_status_t st = { .running = 1, .mode = APP_WEBSHARE_AP };
    strcpy(st.code, "123456"), strcpy(st.network, "Writer"), strcpy(st.address, "http://4.3.2.1"), strcpy(st.ip, "4.3.2.1");
    sk_share_update(&r.s, 0, &st);
    rect_t out[SK_MAX_RECTS];
    CHECK(rig_frame(&r, out) == SK_FULL && r.gray_mode == 0);
    CHECK(fb_black(&r) > 0); /* the page drawn after the mode switch cleared the panel */
    sk_share_update(&r.s, 0, &st);
    CHECK(!r.s.want_full); /* the same status: no redraw */
    st.locked = 1;
    sk_share_update(&r.s, 0, &st);
    CHECK(r.s.want_full); /* a change is drawn */
    touch(&r, APP_TOUCH_MIDDLE, APP_GESTURE_TAP);
    CHECK(r.s.share_req == -1 && r.s.mode == SK_GALLERY);
    touch(&r, APP_TOUCH_MIDDLE, APP_GESTURE_TAP);
    touch(&r, APP_TOUCH_MIDDLE, APP_GESTURE_TAP); /* Open */
    CHECK(r.s.mode == SK_DRAW && same_page_and_fb(&r)); /* back to the page */
    CHECK(rig_frame(&r, out) == SK_FULL && same_page_and_fb(&r) && !r.gray_mode);
    share_view_t v;
    share_view(&v, APP_ENONET, &st);
    CHECK(strstr(v.note, "No network") && !v.address[0]); /* a start that failed says so */
    share_view(&v, 0, &st);
    CHECK(strstr(v.note, "wrong codes"));
    rig_end(&r);
}

/* All four positions: the pen lands where the tablet says, the strokes keep their shape */
static void test_orientations(void)
{
    for (int o = 0; o < 4; o++) {
        rig_t r;
        rig_new(&r, o);
        const penmap_t *m = &r.s.map;
        int fx0, fy0, fx1, fy1;
        pm_map(m, 20000, 20000, &fx0, &fy0);
        pm_map(m, 45000, 40000, &fx1, &fy1);
        r.now += 5;
        sk_pen(&r.s, APP_PEN_TIP | APP_PEN_IN_RANGE, 20000, 20000, 30000, r.now);
        for (int i = 1; i <= 8; i++) {
            r.now += 5;
            sk_pen(&r.s, APP_PEN_TIP | APP_PEN_IN_RANGE, (unsigned)(20000 + 25000 * i / 8), (unsigned)(20000 + 20000 * i / 8), 30000, r.now);
        }
        sk_pen(&r.s, 0, 45000, 40000, 0, r.now + 5);
        CHECK(px_black(&r, fx0, fy0) && px_black(&r, fx1, fy1)); /* both ends of the stroke */
        CHECK(px_black(&r, (fx0 + fx1) / 2, (fy0 + fy1) / 2));
        CHECK(same_page_and_fb(&r));
        key(&r, 0);
        CHECK(!px_black(&r, fx0, fy0));
        rig_end(&r);
    }
}

static void test_turn(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    rig_line(&r, 300, 300, 900, 600, 50000, 0);
    uint8_t page[TS * TH];
    memcpy(page, r.s.page.px, sizeof(page));
    rect_t out[SK_MAX_RECTS];
    sk_flush(&r.s, r.now, out);
    app_touchbar_t bar;
    tbfw_layout(APP_ORIENT_BAR_LEFT, &bar);
    sk_set_orient(&r.s, APP_ORIENT_BAR_LEFT, &bar);
    CHECK(page_is(&r, page) && same_page_and_fb(&r)); /* the page is kept, nothing else is left on the glass */
    CHECK(sk_flush(&r.s, r.now + 100, out) == SK_FULL);
    CHECK(r.s.map.sw == TH && r.s.map.sh == TW);
    rig_end(&r);
}

int main(void)
{
    test_legend_and_ink();
    test_pressure_and_size();
    test_eraser();
    test_hover_cursor();
    test_undo_redo_buttons();
    test_clear();
    test_pages_and_autosave();
    test_corrupt_start();
    test_menu_and_share();
    test_orientations();
    test_turn();
    return CHECK_DONE("test_sketch");
}
