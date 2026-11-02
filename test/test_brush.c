/* The brushes and the shades: footprints, determinism, the cursor shapes. SPDX-License-Identifier: MIT */
#include "check.h"
#include <stdlib.h>
#include <string.h>
#include "brush.h"
#include "hist.h"
#include "util.h"
#include "oldsizes.h"

#define W 400
#define H 300
#define STR 50
static uint8_t page_px[STR * H], base_px[STR * H];
static canvas_t page = { page_px, W, H, STR };

static void blank(void)
{
    memset(page_px, 0xff, sizeof(page_px));
    memset(base_px, 0xff, sizeof(base_px));
}
static int black(void)
{
    int n = 0;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) n += !cv_get(&page, x, y);
    return n;
}
static int row_black(int y)
{
    int n = 0;
    for (int x = 0; x < W; x++) n += !cv_get(&page, x, y);
    return n;
}
static int col_black(int x)
{
    int n = 0;
    for (int y = 0; y < H; y++) n += !cv_get(&page, x, y);
    return n;
}
static void line(int brush, int shade, int size, int x0, int y0, int x1, int y1, unsigned pressure)
{
    bstyle_t st = { (uint8_t)brush, (uint8_t)shade, (uint8_t)SZ(size), 0, 0 };
    int p = brush_param(brush, size, pressure);
    brush_seg(&st, brush_is_eraser(brush), &page, x0, y0, p, x1, y1, p, 0);
}

static void test_table(void)
{
    int b = 0;
    for (int i = 0; i < BR_N; i++) {
        CHECK(b == i); /* the cycle goes through the table in order, then wraps */
        CHECK(brush_name(b) && brush_name(b)[0]);
        for (int j = 0; j < i; j++) CHECK(strcmp(brush_name(i), brush_name(j)));
        b = brush_next(b);
    }
    CHECK(b == 0);
    CHECK(BR_N == 8 && brush_is_eraser(BR_ERASER) && !brush_is_eraser(BR_PENCIL) && brush_is_fill(BR_FILL) && !brush_is_fill(BR_ERASER) && !strcmp(brush_name(BR_FILL), "Fill"));
    CHECK(!strcmp(brush_name(BR_FINELINER), "Fineliner") && !strcmp(brush_name(BR_ERASER), "Eraser"));
}

static void test_shades(void)
{
    /* a solid square in each shade: the share of black pixels follows the pattern, and every pixel is where the pattern says */
    int cnt[SH_N];
    for (int sh = 0; sh < SH_N; sh++) {
        blank();
        for (int y = 20; y < 84; y++) cv_span_s(&page, y, 20, 83, 0, sh);
        cnt[sh] = black();
        for (int y = 20; y < 84; y++)
            for (int x = 20; x < 84; x++) CHECK((!cv_get(&page, x, y)) == cv_shade_black(sh, x, y));
        CHECK(cv_get(&page, 19, 40) && cv_get(&page, 84, 40) && cv_get(&page, 40, 19) && cv_get(&page, 40, 84)); /* nothing outside */
    }
    CHECK(cnt[SH_BLACK] == 64 * 64);
    CHECK(cnt[SH_DARK] == 64 * 64 * 11 / 16);
    CHECK(cnt[SH_LIGHT] == 64 * 64 * 5 / 16);
    /* the pattern is fixed to the page: the same square drawn by another brush, or twice, or after an eraser, is the same */
    blank();
    line(BR_MARKER, SH_DARK, 1, 100, 100, 160, 100, 30000);
    uint8_t once[sizeof(page_px)];
    memcpy(once, page_px, sizeof(once));
    line(BR_MARKER, SH_DARK, 1, 100, 100, 160, 100, 30000);
    CHECK(!memcmp(once, page_px, sizeof(once))); /* twice: idempotent, no build-up */
    line(BR_ERASER, SH_DARK, 1, 100, 100, 160, 100, 30000);
    CHECK(black() == 0);
    line(BR_MARKER, SH_DARK, 1, 100, 100, 160, 100, 30000);
    CHECK(!memcmp(once, page_px, sizeof(once))); /* erased and drawn again: the same dots */
    /* a shade only blackens; the eraser ignores the shade */
    blank();
    line(BR_MARKER, SH_BLACK, 1, 100, 100, 160, 100, 0);
    int solid = black();
    line(BR_MARKER, SH_LIGHT, 1, 100, 100, 160, 100, 0);
    CHECK(black() == solid);
    line(BR_ERASER, SH_LIGHT, 2, 130, 100, 130, 100, 0);
    CHECK(black() < solid && cv_get(&page, 130, 100));
}

