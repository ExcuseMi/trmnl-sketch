/* At most one blank page exists (owner 2026-11-01): the one on screen. Blank pages are never on disk, a blank file is
   removed at the start and when its page is left, a file that does not decode is never touched. SPDX-License-Identifier: MIT */
#include "check.h"
#include <stdio.h>
#include "util.h"

static void tap(rig_t *r, int zone)
{
    r->now += 50;
    sk_touch(&r->s, zone, APP_GESTURE_TAP, r->now);
}
static void btn(rig_t *r, int n)
{
    sk_pen_key(&r->s, 1u << (n - 1), 1u << (n - 1), r->now += 50);
    sk_pen_key(&r->s, 0, 1u << (n - 1), r->now);
}
static void inked(rig_t *r, int i)
{
    rig_line(r, 100 + i * 40, 200, 900 + i * 30, 300 + i * 20, 50000, 0);
    r->now += 3000;
    sk_tick(&r->s, r->now);
}

/* blank page files that decode, on disk now */
static int disk_blank(rig_t *r)
{
    static pages_t p;
    pg_init(&p, &r->fs.fs);
    pg_scan(&p);
    canvas_t c = { malloc((size_t)TS * TH), TW, TH, TS };
    int n = 0;
    for (int i = 0; i < p.n; i++)
        if (pg_load(&p, p.nums[i], &c) == PG_OK) {
            canvas_t d = c;
            n += sk_is_blank(&d);
        }
    free(c.px);
    return n;
}
static int disk_pages(rig_t *r)
{
    static pages_t p;
    pg_init(&p, &r->fs.fs);
    pg_scan(&p);
    return p.n;
}
/* every blank page there is: on disk, plus the one on screen when it is not on disk */
static int blanks(rig_t *r) { return disk_blank(r) + (sk_is_blank(&r->s.page) && pg_index(&r->s.pg, r->s.cur) < 0); }

/* gallery -> Open the tile of page `num` */
static void gallery_open(rig_t *r, int num)
{
    tap(r, APP_TOUCH_MIDDLE); /* the gallery */
    CHECK(r->s.mode == SK_GALLERY);
    int want = 0;
    for (int i = 0; i < r->s.pg.n; i++)
        if (r->s.pg.nums[i] == num) want = r->s.pg.n - i; /* newest first, the new-page tile is 0 */
    while (r->s.sel < want) tap(r, APP_TOUCH_RIGHT);
    while (r->s.sel > want) tap(r, APP_TOUCH_LEFT);
    tap(r, APP_TOUCH_MIDDLE); /* the drawing's list */
    CHECK(r->s.mode == SK_ACTIONS);
    tap(r, APP_TOUCH_MIDDLE); /* Open */
    CHECK(r->s.mode == SK_DRAW && r->s.cur == num);
}

static void test_new_page_leaving(void)
{
    static rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    CHECK(blanks(&r) == 1 && disk_pages(&r) == 0); /* the first page: the one blank page, not on disk */
    inked(&r, 0);
    btn(&r, 6);
    inked(&r, 1);
    CHECK(disk_pages(&r) == 2 && blanks(&r) == 0);
    for (int k = 0; k < 4; k++) { /* new page, leave it blank through the gallery to an old drawing, again */
        btn(&r, 6);
        CHECK(r.s.cur == 3 && sk_is_blank(&r.s.page) && blanks(&r) == 1);
        gallery_open(&r, 1);
        CHECK(disk_pages(&r) == 2 && blanks(&r) == 0 && !memfs_find(&r.fs, "sketch/sk0003.png"));
    }
    /* the New page tile from a drawing with ink, twice: the second one opens the same blank page */
    tap(&r, APP_TOUCH_MIDDLE);
    tap(&r, APP_TOUCH_LEFT);
    while (r.s.sel > 0) tap(&r, APP_TOUCH_LEFT);
    tap(&r, APP_TOUCH_MIDDLE);
    CHECK(r.s.mode == SK_DRAW && r.s.cur == 3 && blanks(&r) == 1);
    tap(&r, APP_TOUCH_MIDDLE);
    while (r.s.sel > 0) tap(&r, APP_TOUCH_LEFT);
    tap(&r, APP_TOUCH_MIDDLE);
    CHECK(r.s.mode == SK_DRAW && r.s.cur == 3 && blanks(&r) == 1 && disk_pages(&r) == 2);
    btn(&r, 6); /* the button on the blank page: the same */
    CHECK(r.s.cur == 3 && blanks(&r) == 1);
    rig_end(&r);
}

