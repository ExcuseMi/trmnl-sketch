/* Undo, redo, clear: the exact pixels come back; the log is bounded. SPDX-License-Identifier: MIT */
#include "check.h"
#include <stdlib.h>
#include <string.h>
#include "hist.h"

#define W 320
#define H 240
#define BYTES ((W / 8) * H)
static uint8_t page_px[BYTES], base_px[BYTES], snap[80][BYTES];
static canvas_t page = { page_px, W, H, W / 8 };

static void stroke(hist_t *h, int kind, int seed, int n, box_t *dirty)
{
    srand((unsigned)seed);
    hist_begin(h, kind);
    int x = 20 + rand() % 280, y = 20 + rand() % 200;
    for (int i = 0; i < n; i++) {
        x += rand() % 41 - 20, y += rand() % 31 - 15;
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        if (x > W - 1) x = W - 1;
        if (y > H - 1) y = H - 1;
        hist_add(h, x, y, kind == HK_ERASE ? 21 : 1 + rand() % 9, dirty);
    }
    hist_end(h);
}

static void setup(hist_t *h, hpt_t *pts, uint32_t pts_cap, hstroke_t *st, int st_cap)
{
    cv_fill(&page, 1);
    hist_init(h, &page, base_px, pts, pts_cap, st, st_cap);
}

static void test_undo_redo_exact(void)
{
    static hpt_t pts[4000];
    static hstroke_t st[16];
    hist_t h;
    setup(&h, pts, 4000, st, 16);
    memcpy(snap[0], page_px, BYTES);
    for (int i = 1; i <= 6; i++) {
        stroke(&h, i == 4 ? HK_ERASE : HK_INK, 100 + i, 25, 0);
        memcpy(snap[i], page_px, BYTES);
    }
    CHECK(memcmp(snap[5], snap[6], BYTES) != 0);
    for (int i = 5; i >= 0; i--) { /* every undo is exactly the page before that stroke */
        box_t b;
        CHECK(hist_undo(&h, &b));
        CHECK(!memcmp(page_px, snap[i], BYTES));
    }
    box_t b;
    CHECK(!hist_undo(&h, &b) && !hist_can_undo(&h));
    for (int i = 1; i <= 6; i++) {
        CHECK(hist_redo(&h, &b));
        CHECK(!memcmp(page_px, snap[i], BYTES));
        CHECK(b.x0 <= b.x1); /* the box tells what changed */
    }
    CHECK(!hist_redo(&h, &b));
    /* a new stroke after an undo ends the redo list */
    hist_undo(&h, &b);
    hist_undo(&h, &b);
    CHECK(hist_can_redo(&h));
    stroke(&h, HK_INK, 999, 10, 0);
    CHECK(!hist_can_redo(&h) && !hist_redo(&h, &b));
}

/* The box an undo reports holds every pixel it changed (the screen is updated from it) */
static void test_undo_box(void)
{
    static hpt_t pts[4000];
    static hstroke_t st[16];
    hist_t h;
    setup(&h, pts, 4000, st, 16);
    stroke(&h, HK_INK, 1, 30, 0);
    stroke(&h, HK_INK, 2, 30, 0);
    uint8_t before[BYTES];
    memcpy(before, page_px, BYTES);
    box_t b;
    hist_undo(&h, &b);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            if (cv_get(&page, x, y) != ((before[y * (W / 8) + x / 8] >> (7 - x % 8)) & 1))
                CHECK(x >= b.x0 && x <= b.x1 && y >= b.y0 && y <= b.y1);
}

static void test_clear(void)
{
    static hpt_t pts[4000];
    static hstroke_t st[16];
    hist_t h;
    setup(&h, pts, 4000, st, 16);
    stroke(&h, HK_INK, 1, 40, 0);
    stroke(&h, HK_INK, 2, 40, 0);
    uint8_t drawn[BYTES];
    memcpy(drawn, page_px, BYTES);
    box_t b;
    hist_clear(&h, &b);
    uint8_t white[BYTES];
    memset(white, 0xff, BYTES);
    CHECK(!memcmp(page_px, white, BYTES));
    CHECK(hist_undo(&h, &b));
    CHECK(!memcmp(page_px, drawn, BYTES)); /* clear then undo: the exact pixels */
    CHECK(hist_redo(&h, &b));
    CHECK(!memcmp(page_px, white, BYTES));
    stroke(&h, HK_INK, 3, 10, 0); /* drawing after a clear, then undo twice */
    CHECK(hist_undo(&h, &b) && hist_undo(&h, &b));
    CHECK(!memcmp(page_px, drawn, BYTES));
}