static void test_footprints(void)
{
    /* fineliner: wider with pressure */
    blank();
    line(BR_FINELINER, 0, 1, 50, 100, 300, 100, 3000);
    line(BR_FINELINER, 0, 1, 50, 200, 300, 200, 65000);
    CHECK(col_black(150) >= 1 && col_black(150) > 0);
    int thin = 0, thick = 0;
    for (int y = 90; y < 110; y++) thin += !cv_get(&page, 150, y);
    for (int y = 190; y < 210; y++) thick += !cv_get(&page, 150, y);
    CHECK(thin >= 1 && thick > thin * 2);
    /* brush pen: a hairline at a light touch, swelling, always more than the fineliner's range at full pressure */
    int w_lo = brush_param(BR_BRUSHPEN, 1, 2000), w_hi = brush_param(BR_BRUSHPEN, 1, 65000);
    CHECK(w_lo <= 2 && w_hi >= 12 && w_hi > w_lo * 4);
    for (unsigned p = 0, prev = 0; p <= 65535; p += 257) {
        unsigned v = (unsigned)brush_param(BR_BRUSHPEN, 1, p);
        CHECK(v >= prev);
        prev = v;
    }
    /* marker: even whatever the pressure, broad and flat (square) */
    CHECK(brush_param(BR_MARKER, 1, 0) == brush_param(BR_MARKER, 1, 65535));
    blank();
    line(BR_MARKER, 0, 1, 50, 100, 300, 100, 20000);
    int side = brush_param(BR_MARKER, 1, 0);
    CHECK(col_black(150) == side && row_black(100) >= 250 && black() == (250 + side) * side);
    /* calligraphy: the width depends on the direction of the stroke */
    blank();
    line(BR_CALLIGRAPHY, 0, 1, 100, 250, 250, 100, 40000); /* along "/" (up right): thin */
    int thin_row = row_black(175);
    blank();
    line(BR_CALLIGRAPHY, 0, 1, 100, 100, 250, 250, 40000); /* along "\" (down right): broad */
    int broad_row = row_black(175);
    CHECK(thin_row >= 2 && thin_row <= 4 && broad_row > thin_row * 4);
    blank();
    line(BR_CALLIGRAPHY, 0, 1, 50, 100, 250, 100, 40000); /* horizontal and vertical both have body */
    line(BR_CALLIGRAPHY, 0, 1, 300, 50, 300, 250, 40000);
    CHECK(col_black(150) > 8 && row_black(150) > 8);
    /* pencil: grainy, lighter at a light touch, the same grain every time */
    blank();
    line(BR_PENCIL, 0, 2, 50, 100, 300, 100, 5000);
    int light = black();
    int holes = 0; /* grain: gaps inside the stroke's body */
    for (int x = 60; x < 290; x++) holes += cv_get(&page, x, 100) && !cv_get(&page, x, 99) && !cv_get(&page, x, 101);
    uint8_t first[sizeof(page_px)];
    memcpy(first, page_px, sizeof(first));
    line(BR_PENCIL, 0, 2, 50, 100, 300, 100, 5000);
    CHECK(!memcmp(first, page_px, sizeof(first)));
    blank();
    line(BR_PENCIL, 0, 2, 50, 100, 300, 100, 65000);
    int hard = black();
    CHECK(light > 100 && hard > light + light / 2 && holes > 5);
    /* spray: dots, denser with pressure, inside its radius, the same dots every time */
    blank();
    line(BR_SPRAY, 0, 1, 100, 150, 300, 150, 3000);
    int sparse = black();
    memcpy(first, page_px, sizeof(first));
    line(BR_SPRAY, 0, 1, 100, 150, 300, 150, 3000);
    CHECK(!memcmp(first, page_px, sizeof(first)));
    blank();
    line(BR_SPRAY, 0, 1, 100, 150, 300, 150, 65000);
    int dense = black();
    CHECK(sparse > 50 && dense > sparse * 2);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            if (!cv_get(&page, x, y)) CHECK(y >= 150 - 18 && y <= 150 + 18 && x >= 100 - 18 && x <= 300 + 18);
    /* eraser: a square hole of its own size */
    blank();
    for (int y = 40; y < 260; y++) cv_span_s(&page, y, 40, 360, 0, 0);
    line(BR_ERASER, 0, 1, 200, 150, 200, 150, 0);
    int e = brush_param(BR_ERASER, 1, 0), holes_px = 0;
    for (int y = 40; y < 260; y++)
        for (int x = 40; x < 361; x++) holes_px += cv_get(&page, x, y);
    CHECK(holes_px == e * e);
    CHECK(e != brush_param(BR_ERASER, 0, 0) && e != brush_param(BR_ERASER, 2, 0));
}

