/* The page menu, Exit, printing from it, damaged pages, and the walks through every screen with the pen only and with the
   tablet buttons only. SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 */
#include "check.h"
#include <stdio.h>
#include "util.h"
#include "oldsizes.h"

static void tap(rig_t *r, int zone) { sk_touch(&r->s, zone, APP_GESTURE_TAP, r->now += 50); }
static void btn(rig_t *r, int n)
{
    sk_pen_key(&r->s, 1u << (n - 1), 1u << (n - 1), r->now += 50);
    sk_pen_key(&r->s, 0, 1u << (n - 1), r->now);
}
static void make_pages(rig_t *r, int n)
{
    for (int i = 0; i < n; i++) {
        rig_line(r, 100 + i * 40, 200, 900 + i * 30, 300 + i * 20 % 700, 50000, 0);
        r->now += 3000;
        sk_tick(&r->s, r->now);
        if (i + 1 < n) rig_newpage(r);
    }
}
static int read_all(rig_t *r, const char *path, uint8_t *buf, int cap)
{
    void *f = r->fs.fs.open(&r->fs, path, 0);
    if (!f) return -1;
    int n = r->fs.fs.read(&r->fs, f, buf, cap);
    r->fs.fs.close(&r->fs, f);
    return n;
}
static void frames(rig_t *r)
{
    rect_t out[SK_MAX_RECTS];
    while (rig_frame(r, out) != 0) {}
}

/* ---- the pen only ---- */
static void pen_xy(rig_t *r, int x, int y)
{
    rig_pen_user(r, x, y, APP_PEN_IN_RANGE, 0);
    rig_pen_user(r, x, y, APP_PEN_TIP | APP_PEN_IN_RANGE, 20000);
    rig_pen_user(r, x + 2, y + 1, APP_PEN_TIP | APP_PEN_IN_RANGE, 20000);
    rig_pen_user(r, x + 2, y + 1, APP_PEN_IN_RANGE, 0);
    rig_pen_user(r, 5, 5, 0, 0); /* out of range again: nothing is hovered between taps */
}
static void pen_row(rig_t *r, int i) { pen_xy(r, r->s.lg.x + 50, r->s.lg.y0 + i * LIST_PITCH - 25); }
static void pen_ctl(rig_t *r, int c) /* CTL_BACK, CTL_SETUP, or CTL_SEL (the OK of a result screen) */
{
    int x0, y0, x1, y1;
    if (c == CTL_SEL) ui_pill_box(r->s.orient % 2 ? TH : TW, r->s.orient % 2 ? TW : TH, &r->s.bar, APP_TOUCH_MIDDLE, "OK", PILL_NONE, &x0, &y0, &x1, &y1);
    else ui_ctl_box(&r->s.L, c, &x0, &y0, &x1, &y1);
    pen_xy(r, (x0 + x1) / 2, (y0 + y1) / 2);
}
static void pen_tile(rig_t *r, int idx)
{
    int tx, ty;
    gal_tile_xy(&r->s.L, idx % r->s.L.per, &tx, &ty);
    pen_xy(r, tx + r->s.L.tw / 2, ty + r->s.L.th / 2);
}
static void pen_anywhere(rig_t *r) { pen_xy(r, 400, 300); }
/* sk_run_print as the glue does after the "Printing..." screen was shown: once a loop turn until the job is out */
static void run_print(rig_t *r)
{
    CHECK(r->s.mode == SK_BUSY && r->s.print_req);
    frames(r);
    for (int k = 0; k < 100 && r->s.print_req; k++) sk_run_print(&r->s);
    CHECK(!r->s.print_req && !r->s.pjob);
}

