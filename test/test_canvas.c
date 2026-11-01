/* Discs and segments: no gaps at any speed, clipping, dirty boxes, flips. SPDX-License-Identifier: MIT */
#include "check.h"
#include <stdlib.h>
#include <string.h>
#include "canvas.h"

#define W 400
#define H 300
static uint8_t buf[(W / 8) * H];
static canvas_t cv = { buf, W, H, W / 8 };

static int black(void)
{
    int n = 0;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) n += !cv_get(&cv, x, y);
    return n;
}

static void test_disc(void)
{
    cv_fill(&cv, 1);
    box_t b;
    box_clear(&b);
    cv_disc(&cv, 100, 100, 1, 0, &b);
    CHECK(black() == 1 && !cv_get(&cv, 100, 100));
    CHECK(b.x0 == 100 && b.x1 == 100 && b.y0 == 100 && b.y1 == 100);
    cv_fill(&cv, 1);
    box_clear(&b);
    cv_disc(&cv, 100, 100, 17, 0, &b);
    int n = black();
    CHECK(n > 200 && n < 260); /* about pi * 8.5^2 = 227 */
    CHECK(!cv_get(&cv, 100, 92) && !cv_get(&cv, 100, 108) && cv_get(&cv, 100, 90) && cv_get(&cv, 100, 110));
    CHECK(b.x0 <= 92 && b.x1 >= 108 && b.y0 <= 92 && b.y1 >= 108);
    for (int y = 0; y < H; y++) /* nothing outside the box */
        for (int x = 0; x < W; x++)
            if (!cv_get(&cv, x, y)) CHECK(x >= b.x0 && x <= b.x1 && y >= b.y0 && y <= b.y1);
    cv_disc(&cv, 100, 100, 17, 1, 0); /* white again */
    CHECK(black() == 0);
    cv_disc(&cv, -3, -3, 17, 0, 0); /* clipped at the corner, no crash */
    cv_disc(&cv, W + 2, H + 2, 17, 0, 0);
    CHECK(black() > 0);
}

/* The path of a segment is black everywhere along its longer axis, whatever the distance: a pen that moved 400 px
   between two reports leaves a line, not dots */
static void test_no_gaps(void)
{
    srand(7);
    for (int t = 0; t < 300; t++) {
        cv_fill(&cv, 1);
        int x0 = rand() % W, y0 = rand() % H, x1 = rand() % W, y1 = rand() % H;
        cv_seg(&cv, x0, y0, 1, x1, y1, 1, 0, 0);
        int dx = x1 - x0, dy = y1 - y0, adx = abs(dx), ady = abs(dy);
        int n = adx > ady ? adx : ady;
        for (int i = 0; i <= n; i++) {
            /* the ideal point of step i (rounded), one of its eight neighbours must be black */
            float fx = n ? x0 + (float)dx * (float)i / (float)n : (float)x0, fy = n ? y0 + (float)dy * (float)i / (float)n : (float)y0;
            int ok = 0;
            for (int ox = -1; ox <= 1; ox++)
                for (int oy = -1; oy <= 1; oy++) ok |= !cv_get(&cv, (int)(fx + 0.5f) + ox, (int)(fy + 0.5f) + oy);
            CHECK(ok);
        }
    }
    /* a fast thick stroke: every pixel of the ideal line, its ends and the middle, is covered with its width */
    cv_fill(&cv, 1);
    cv_seg(&cv, 20, 20, 9, 380, 280, 9, 0, 0);
    for (int i = 0; i <= 100; i++) {
        int x = 20 + 360 * i / 100, y = 20 + 260 * i / 100;
        CHECK(!cv_get(&cv, x, y) && !cv_get(&cv, x + 3, y) && !cv_get(&cv, x - 3, y));
    }
}

static void test_taper(void)
{
    cv_fill(&cv, 1);
    cv_seg(&cv, 20, 150, 3, 380, 150, 15, 0, 0);
    int near_w = 0, far_w = 0;
    for (int y = 100; y < 200; y++) near_w += !cv_get(&cv, 40, y), far_w += !cv_get(&cv, 360, y);
    CHECK(far_w > near_w + 6); /* the width follows the pressure along the segment */
}

static void test_boxes_and_copy(void)
{
    box_t a;
    box_clear(&a);
    CHECK(box_empty(&a));
    box_add(&a, 5, 6);
    box_add(&a, 1, 9);
    CHECK(a.x0 == 1 && a.x1 == 5 && a.y0 == 6 && a.y1 == 9);
    box_t e;
    box_clear(&e);
    box_union(&a, &e);
    CHECK(a.x0 == 1 && a.x1 == 5);
    uint8_t other[sizeof(buf)];
    canvas_t o = { other, W, H, W / 8 };
    cv_fill(&cv, 1);
    cv_fill(&o, 1);
    cv_disc(&o, 200, 150, 15, 0, 0);
    box_t b = { 190, 140, 210, 160 };
    cv_copy_box(&cv, &o, &b);
    CHECK(!cv_get(&cv, 200, 150) && cv_get(&cv, 100, 100));
    box_t far = { -50, -50, 5000, 5000 };
    cv_copy_box(&cv, &o, &far); /* clipped */
    CHECK(!memcmp(buf, other, sizeof(buf)));
    cv_flip(&cv, 3, 3);
    CHECK(!cv_get(&cv, 3, 3));
    cv_flip(&cv, 3, 3);
    CHECK(cv_get(&cv, 3, 3));
    cv_flip(&cv, -1, 5); /* outside: nothing */
}

int main(void)
{
    test_disc();
    test_no_gaps();
    test_taper();
    test_boxes_and_copy();
    return CHECK_DONE("test_canvas");
}