/* ---- the cursors ---- */

static int shape(int br, int size, uint8_t *bits /* (2r+1)^2 */)
{
    int r = brush_shape_r(br, size), n = 0; /* the natural outline; the drawn cursor is checked in test_cursors */
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++) {
            int v = brush_shape_px(br, size, dx, dy);
            if (bits) bits[(dy + r) * (2 * r + 1) + dx + r] = (uint8_t)v;
            n += v;
        }
    return n;
}

static void test_cursors(void)
{
    static uint8_t a[100 * 100], b[100 * 100];
    for (int br = 0; br < BR_N; br++) {
        int prev_r = 0, prev_n = 0;
        for (int size = 0; size < 3; size++) {
            int r = brush_cursor_r(br, size), rs = brush_shape_r(br, size), n = shape(br, size, 0);
            CHECK(r >= 16 && r >= rs + 1 && r < 48 && n > 8 && (br == BR_FILL || (n > prev_n && r >= prev_r))); /* (the bucket has one size) the size shows in the shape; 32 px across at least */
            prev_r = r, prev_n = n;
            /* the drawn cursor holds the whole outline, thicker, and a cross to 16 px when the brush is small */
            for (int dy = -rs; dy <= rs; dy++)
                for (int dx = -rs; dx <= rs; dx++) CHECK(!brush_shape_px(br, size, dx, dy) || brush_cursor_px(br, size, dx, dy));
            if (rs + 1 < 16) CHECK(brush_cursor_px(br, size, 16, 0) && brush_cursor_px(br, size, -16, 0) && brush_cursor_px(br, size, 0, 16) && brush_cursor_px(br, size, 0, -16));
            for (int d = -50; d <= 50; d++) /* nothing is drawn outside its square */
                CHECK(!brush_cursor_px(br, size, d, r + 1 + (d & 3)) && !brush_cursor_px(br, size, r + 1 + (d & 3), d) && !brush_cursor_px(br, size, -r - 1 - (d & 3), d));
        }
    }
    /* every brush has its own shape */
    for (int i = 0; i < BR_N; i++)
        for (int j = i + 1; j < BR_N; j++) {
            int ri = brush_shape_r(i, 1), rj = brush_shape_r(j, 1), same = ri == rj;
            if (same) {
                shape(i, 1, a);
                shape(j, 1, b);
                same = !memcmp(a, b, (size_t)(2 * ri + 1) * (size_t)(2 * ri + 1));
            }
            CHECK(!same);
        }
    /* the ones the owner named */
    int r = brush_shape_r(BR_ERASER, 1), h = r - 1, inner = 0;
    for (int dy = -h + 1; dy < h; dy++)
        for (int dx = -h + 1; dx < h; dx++) inner += brush_shape_px(BR_ERASER, 1, dx, dy);
    CHECK(inner == 0 && brush_shape_px(BR_ERASER, 1, h, 0) && brush_shape_px(BR_ERASER, 1, 0, -h) && !brush_shape_px(BR_ERASER, 1, 0, 0)); /* hollow square */
    CHECK(brush_shape_px(BR_FINELINER, 1, 0, 0)); /* a ring with a dot: the hot spot */
    r = brush_shape_r(BR_FINELINER, 1);
    CHECK(brush_shape_px(BR_FINELINER, 1, r, 0) && brush_shape_px(BR_FINELINER, 1, 0, r) && !brush_shape_px(BR_FINELINER, 1, r - 3, 0));
    CHECK(shape(BR_SPRAY, 1, 0) < shape(BR_FINELINER, 1, 0) * 3 && shape(BR_SPRAY, 1, 0) > 10); /* dotted: gaps in the ring */
    int gaps = 0; /* the spray ring is not closed */
    r = brush_shape_r(BR_SPRAY, 1);
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++) {
            int d2 = dx * dx + dy * dy;
            gaps += d2 <= r * r && d2 >= (r - 1) * (r - 1) && !brush_shape_px(BR_SPRAY, 1, dx, dy);
        }
    CHECK(gaps > 10);
    /* calligraphy: a bar slanted like "/" */
    CHECK(brush_shape_px(BR_CALLIGRAPHY, 1, 3, -3) && brush_shape_px(BR_CALLIGRAPHY, 1, -3, 3) && !brush_shape_px(BR_CALLIGRAPHY, 1, 3, 3));
    /* brush pen: a drop, pointed to the upper right; pencil: a tip with the body to the upper right */
    int ext_ur = -99, ext_ll = -99;
    for (int br = BR_BRUSHPEN; br <= BR_PENCIL; br++) {
        ext_ur = ext_ll = -99;
        r = brush_shape_r(br, 1);
        for (int dy = -r; dy <= r; dy++)
            for (int dx = -r; dx <= r; dx++)
                if (brush_shape_px(br, 1, dx, dy)) {
                    if (dx - dy > ext_ur) ext_ur = dx - dy;
                    if (dy - dx > ext_ll) ext_ll = dy - dx;
                }
        CHECK(ext_ur > ext_ll + 4); /* the point to the upper right, the hot spot at the round end or the tip */
    }
    CHECK(ext_ll <= 2); /* the pencil tip is at the hot spot */
}

