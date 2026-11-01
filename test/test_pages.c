/* The page files: temp + rename saves, a lost power or full disk keeps the old file, a damaged or missing file is an
   empty page and nothing is deleted, the page list. SPDX-License-Identifier: MIT */
#include "check.h"
#include <stdlib.h>
#include <string.h>
#include "canvas.h"
#include "memfs.h"
#include "pages.h"
#include "sk_cfg.h"

#define W 200
#define H 100
static uint8_t px[25 * H], px2[25 * H];
static canvas_t cv = { px, W, H, 25 }, cv2 = { px2, W, H, 25 };

static void ink(canvas_t *c, int x)
{
    cv_fill(c, 1);
    cv_seg(c, x, 10, 5, x + 60, 80, 5, 0, 0);
}
static int has(memfs_t *m, const char *name) { return memfs_find(m, name) != 0; }

static void test_save_load(void)
{
    memfs_t m;
    memfs_init(&m);
    pages_t p;
    pg_init(&p, &m.fs);
    pg_scan(&p);
    ink(&cv, 20);
    CHECK(pg_save(&p, 1, &cv) == PG_OK);
    CHECK(has(&m, "sketch/sk0001.png") && !has(&m, "sketch/sk0001.tmp")); /* the temporary file is gone */
    CHECK(m.removes == 0 && m.renames_ok == 1); /* written aside, then renamed over */
    CHECK(pg_load(&p, 1, &cv2) == PG_OK && !memcmp(px, px2, sizeof(px)));
    ink(&cv, 70); /* a second save replaces the first whole */
    CHECK(pg_save(&p, 1, &cv) == PG_OK);
    CHECK(pg_load(&p, 1, &cv2) == PG_OK && !memcmp(px, px2, sizeof(px)));
    CHECK(m.removes == 0);
    memfs_free(&m);
}

static void test_power_loss(void)
{
    memfs_t m;
    memfs_init(&m);
    pages_t p;
    pg_init(&p, &m.fs);
    ink(&cv, 20);
    CHECK(pg_save(&p, 1, &cv) == PG_OK);
    ink(&cv2, 70);
    m.fail_rename = 1; /* the power went between the write and the rename */
    CHECK(pg_save(&p, 1, &cv2) == PG_IO);
    m.fail_rename = 0;
    canvas_t chk = { calloc(1, sizeof(px)), W, H, 25 };
    CHECK(pg_load(&p, 1, &chk) == PG_OK && !memcmp(px, chk.px, sizeof(px))); /* the old page, whole */
    m.fail_write = 1; /* disk full */
    CHECK(pg_save(&p, 1, &cv2) == PG_IO);
    m.fail_write = 0;
    CHECK(pg_load(&p, 1, &chk) == PG_OK && !memcmp(px, chk.px, sizeof(px)));
    m.fail_open_write = 1; /* no file can be made */
    CHECK(pg_save(&p, 1, &cv2) == PG_IO);
    m.fail_open_write = 0;
    CHECK(pg_load(&p, 1, &chk) == PG_OK && !memcmp(px, chk.px, sizeof(px)));
    CHECK(pg_save(&p, 1, &cv2) == PG_OK); /* and it works again after */
    free(chk.px);
    memfs_free(&m);
}

static void test_missing_and_corrupt(void)
{
    memfs_t m;
    memfs_init(&m);
    pages_t p;
    pg_init(&p, &m.fs);
    ink(&cv, 20);
    CHECK(pg_load(&p, 5, &cv) == PG_MISSING);
    uint8_t white[sizeof(px)];
    memset(white, 0xff, sizeof(white));
    CHECK(!memcmp(px, white, sizeof(px))); /* an empty page */
    /* a damaged file */
    ink(&cv, 20);
    pg_save(&p, 2, &cv);
    memfile_t *f = memfs_find(&m, "sketch/sk0002.png");
    f->data[f->n / 2] ^= 0xff;
    uint8_t before[4096];
    size_t bn = f->n < sizeof(before) ? f->n : sizeof(before);
    memcpy(before, f->data, bn);
    ink(&cv2, 50);
    CHECK(pg_load(&p, 2, &cv2) == PG_BAD);
    CHECK(!memcmp(px2, white, sizeof(px2))); /* loads as an empty page */
    CHECK(m.removes == 0 && has(&m, "sketch/sk0002.png") && !memcmp(f->data, before, bn)); /* nothing deleted or changed */
    /* not a PNG at all, and an empty file */
    memfs_put(&m, "sketch/sk0003.png", "hello, this is not a picture", 28);
    CHECK(pg_load(&p, 3, &cv2) == PG_BAD);
    memfs_put(&m, "sketch/sk0004.png", "", 0);
    CHECK(pg_load(&p, 4, &cv2) == PG_BAD);
    /* a file bigger than any page: refused, not read whole */
    uint8_t *big = calloc(1, SK_FILE_MAX + 100);
    memfs_put(&m, "sketch/sk0006.png", big, SK_FILE_MAX + 100);
    CHECK(pg_load(&p, 6, &cv2) == PG_BAD);
    free(big);
    /* the unreadable page is moved aside before a save can replace it */
    CHECK(pg_keep_bad(&p, 2) == 0);
    CHECK(!has(&m, "sketch/sk0002.png") && has(&m, "sketch/sk0002-bad.png"));
    CHECK(m.removes == 0);
    memfs_free(&m);
}

static void test_list(void)
{
    memfs_t m;
    memfs_init(&m);
    pages_t p;
    pg_init(&p, &m.fs);
    pg_scan(&p);
    CHECK(p.n == 0 && pg_next_num(&p) == 1);
    const char *names[] = { "sketch/sk0010.png", "sketch/sk0002.png", "sketch/sk0003.tmp", "sketch/sk0004-bad.png", "sketch/sk0005-cleared.png",
        "sketch/photo.png", "sketch/sk00x1.png", "sketch/sk0000.png", "sketch/sk0007.PNG", "other/sk0001.png", "sketch/sk0001.png" };
    for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); i++) memfs_put(&m, names[i], "x", 1);
    pg_scan(&p);
    CHECK(p.n == 3 && p.nums[0] == 1 && p.nums[1] == 2 && p.nums[2] == 10); /* sorted, only real pages */
    CHECK(pg_index(&p, 2) == 1 && pg_index(&p, 3) == -1);
    CHECK(pg_next_num(&p) == 11);
    memfs_put(&m, "sketch/sk9999.png", "x", 1);
    pg_scan(&p);
    CHECK(pg_next_num(&p) == 0); /* the last number: no more pages */
    pg_save(&p, 12, &cv);
    CHECK(pg_index(&p, 12) >= 0); /* a saved page is listed */
    char path[48];
    pg_path(path, sizeof(path), 7, "-bad.png");
    CHECK(!strcmp(path, "sketch/sk0007-bad.png"));
    memfs_free(&m);
}

int main(void)
{
    test_save_load();
    test_power_loss();
    test_missing_and_corrupt();
    test_list();
    return CHECK_DONE("test_pages");
}