/* ---- the menu opens, Back brings the same drawing ---- */
static void test_menu_and_exact_back(void)
{
    static rig_t r;
    static uint8_t keep[TS * TH], fbkeep[TS * TH];
    for (int way = 0; way < 6; way++) {
        rig_new(&r, APP_ORIENT_BAR_BOTTOM);
        make_pages(&r, 2);
        btn(&r, 4); /* the toolbar, a brush, a shade, a size */
        sk_set_brush(&r.s, BR_PENCIL, r.now += 50);
        sk_set_shade(&r.s, 1, r.now += 50);
        sk_set_size(&r.s, 2, r.now += 50);
        rig_line(&r, 300, 800, 1200, 900, 40000, 0); /* ink not yet saved by the autosave */
        rig_pen_user(&r, 5, 5, 0, 0); /* the pen leaves; the notes run out: nothing but page and toolbar is on the screen */
        r.now += 4000;
        r.s.save_at = r.now + 100000; /* the autosave stays out of it: the menu writes the page */
        sk_tick(&r.s, r.now);
        frames(&r);
        memcpy(keep, r.s.page.px, sizeof(keep));
        memcpy(fbkeep, r.fb, sizeof(fbkeep));
        bstyle_t sty = r.s.sty;
        int cur = r.s.cur, tb = r.s.tb_on, n_undo = r.s.hist.st_cur;
        CHECK(tb && r.s.dirty && !r.s.note_on);
        if (way == 0) btn(&r, 7);
        else if (way == 1) btn(&r, 7); /* (8 is the colour now: it does not open the menu) */
        else if (way == 2) tap(&r, APP_TOUCH_MIDDLE);
        else btn(&r, 7);
        CHECK(r.s.mode == SK_PAGEMENU && r.s.act_num == cur);
        frames(&r);
        CHECK(r.s.list_n == 8 && !r.s.dirty); /* the page is written first: Print and Send work on the file */
        char path[48];
        pg_path(path, sizeof(path), cur, ".png");
        CHECK(memfs_find(&r.fs, path) != 0);
        CHECK(!memcmp(r.s.page.px, keep, sizeof(keep))); /* the page is still in memory, untouched */
        if (way == 0 || way == 3) btn(&r, 5);              /* tablet 5 */
        else if (way == 1) btn(&r, 7);                     /* the menu button again */
        else if (way == 2) { tap(&r, APP_TOUCH_LEFT); tap(&r, APP_TOUCH_MIDDLE); } /* the Back row, on top (owner 2026-11-02) */
        else if (way == 4) pen_ctl(&r, CTL_BACK);          /* the Back pill */
        else btn(&r, 7);
        CHECK(r.s.mode == SK_DRAW && r.s.cur == cur);
        CHECK(!memcmp(r.s.page.px, keep, sizeof(keep)) && !memcmp(r.fb, fbkeep, sizeof(fbkeep))); /* pixels as they were, on the screen too */
        CHECK(!memcmp(&r.s.sty, &sty, sizeof(sty)) && r.s.tb_on == tb && r.s.hist.st_cur == n_undo); /* and the tool state */
        sk_undo(&r.s, r.now += 50); /* the undo log survived the menu */
        CHECK(memcmp(r.s.page.px, keep, sizeof(keep)) != 0);
        rig_end(&r);
    }
}

/* A save that fails keeps the drawing on screen: no menu on a page that could not be written */
static void test_menu_needs_a_saved_page(void)
{
    static rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    make_pages(&r, 1);
    rig_line(&r, 300, 800, 1200, 900, 40000, 0);
    r.fs.fail_write = 1;
    btn(&r, 7);
    CHECK(r.s.mode == SK_DRAW && r.s.note_on); /* "Could not save this page" */
    r.fs.fail_write = 0;
    btn(&r, 7);
    CHECK(r.s.mode == SK_PAGEMENU);
    rig_end(&r);
}

/* ---- Exit Sketch ---- */
static void test_exit(void)
{
    static rig_t r;
    static uint8_t file[SK_FILE_MAX];
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    make_pages(&r, 1);
    rig_line(&r, 300, 800, 1200, 900, 40000, 0);
    btn(&r, 7);
    for (int i = 0; i < 7; i++) btn(&r, 2); /* Exit Sketch is the last row */
    CHECK(r.s.act_sel == 7 && !r.s.exit_req);
    btn(&r, 4);
    CHECK(r.s.exit_req && r.s.mode == SK_PAGEMENU); /* the glue leaves app_main, as after a long press */
    CHECK(read_all(&r, "sketch/sk0001.png", file, SK_FILE_MAX) > 0);
    rig_end(&r);
    /* the save fails: one confirm step, never a silent loss */
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    make_pages(&r, 1);
    btn(&r, 7);
    r.s.dirty = 1;
    r.fs.fail_write = 1;
    for (int i = 0; i < 7; i++) btn(&r, 2);
    btn(&r, 4);
    CHECK(!r.s.exit_req && r.s.mode == SK_CONFIRM && r.s.exit_unsaved);
    btn(&r, 5); /* Back: the menu, on Back */
    CHECK(r.s.mode == SK_PAGEMENU && r.s.act_sel == 0 && !r.s.exit_req);
    for (int i = 0; i < 7; i++) btn(&r, 2);
    btn(&r, 4);
    CHECK(r.s.mode == SK_CONFIRM && r.s.act_sel == 0);
    btn(&r, 2), btn(&r, 4); /* Exit without saving */
    CHECK(r.s.exit_req);
    rig_end(&r);
}