/* ---- replay: undo and redo give the same pixels for every brush and shade ---- */

static void test_replay(void)
{
    static hpt_t pts[6000];
    static hstroke_t st[16];
    for (int br = 0; br < BR_N; br++)
        for (int sh = 0; sh < SH_N; sh++) {
            blank();
            hist_t h;
            hist_init(&h, &page, base_px, pts, 6000, st, 16);
            static uint8_t snap[3][sizeof(page_px)];
            box_t b;
            for (int k = 0; k < 3; k++) {
                bstyle_t sty = { (uint8_t)br, (uint8_t)sh, (uint8_t)(k % 3), 0, 0 };
                hist_begin_style(&h, brush_is_eraser(br) ? HK_ERASE : HK_INK, &sty);
                srand((unsigned)(br * 31 + sh * 7 + k));
                int x = 40 + k * 30, y = 60 + k * 40;
                for (int i = 0; i < 60; i++) {
                    hist_add(&h, x, y, brush_param(br, k, (unsigned)(rand() % 65536)), 0);
                    x += rand() % 11 - 3;
                    y += rand() % 9 - 4;
                }
                hist_end(&h);
                memcpy(snap[k], page_px, sizeof(page_px));
            }
            for (int k = 2; k >= 0; k--) {
                CHECK(hist_undo(&h, &b));
                CHECK(k == 0 || !memcmp(snap[k - 1], page_px, sizeof(page_px)));
            }
            uint8_t zero[sizeof(page_px)];
            memset(zero, 0xff, sizeof(zero));
            CHECK(!memcmp(zero, page_px, sizeof(zero)));
            for (int k = 0; k < 3; k++) {
                CHECK(hist_redo(&h, &b));
                CHECK(!memcmp(snap[k], page_px, sizeof(page_px))); /* the same pixels, stroke by stroke */
            }
            /* and when the log is too short, the strokes baked into the base image come out the same */
            blank();
            static hpt_t small[100];
            hist_init(&h, &page, base_px, small, 100, st, 16);
            for (int k = 0; k < 3; k++) {
                bstyle_t sty = { (uint8_t)br, (uint8_t)sh, (uint8_t)(k % 3), 0, 0 };
                hist_begin_style(&h, brush_is_eraser(br) ? HK_ERASE : HK_INK, &sty);
                srand((unsigned)(br * 31 + sh * 7 + k));
                int x = 40 + k * 30, y = 60 + k * 40;
                for (int i = 0; i < 60; i++) {
                    hist_add(&h, x, y, brush_param(br, k, (unsigned)(rand() % 65536)), 0);
                    x += rand() % 11 - 3;
                    y += rand() % 9 - 4;
                }
                hist_end(&h);
            }
            CHECK(!memcmp(snap[2], page_px, sizeof(page_px)));
        }
}

int main(void)
{
    test_table();
    test_shades();
    test_footprints();
    test_cursors();
    test_replay();
    return CHECK_DONE("test_brush");
}