static void test_clear_then_leave(void)
{
    static rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    inked(&r, 0);
    btn(&r, 6);
    inked(&r, 1);
    btn(&r, 5); /* clear page 2 */
    r.now += 3000;
    sk_tick(&r.s, r.now);
    CHECK(sk_is_blank(&r.s.page));
    tap(&r, APP_TOUCH_MIDDLE); /* leave it: to the gallery */
    CHECK(!memfs_find(&r.fs, "sketch/sk0002.png") && disk_pages(&r) == 1);
    CHECK(memfs_find(&r.fs, "sketch/sk0002-cleared.png")); /* what was cleared stays for undo */
    rig_end(&r);
}

static void plant(rig_t *r, int num, int ink)
{
    static pages_t p;
    pg_init(&p, &r->fs.fs);
    canvas_t c = { malloc((size_t)TS * TH), TW, TH, TS };
    cv_fill(&c, 1);
    if (ink) cv_disc(&c, 300, 300, 40, 0, 0);
    CHECK(pg_save(&p, num, &c) == PG_OK);
    free(c.px);
}

static void test_start_cleanup(void)
{
    static rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    plant(&r, 1, 1);
    plant(&r, 2, 0); /* blank files from an older build */
    plant(&r, 3, 1);
    plant(&r, 4, 0);
    const uint8_t junk[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9 }; /* does not decode */
    memfs_put(&r.fs, "sketch/sk0005.png", junk, sizeof(junk));
    snprintf(r.cfg_cur, sizeof(r.cfg_cur), "4"); /* the saved current page is one of the blank ones */
    rig_start(&r, APP_ORIENT_BAR_BOTTOM);
    CHECK(!memfs_find(&r.fs, "sketch/sk0002.png") && !memfs_find(&r.fs, "sketch/sk0004.png")); /* blank, decoded: removed */
    CHECK(memfs_find(&r.fs, "sketch/sk0001.png") && memfs_find(&r.fs, "sketch/sk0003.png")); /* inked: kept */
    memfile_t *j = memfs_find(&r.fs, "sketch/sk0005.png");
    CHECK(j && j->n == sizeof(junk) && !memcmp(j->data, junk, sizeof(junk))); /* undecodable: kept untouched */
    CHECK(disk_blank(&r) == 0 && r.s.pg.n == 3);
    /* the gallery lists the three, none of them blank */
    tap(&r, APP_TOUCH_MIDDLE);
    CHECK(r.s.mode == SK_GALLERY && r.s.pg.n == 3);
    rig_end(&r);
}

/* a blank page on screen at power off is not written: nothing but the first ink makes a file */
static void test_blank_not_written(void)
{
    static rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    inked(&r, 0);
    btn(&r, 6);
    sk_save_now(&r.s);
    CHECK(disk_pages(&r) == 1 && blanks(&r) == 1);
    rig_start(&r, APP_ORIENT_BAR_BOTTOM);
    CHECK(r.s.cur == 1 && !sk_is_blank(&r.s.page) && blanks(&r) == 0); /* back on the drawing with ink */
    rig_end(&r);
}

int main(void)
{
    test_new_page_leaving();
    test_clear_then_leave();
    test_start_cleanup();
    test_blank_not_written();
    return CHECK_DONE("test_blank");
}