/* ---- the gallery from the menu ---- */
static void test_gallery_from_menu(void)
{
    static rig_t r;
    static uint8_t keep[TS * TH], file[SK_FILE_MAX];
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    make_pages(&r, 3);
    memcpy(keep, r.s.page.px, sizeof(keep));
    btn(&r, 7);
    for (int i = 0; i < 6; i++) btn(&r, 2);
    btn(&r, 4);
    CHECK(r.s.mode == SK_GALLERY && r.s.cur == 3 && r.s.sel == 1);
    btn(&r, 5); /* Back from the gallery: the page you came from */
    CHECK(r.s.mode == SK_DRAW && r.s.cur == 3 && !memcmp(r.s.page.px, keep, sizeof(keep)));
    /* the menu again, the gallery, another drawing: it opens at once; the one left is on disk */
    btn(&r, 7);
    for (int i = 0; i < 6; i++) btn(&r, 2);
    btn(&r, 4);
    btn(&r, 2);
    btn(&r, 2);
    CHECK(r.s.sel == 3);
    btn(&r, 4);
    CHECK(r.s.mode == SK_DRAW && r.s.cur == 1);
    int n = read_all(&r, "sketch/sk0003.png", file, SK_FILE_MAX);
    CHECK(n > 0);
    rig_end(&r);
}

/* ---- printing from the menu ---- */
static void test_print_from_the_menu(void)
{
    static rig_t r;
    static uint8_t file[SK_FILE_MAX];
    rect_t out[SK_MAX_RECTS];
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    make_pages(&r, 2);
    r.prn.format = PJ_PCL;
    btn(&r, 7);
    btn(&r, 2), btn(&r, 4); /* Print is one move down: the menu opens on Back */
    CHECK(r.s.mode == SK_CONFIRM && r.pr.posts == 0); /* nothing is sent before the confirm */
    btn(&r, 2), btn(&r, 4); /* the confirm opens on Back too */
    CHECK(rig_frame(&r, out) == SK_FULL); /* "Printing..." is on the glass before the job goes out */
    run_print(&r);
    CHECK(r.s.mode == SK_RESULT && r.pr.posts >= 1 && strstr(r.pr.url, "10.1.2.3") && r.s.pres.pages == 1);
    int n = read_all(&r, "sketch/sk0002.png", file, SK_FILE_MAX);
    CHECK(n > 0);
    fprinter_t direct; /* the same bytes the Print app sends for that file and printer, in the printer's format */
    memset(&direct, 0, sizeof(direct));
    direct.status = 200;
    pj_net_t net = { PJ_PCL, fake_post, &direct };
    pj_result_t res;
    CHECK(pj_print_png(&net, &r.prn, file, (size_t)n, &res) == 0);
    CHECK(direct.posts == r.pr.posts && direct.len[0] == r.pr.len[0] && !memcmp(direct.body[0], r.pr.body[0], direct.len[0]));
    for (int i = 0; i < 4; i++) free(direct.body[i]);
    btn(&r, 5);
    CHECK(r.s.mode == SK_PAGEMENU);
    /* the printer is off: a plain message, nothing crashed */
    r.pr.status = -1;
    btn(&r, 2), btn(&r, 4), btn(&r, 2), btn(&r, 4);
    run_print(&r);
    CHECK(r.s.mode == SK_RESULT && r.s.pres.msg[0] && r.s.pres.jobs == 0 && !r.s.need_setup);
    btn(&r, 5);
    /* a firmware that cannot print */
    r.s.env.net.post = 0;
    btn(&r, 2), btn(&r, 4);
    CHECK(r.s.mode == SK_RESULT && strstr(r.s.pres.msg, "not available"));
    btn(&r, 5);
    r.s.env.net.post = fake_post;
    /* a page file that is gone */
    r.fs.fs.remove(&r.fs, "sketch/sk0002.png");
    CHECK(sk_print_page(&r.s, 2) < 0 && strstr(r.s.pres.msg, "could not be read"));
    /* an empty page has nothing to print */
    btn(&r, 5);
    rig_newpage(&r);
    btn(&r, 7);
    r.pr.posts = 0;
    btn(&r, 2), btn(&r, 4);
    CHECK(r.s.mode == SK_RESULT && strstr(r.s.pres.msg, "empty") && r.pr.posts == 0);
    rig_end(&r);
}

/* No printer file: no job, the words from the coordinator, a "Set up printer" button the pen and button 6 reach */
/* The job is made a few rows a loop turn (owner 2026-11-03: Print took 38 s with the loop stopped): the loop does not
   sleep, input between the turns changes nothing, the post comes at the last turn; leaving half way sends nothing */