static void test_eraser(void)
{
    static hpt_t pts[4000];
    static hstroke_t st[16];
    hist_t h;
    setup(&h, pts, 4000, st, 16);
    stroke(&h, HK_INK, 5, 60, 0);
    uint8_t ink[BYTES];
    memcpy(ink, page_px, BYTES);
    int before = 0, after = 0;
    for (int i = 0; i < BYTES; i++) before += __builtin_popcount((unsigned)~ink[i] & 0xff);
    /* erase along the first part of the same stroke: white again where the eraser passed */
    hist_begin(&h, HK_ERASE);
    srand(5);
    int x = 20 + rand() % 280, y = 20 + rand() % 200;
    for (int i = 0; i < 20; i++) {
        hist_add(&h, x, y, 25, 0);
        x += rand() % 41 - 20, y += rand() % 31 - 15;
        if (x < 0) x = 0;
        if (y < 0) y = 0;
    }
    hist_end(&h);
    for (int i = 0; i < BYTES; i++) after += __builtin_popcount((unsigned)~page_px[i] & 0xff);
    CHECK(after < before);
    box_t b;
    hist_undo(&h, &b);
    CHECK(!memcmp(page_px, ink, BYTES)); /* the ink the eraser took comes back exactly */
}

/* More strokes than the log holds: the oldest are baked into the base image, undo goes back as far as the log
   reaches and the page stays exact; the memory never grows */
static void test_bounded(void)
{
    static hpt_t pts[4000];
    static hstroke_t st[8];
    hist_t h;
    setup(&h, pts, 4000, st, 8);
    for (int i = 1; i <= 30; i++) {
        stroke(&h, HK_INK, 300 + i, 20, 0);
        memcpy(snap[i % 40], page_px, BYTES);
        CHECK(h.st_n <= 8);
        CHECK(h.pt_n <= 4000);
    }
    box_t b;
    int undone = 0;
    while (hist_undo(&h, &b)) {
        undone++;
        CHECK(!memcmp(page_px, snap[(30 - undone) % 40], BYTES));
    }
    CHECK(undone == 8); /* the log's depth */
    /* the page after the last undo is what stood 8 strokes ago, with everything older kept */
    CHECK(!hist_can_undo(&h));
}

/* The point log full inside one long stroke: nothing is lost on the page, memory stays bounded */
static void test_points_full(void)
{
    static hpt_t pts[50];
    static hstroke_t st[8];
    hist_t h;
    setup(&h, pts, 50, st, 8);
    stroke(&h, HK_INK, 1, 10, 0);
    uint8_t reference[BYTES];
    cv_fill(&page, 1);
    hist_reset(&h);
    stroke(&h, HK_INK, 1, 10, 0);
    memcpy(reference, page_px, BYTES);
    cv_fill(&page, 1);
    hist_reset(&h);
    stroke(&h, HK_INK, 1, 10, 0);
    stroke(&h, HK_INK, 2, 40, 0);
    CHECK(h.pt_n <= 50);
    stroke(&h, HK_INK, 3, 200, 0); /* far more points than the log holds */
    CHECK(h.pt_n <= 50 && h.st_n <= 8);
    uint8_t after[BYTES];
    memcpy(after, page_px, BYTES);
    box_t b;
    while (hist_undo(&h, &b)) { }
    CHECK(h.st_cur == 0);
    /* the page ink drawn so far is never taken away by the bounds */
    int a = 0, r = 0;
    for (int i = 0; i < BYTES; i++) a += __builtin_popcount((unsigned)~after[i] & 0xff), r += __builtin_popcount((unsigned)~reference[i] & 0xff);
    CHECK(a > r);
}

/* The log fills while a stroke is open: the older strokes go into the base, the open one stays whole and undoable */
static void test_points_full_keeps_open(void)
{
    static hpt_t pts[60];
    static hstroke_t st[8];
    hist_t h;
    setup(&h, pts, 60, st, 8);
    stroke(&h, HK_INK, 11, 20, 0);
    stroke(&h, HK_INK, 12, 20, 0);
    memcpy(snap[2], page_px, BYTES);
    stroke(&h, HK_INK, 13, 30, 0); /* 20 + 20 + 30 points: the log is full inside this stroke */
    CHECK(memcmp(page_px, snap[2], BYTES) != 0);
    box_t b;
    CHECK(hist_undo(&h, &b));
    CHECK(!memcmp(page_px, snap[2], BYTES)); /* the whole open stroke undoes at once */
}

static void test_empty_stroke(void)
{
    static hpt_t pts[100];
    static hstroke_t st[8];
    hist_t h;
    setup(&h, pts, 100, st, 8);
    hist_begin(&h, HK_INK);
    hist_end(&h); /* a pen down without a point is no stroke */
    box_t b;
    CHECK(!hist_can_undo(&h) && !hist_undo(&h, &b));
    hist_begin(&h, HK_INK);
    hist_add(&h, 50, 50, 5, 0);
    hist_add(&h, 50, 50, 5, 0); /* the same point twice is one */
    hist_end(&h);
    CHECK(h.pt_n == 1 && !cv_get(&page, 50, 50));
}

int main(void)
{
    test_undo_redo_exact();
    test_undo_box();
    test_clear();
    test_eraser();
    test_bounded();
    test_points_full();
    test_points_full_keeps_open();
    test_empty_stroke();
    return CHECK_DONE("test_hist");
}
