/* The bucket's regions stay the same when its passes get faster: the mask, box and size of taps on a busy page (every
   look, every gap, a small seed stack, regions at the page edges) hashed against the plan before the passes were
   windowed (2026-11-03, round 2). Run with an argument to print the sums. SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 */
#include "util.h"
#include "fill.h"
#include <math.h>

static uint8_t px[TS * TH];
static canvas_t cvs = { px, TW, TH, TS };

static uint64_t fnv(uint64_t h, const void *p, size_t n)
{
    const uint8_t *b = p;
    for (size_t i = 0; i < n; i++) h = (h ^ b[i]) * 1099511628211ull;
    return h;
}

static void page(void)
{
    cv_fill(&cvs, 1);
    for (int y = 100; y < 600; y++) cv_span_pat(&cvs, y, 1000, 1700, SH_GREY); /* a grey area, its lines below */
    for (int y = 700; y < 1000; y++) cv_span_pat(&cvs, y, 1100, 1500, 3);    /* a dark shade */
    for (int k = 0; k < 40; k++) {                                             /* lines with gaps of 1 .. 8 px */
        int x0 = 50 + (k * 137) % 1700, y0 = 50 + (k * 251) % 1300, x1 = x0 + 200 - (k * 31) % 400, y1 = y0 + 150 - (k * 17) % 300;
        cv_seg(&cvs, x0, y0, 3 + k % 4, x1, y1, 3 + k % 4, 0, 0);
        int gx = (x0 + x1) / 2, gy = (y0 + y1) / 2, g = 1 + k % 8;
        for (int y = gy - 6; y <= gy + 6; y++) cv_span_s(&cvs, y, gx - g / 2, gx + g - g / 2, 1, 0);
    }
    for (int k = 0; k < 6; k++) /* rings, one cut open */
        for (int a = 0; a < 360; a += 2) {
            if (k == 2 && a > 40 && a < 46) continue;
            int r = 60 + k * 25;
            cv_disc(&cvs, 300 + (int)(r * cos(a * 0.0174533)), 1100 + (int)(r * sin(a * 0.0174533)), 4, 0, 0);
        }
    uint32_t s = 7;
    for (int i = 0; i < 4000; i++) { /* dust */
        s = s * 1103515245u + 12345u;
        cv_set(&cvs, (int)(s >> 8) % TW, (int)(s >> 20) % TH, 0);
    }
    for (int y = 0; y < 30; y++) cv_span_s(&cvs, y, 0, 60, 0, 0); /* a corner block */
}

int main(int argc, char **argv)
{
    static const struct { int x, y, shade, gap, cap; uint64_t sum; } t[] = {
        { 1800, 1350, SH_GREY, 3, 0, 0xff22b1f0602e61e4ull }, /* the page around everything */
        { 1800, 1350, SH_BLACK, 0, 0, 0xcc81c3a556ba3e34ull },
        { 1300, 850, SH_WHITE, 6, 0, 0x3a64ec3db9b07f2dull },
        { 300, 1100, SH_GREY, 3, 0, 0x45274b1a6c4dbb78ull }, /* inside the rings */
        { 300, 1100, SH_BLACK, 1, 0, 0xca3c0770cc5d80c9ull },
        { 1300, 300, SH_BLACK, 3, 0, 0x901aa7d04f61782dull }, /* the grey area */
        { 1300, 300, SH_WHITE, 2, 0, 0x275b17da2ddcf1c3ull },
        { 1300, 850, SH_GREY, 4, 0, 0x0812089ff6abefe4ull },  /* the dark shade */
        { 10, 10, SH_WHITE, 3, 0, 0x4e98b3f7cde76fb1ull },    /* the black corner */
        { 1871, 0, SH_BLACK, 6, 0, 0xf4aed1f5ba63221full },   /* the page's corner, the widest gap */
        { 1800, 1350, SH_GREY, 3, 256, 0x516442878400fb80ull }, /* a small seed stack: rescans */
        { 900, 700, SH_GREY, 5, 0, 0xc990fe4137c44cf4ull },
        { 1800, 1350, SH_BLACK, 2, 2048, 0xd7ebd316485a82e6ull },
    };
    page();
    int print = argc > 1;
    (void)argv;
    for (size_t i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
        bf_t r;
        int rc = bf_plan(&r, &cvs, t[i].x, t[i].y, t[i].shade, t[i].gap, (size_t)t[i].cap);
        uint64_t h = fnv(1469598103934665603ull, &rc, sizeof(rc));
        if (rc == BF_OK) {
            int bb[4] = { r.bb.x0, r.bb.y0, r.bb.x1, r.bb.y1 };
            h = fnv(h, bb, sizeof(bb));
            h = fnv(h, &r.px, sizeof(r.px));
            h = fnv(h, r.mask, (size_t)r.stride * (size_t)r.h);
            bf_free(&r);
        }
        if (print) printf("%zu: rc %d 0x%016llxull\n", i, rc, (unsigned long long)h);
        else CHECK(h == t[i].sum);
    }
    return print ? 0 : CHECK_DONE("test_fillgold");
}