static void test_print_in_steps(void)
{
    static rig_t r;
    for (int leave = 0; leave < 2; leave++) {
        rig_new(&r, APP_ORIENT_BAR_BOTTOM);
        make_pages(&r, 1);
        btn(&r, 7);
        btn(&r, 2), btn(&r, 4), btn(&r, 2), btn(&r, 4);
        frames(&r);
        int turns = 0;
        while (r.s.print_req && turns < 100) {
            sk_run_print(&r.s);
            turns++;
            if (!r.s.print_req) break;
            CHECK(r.s.mode == SK_BUSY && r.pr.posts == 0 && sk_next_due(&r.s, r.now) == 0);
            btn(&r, 5), btn(&r, 4), tap(&r, APP_TOUCH_MIDDLE), tap(&r, APP_TOUCH_LEFT); /* ignored while printing */
            rig_pen_user(&r, 400, 300, APP_PEN_TIP | APP_PEN_IN_RANGE, 20000), rig_pen_user(&r, 400, 300, 0, 0);
            r.now += 2000;
            sk_tick(&r.s, r.now); /* no idle clean-up flash of the busy screen */
            CHECK(!r.s.want_full && box_empty(&r.s.flash));
            if (leave && turns == 4) break;
        }
        if (leave) {
            CHECK(r.s.pjob && r.pr.posts == 0);
            rig_end(&r); /* the job is freed (ASan: no leak), nothing was posted */
            CHECK(r.pr.posts == 0);
            continue;
        }
        CHECK(turns > 10 && r.pr.posts == 1 && r.s.mode == SK_RESULT && r.s.pres.pages == 1);
        rig_end(&r);
    }
}

static void test_no_printer_offers_setup(void)
{
    static rig_t r;
    for (int way = 0; way < 2; way++) {
        rig_new(&r, APP_ORIENT_BAR_BOTTOM);
        make_pages(&r, 1);
        r.pr_rc = PJ_NO_PRINTER;
        btn(&r, 7);
        btn(&r, 2), btn(&r, 4);
        CHECK(r.s.mode == SK_RESULT && r.s.need_setup && !strcmp(r.s.pres.msg, "No printer yet: set one up in Print") && r.pr.posts == 0);
        rect_t out[SK_MAX_RECTS];
        CHECK(rig_frame(&r, out) == SK_FULL);
        CHECK(!r.s.setup_req);
        if (way == 0) pen_ctl(&r, CTL_SETUP);
        else btn(&r, 6);
        CHECK(r.s.setup_req && r.pr.posts == 0); /* the glue launches Print's setup and returns */
        r.s.setup_req = 0;
        pen_ctl(&r, CTL_BACK);
        CHECK(r.s.mode == SK_PAGEMENU);
        r.pr_rc = APP_ENOTSUP; /* an old firmware: a message, no setup button */
        btn(&r, 2), btn(&r, 4);
        CHECK(r.s.mode == SK_RESULT && !r.s.need_setup && r.pr.posts == 0);
        r.pr_rc = APP_EIO;
        btn(&r, 5);
        btn(&r, 2), btn(&r, 4);
        CHECK(r.s.mode == SK_RESULT && !r.s.need_setup && r.pr.posts == 0);
        rig_end(&r);
    }
}

/* ---- damaged pages ---- */
static void test_damaged_page(void)
{
    static rig_t r;
    static uint8_t junk[300], file[SK_FILE_MAX], before[SK_FILE_MAX];
    for (int i = 0; i < 300; i++) junk[i] = (uint8_t)(i * 7 + 3);
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    make_pages(&r, 3);
    memfs_put(&r.fs, "sketch/sk0002.png", junk, sizeof(junk));
    gal_invalidate(&r.s.cache, 2); /* the page menu cached its thumbnail; the file went bad behind the app's back */
    btn(&r, 7);
    for (int i = 0; i < 6; i++) btn(&r, 2);
    btn(&r, 4);
    CHECK(r.s.mode == SK_GALLERY);
    rig_gal_fill(&r);
    rect_t out[SK_MAX_RECTS];
    rig_frame(&r, out);
    CHECK(r.s.dmg_n == 1 && r.s.dmg[0] == 2); /* found while the tiles were drawn */
    { /* and drawn as a "Damaged" tile: text inside the frame where a drawing's thumbnail would be (a blank tile has none) */
        gfx_t g;
        int x, y, black = 0;
        gfx_init(&g, r.fb, TW, TH, TS, r.s.orient);
        gal_tile_xy(&r.s.L, 2, &x, &y);
        for (int dy = 4; dy < r.s.L.th - 4; dy++)
            for (int dx = 4; dx < r.s.L.tw - 4; dx++) black += !gfx_get(&g, x + dx, y + dy);
        CHECK(black > 1200); /* 2100 with the word, 540 without */
    }
    btn(&r, 2); /* tile 2 is page 2 (3 is newest) */
    CHECK(r.s.sel == 2);
    btn(&r, 4);
    CHECK(r.s.mode == SK_ACTIONS && r.s.act_num == 2 && r.s.list_n == 2); /* never opened for drawing */
    CHECK(r.s.act_sel == 0); /* the list opens on Back */
    btn(&r, 2), btn(&r, 4);
    CHECK(r.s.mode == SK_SHARE && r.s.share_req == 1); /* its action: Send to phone */
    btn(&r, 1);
    CHECK(r.s.mode == SK_ACTIONS && r.s.act_sel == 0);
    btn(&r, 5);
    CHECK(r.s.mode == SK_GALLERY);
    btn(&r, 5);
    CHECK(r.s.mode == SK_DRAW && r.s.cur == 3);
    int n = read_all(&r, "sketch/sk0002.png", file, SK_FILE_MAX);
    CHECK(n == (int)sizeof(junk) && !memcmp(file, junk, sizeof(junk))); /* never deleted, never changed */
    /* the app starts on the damaged page (its setting): it draws on another page and leaves the file alone */
    rig_cfg(&r, "cur", "2");
    rig_start(&r, APP_ORIENT_BAR_BOTTOM);
    CHECK(r.s.cur != 2 && r.s.cur == 3);
    rig_line(&r, 300, 800, 1200, 900, 40000, 0);
    r.now += 3000;
    sk_tick(&r.s, r.now);
    memcpy(before, junk, sizeof(junk));
    n = read_all(&r, "sketch/sk0002.png", file, SK_FILE_MAX);
    CHECK(n == (int)sizeof(junk) && !memcmp(file, before, sizeof(junk)) && memfs_find(&r.fs, "sketch/sk0002-bad.png") == 0);
    /* every page damaged: a new page, numbered past them */
    memfs_put(&r.fs, "sketch/sk0003.png", junk, sizeof(junk));
    memfs_put(&r.fs, "sketch/sk0001.png", junk, sizeof(junk));
    rig_start(&r, APP_ORIENT_BAR_BOTTOM);
    CHECK(r.s.cur == 4 && sk_is_blank(&r.s.page));
    rig_end(&r);
}

/* ---- the walks: every screen and back, with the pen only, and with the tablet buttons only ---- */
static void setup_walk(rig_t *r, int orient)
{
    static uint8_t junk[64];
    rig_new(r, orient);
    make_pages(r, 2);
    memfs_put(&r->fs, "sketch/sk0003.png", junk, sizeof(junk)); /* a damaged page, newest */
    rig_pen_user(r, 5, 5, 0, 0);
}

static void test_walk_with_the_pen_only(void)
{
    static rig_t r;
    for (int o = 0; o < 4; o += 3) {
        setup_walk(&r, o);
        btn(&r, 4);
        CHECK(r.s.mode == SK_DRAW && r.s.tb_on);
        btn(&r, 4);
        CHECK(!r.s.tb_on);
        CHECK(!r.s.dirty || r.s.stroke == 0); /* no pill tap inked */
        btn(&r, 7);
        CHECK(r.s.mode == SK_PAGEMENU);
        rig_frame(&r, (rect_t[SK_MAX_RECTS]){ 0 });
        pen_row(&r, 0); /* Back, the top row (owner 2026-11-02) */
        CHECK(r.s.mode == SK_DRAW);
        btn(&r, 7);
        pen_ctl(&r, CTL_BACK);
        CHECK(r.s.mode == SK_DRAW);
        btn(&r, 7);
        pen_row(&r, 1); /* Print */
        CHECK(r.s.mode == SK_CONFIRM);
        pen_ctl(&r, CTL_BACK);
        CHECK(r.s.mode == SK_PAGEMENU);
        pen_row(&r, 1);
        pen_row(&r, 1); /* the Print row of the confirm list */
        CHECK(r.s.mode == SK_BUSY);
        run_print(&r);
        CHECK(r.s.mode == SK_RESULT);
        pen_ctl(&r, CTL_SEL); /* OK */
        CHECK(r.s.mode == SK_PAGEMENU);
        r.pr_rc = PJ_NO_PRINTER;
        pen_row(&r, 1);
        CHECK(r.s.mode == SK_RESULT && r.s.need_setup);
        pen_ctl(&r, CTL_SETUP);
        CHECK(r.s.setup_req);
        r.s.setup_req = 0;
        pen_ctl(&r, CTL_BACK);
        CHECK(r.s.mode == SK_PAGEMENU);
        r.pr_rc = 0;
        pen_row(&r, 2); /* Send to phone */
        CHECK(r.s.mode == SK_SHARE && r.s.share_req == 1);
        pen_anywhere(&r);
        CHECK(r.s.mode == SK_PAGEMENU);
        pen_row(&r, 3); /* Settings: its own screen, not the phone page */
        CHECK(r.s.mode == SK_SETTINGS);
        pen_row(&r, 0); /* Back */
        CHECK(r.s.mode == SK_PAGEMENU && r.s.act_sel == 0);
        pen_row(&r, 6); /* Gallery */
        CHECK(r.s.mode == SK_GALLERY);
        rig_frame(&r, (rect_t[SK_MAX_RECTS]){ 0 });
        pen_tile(&r, 1); /* the damaged one */
        CHECK(r.s.mode == SK_ACTIONS);
        pen_row(&r, 1);
        CHECK(r.s.mode == SK_SHARE);
        pen_anywhere(&r);
        CHECK(r.s.mode == SK_ACTIONS);
        pen_ctl(&r, CTL_BACK);
        CHECK(r.s.mode == SK_GALLERY);
        pen_ctl(&r, CTL_BACK);
        CHECK(r.s.mode == SK_DRAW);
        btn(&r, 7);
        pen_row(&r, 6);
        pen_tile(&r, 3); /* page 1 */
        CHECK(r.s.mode == SK_DRAW && r.s.cur == 1);
        btn(&r, 7);
        pen_row(&r, 5); /* New page */
        CHECK(r.s.mode == SK_DRAW && r.s.cur == 4);
        btn(&r, 7);
        pen_row(&r, 7); /* Exit Sketch */
        CHECK(r.s.exit_req);
        rig_end(&r);
    }
}

static void row_button(rig_t *r, int row) /* the selection to a list row with buttons 1 and 2 */
{
    for (int i = 0; i < 8; i++) btn(r, 1);
    for (int i = 0; i < row; i++) btn(r, 2);
    CHECK(r->s.act_sel == row);
}

static void test_walk_with_the_tablet_buttons_only(void)
{
    static rig_t r;
    for (int o = 0; o < 4; o += 3) {
        setup_walk(&r, o);
        btn(&r, 4);
        CHECK(r.s.tb_on);
        btn(&r, 4);
        btn(&r, 7);
        CHECK(r.s.mode == SK_PAGEMENU);
        btn(&r, 5);
        CHECK(r.s.mode == SK_DRAW);
        btn(&r, 7);
        btn(&r, 7); /* the menu button again */
        CHECK(r.s.mode == SK_DRAW);
        btn(&r, 7);
        btn(&r, 8); /* 8 does nothing in the page menu: only 7 closes it */
        CHECK(r.s.mode == SK_PAGEMENU);
        btn(&r, 7);
        CHECK(r.s.mode == SK_DRAW);
        btn(&r, 7);
        row_button(&r, 1);
        btn(&r, 4);
        CHECK(r.s.mode == SK_CONFIRM);
        btn(&r, 5);
        CHECK(r.s.mode == SK_PAGEMENU);
        btn(&r, 2), btn(&r, 4); /* Back from the confirm left the menu on Back: Print is one move down */
        btn(&r, 2), btn(&r, 4); /* the confirm opens on Back */
        run_print(&r);
        CHECK(r.s.mode == SK_RESULT);
        btn(&r, 5);
        CHECK(r.s.mode == SK_PAGEMENU);
        r.pr_rc = PJ_NO_PRINTER;
        btn(&r, 2), btn(&r, 4);
        CHECK(r.s.mode == SK_RESULT && r.s.need_setup);
        btn(&r, 6);
        CHECK(r.s.setup_req);
        r.s.setup_req = 0;
        btn(&r, 5);
        r.pr_rc = 0;
        row_button(&r, 2);
        btn(&r, 4);
        CHECK(r.s.mode == SK_SHARE);
        btn(&r, 1);
        CHECK(r.s.mode == SK_PAGEMENU);
        row_button(&r, 3);
        btn(&r, 4);
        CHECK(r.s.mode == SK_SETTINGS);
        btn(&r, 5); /* tablet 5 returns */
        CHECK(r.s.mode == SK_PAGEMENU);
        row_button(&r, 6);
        btn(&r, 4);
        CHECK(r.s.mode == SK_GALLERY);
        rig_frame(&r, (rect_t[SK_MAX_RECTS]){ 0 });
        btn(&r, 1); /* from the page on screen (tile 2) to the damaged tile 1 */
        btn(&r, 4);
        CHECK(r.s.mode == SK_ACTIONS && r.s.act_sel == 0);
        btn(&r, 2), btn(&r, 4);
        CHECK(r.s.mode == SK_SHARE);
        btn(&r, 1);
        btn(&r, 5);
        CHECK(r.s.mode == SK_GALLERY);
        btn(&r, 5);
        CHECK(r.s.mode == SK_DRAW);
        btn(&r, 7);
        row_button(&r, 7);
        btn(&r, 4);
        CHECK(r.s.exit_req);
        rig_end(&r);
    }
}

static int again_title(const rig_t *r) { return sk_print_again(&r->s); }

/* ---- closing the menu never prints (2026-11-03: a tap on Select meant to close the menu printed the page twice) ---- */
static void test_close_never_prints(void)
{
    static rig_t r;
    for (int o = 0; o < 4; o += 3) {
        rig_new(&r, o);
        make_pages(&r, 1);
        tap(&r, APP_TOUCH_MIDDLE); /* the menu opens */
        CHECK(r.s.mode == SK_PAGEMENU && r.s.act_sel == 0);
        tap(&r, APP_TOUCH_MIDDLE); /* the same tap closes it */
        CHECK(r.s.mode == SK_DRAW && !r.s.print_req && r.pr.posts == 0);
        tap(&r, APP_TOUCH_MIDDLE);
        tap(&r, APP_TOUCH_RIGHT); /* Print */
        tap(&r, APP_TOUCH_MIDDLE);
        CHECK(r.s.mode == SK_CONFIRM && r.s.act_sel == 0 && !r.s.print_req); /* the confirm opens on Back */
        for (int k = 0; k < 3; k++) {
            tap(&r, APP_TOUCH_MIDDLE); /* Select on Back closes the confirm ... */
            CHECK(r.s.mode == SK_PAGEMENU && r.s.act_sel == 0 && !r.s.print_req && r.pr.posts == 0);
            tap(&r, APP_TOUCH_RIGHT);
            tap(&r, APP_TOUCH_MIDDLE); /* ... and from the menu Print only asks again */
            CHECK(r.s.mode == SK_CONFIRM && r.s.act_sel == 0 && !r.s.print_req && r.pr.posts == 0);
        }
        CHECK(r.s.mode == SK_CONFIRM && r.pr.posts == 0);
        tap(&r, APP_TOUCH_RIGHT);
        tap(&r, APP_TOUCH_MIDDLE); /* only a move to Print, then Select, prints */
        CHECK(r.s.mode == SK_BUSY && r.s.print_req);
        run_print(&r);
        CHECK(r.s.mode == SK_RESULT && r.s.pres.pages == 1);
        tap(&r, APP_TOUCH_MIDDLE); /* OK: back to the page menu on Back, never on Print */
        CHECK(r.s.mode == SK_PAGEMENU && r.s.act_sel == 0 && !r.s.print_req);
        tap(&r, APP_TOUCH_MIDDLE);
        CHECK(r.s.mode == SK_DRAW && r.pr.posts == 1); /* one print, not two */
        rig_end(&r);
    }
}

/* ---- the same page printed again unchanged says so; a changed page does not ---- */
static void to_confirm(rig_t *r)
{
    btn(r, 7);
    btn(r, 2);
    btn(r, 4);
    CHECK(r->s.mode == SK_CONFIRM && r->s.act_sel == 0);
}
static void to_confirm_or_result(rig_t *r)
{
    btn(r, 7);
    btn(r, 2);
    btn(r, 4);
    CHECK(r->s.mode == SK_RESULT && !r->s.print_req);
}
static void print_now(rig_t *r)
{
    btn(r, 2);
    btn(r, 4);
    run_print(r);
    CHECK(r->s.mode == SK_RESULT && r->s.pres.pages == 1);
    btn(r, 5); /* result to the page menu */
    btn(r, 5); /* and the drawing */
    CHECK(r->s.mode == SK_DRAW);
}
static void test_printed_already(void)
{
    static rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    make_pages(&r, 1);
    to_confirm(&r);
    CHECK(!again_title(&r)); /* never printed */
    print_now(&r);
    to_confirm(&r);
    CHECK(again_title(&r)); /* the page is as printed */
    btn(&r, 5), btn(&r, 5);
    rig_line(&r, 300, 800, 1200, 900, 40000, 0); /* a change */
    to_confirm(&r);
    CHECK(!again_title(&r));
    print_now(&r);
    to_confirm(&r);
    CHECK(again_title(&r));
    btn(&r, 5), btn(&r, 5);
    sk_undo(&r.s, r.now += 50); /* undo is a change too */
    to_confirm(&r);
    CHECK(!again_title(&r));
    btn(&r, 5), btn(&r, 5);
    sk_redo(&r.s, r.now += 50);
    to_confirm(&r);
    CHECK(!again_title(&r)); /* redo is a change too */
    btn(&r, 5), btn(&r, 5);
    sk_clear(&r.s, r.now += 50);
    to_confirm_or_result(&r); /* a cleared page is blank: nothing to print, the confirm is never reached */
    rig_end(&r);
}

/* ---- the confirm, Printing and result screens show the page being printed ---- */
static int thumb_blitted(const rig_t *r) { return r->s.blit_n >= 1 && r->s.blit[0].num == r->s.act_num; }
static void test_print_screens_show_the_page(void)
{
    static rig_t r;
    for (int o = 0; o < 4; o += 3) {
        rig_new(&r, o);
        make_pages(&r, 1);
        to_confirm(&r);
        CHECK(thumb_blitted(&r));
        btn(&r, 2);
        btn(&r, 4);
        CHECK(r.s.mode == SK_BUSY && thumb_blitted(&r));
        run_print(&r);
        CHECK(r.s.mode == SK_RESULT && thumb_blitted(&r));
        int x0, y0, x1, y1; /* the preview beside the text, inside the area and clear of the pills and Back */
        CHECK(r.s.blit[0].x >= r.s.L.ax0 && r.s.blit[0].x + r.s.L.tw <= r.s.L.ax1);
        ui_ctl_box(&r.s.L, CTL_BACK, &x0, &y0, &x1, &y1);
        CHECK(r.s.blit[0].x + r.s.L.tw < x0 || r.s.blit[0].y + r.s.L.th < y0 || r.s.blit[0].y > y1);
        rig_end(&r);
    }
}

/* ---- every list opens with Back selected, and so does the page menu coming back from any screen (owner 2026-11-03) ---- */
static void test_every_list_opens_on_back(void)
{
    static rig_t r;
    static const uint8_t junk[64] = { 1, 2, 3 };
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    make_pages(&r, 2);
    memfs_put(&r.fs, "sketch/sk0001.png", junk, sizeof(junk));
    btn(&r, 7);
    CHECK(r.s.mode == SK_PAGEMENU && r.s.act_sel == 0);
    btn(&r, 2), btn(&r, 2), btn(&r, 2), btn(&r, 4); /* Settings */
    CHECK(r.s.mode == SK_SETTINGS && r.s.act_sel == 0);
    btn(&r, 5);
    CHECK(r.s.mode == SK_PAGEMENU && r.s.act_sel == 0);
    btn(&r, 2), btn(&r, 2), btn(&r, 4); /* Send to phone, and back from the phone page */
    CHECK(r.s.mode == SK_SHARE);
    btn(&r, 1);
    CHECK(r.s.mode == SK_PAGEMENU && r.s.act_sel == 0);
    btn(&r, 2), btn(&r, 4); /* Print: the confirm, and back from it */
    CHECK(r.s.mode == SK_CONFIRM && r.s.act_sel == 0);
    btn(&r, 5);
    CHECK(r.s.mode == SK_PAGEMENU && r.s.act_sel == 0);
    btn(&r, 2), btn(&r, 4), btn(&r, 2), btn(&r, 4); /* print, the result, back from it */
    run_print(&r);
    btn(&r, 5);
    CHECK(r.s.mode == SK_PAGEMENU && r.s.act_sel == 0);
    for (int i = 0; i < 6; i++) btn(&r, 2);
    btn(&r, 4); /* the gallery: the damaged page's list */
    CHECK(r.s.mode == SK_GALLERY);
    rig_frame(&r, (rect_t[SK_MAX_RECTS]){ 0 });
    while (r.s.sel < r.s.pg.n) btn(&r, 2);
    btn(&r, 4);
    CHECK(r.s.mode == SK_ACTIONS && r.s.act_sel == 0);
    rig_end(&r);
    rig_new(&r, APP_ORIENT_BAR_BOTTOM); /* the exit that cannot save */
    make_pages(&r, 1);
    btn(&r, 7);
    r.s.dirty = 1;
    r.fs.fail_write = 1;
    for (int i = 0; i < 7; i++) btn(&r, 2);
    btn(&r, 4);
    CHECK(r.s.mode == SK_CONFIRM && r.s.exit_unsaved && r.s.act_sel == 0);
    rig_end(&r);
}

int main(void)
{
    test_menu_and_exact_back();
    test_menu_needs_a_saved_page();
    test_exit();
    test_gallery_from_menu();
    test_print_from_the_menu();
    test_print_in_steps();
    test_no_printer_offers_setup();
    test_damaged_page();
    test_walk_with_the_pen_only();
    test_walk_with_the_tablet_buttons_only();
    test_close_never_prints();
    test_every_list_opens_on_back();
    test_printed_already();
    test_print_screens_show_the_page();
    return CHECK_DONE("test_pagemenu");
}
