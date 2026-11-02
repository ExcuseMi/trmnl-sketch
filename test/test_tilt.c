/* The pen's tilt: a tilted pencil lays a wider, lighter mark along the lean; the brush pen a little wider; other brushes
   ignore it; the log replays it. SPDX-License-Identifier: MIT */
#include "check.h"
#include <math.h>
#include <string.h>
#include "util.h"

static void test_code(void)
{
    for (int a = -48; a <= 42; a += 6)
        for (int b = -48; b <= 42; b += 6) {
            int x, y;
            bt_decode(bt_code(a, b), &x, &y);
            CHECK(x == a && y == b);
        }
    int x, y;
    bt_decode(bt_code(90, -90), &x, &y);
    CHECK(x == 42 && y == -48); /* the extremes are capped, not wrapped */
    bt_decode(bt_code(0, 0), &x, &y);
    CHECK(x == 0 && y == 0 && bt_code(0, 0) == 0);
    bt_decode(bt_code(2, -2), &x, &y);
    CHECK(x == 0 && y == 0); /* below half a step: upright */
}

typedef struct { int w, h, n; } fp_t; /* the black pixels' box and count of one stamp */
static fp_t stamp(int brush, int size, int tx, int ty, int shade)
{
    static uint8_t px[200 * 25];
    canvas_t c = { px, 200, 200, 25 };
    cv_fill(&c, 1);
    bstyle_t st = { (uint8_t)brush, (uint8_t)shade, (uint8_t)size, 0 };
    brush_segt(&st, 0, &c, 100, 100, brush == BR_PENCIL || brush == BR_SPRAY ? 200 : 14, bt_code(tx, ty), 100, 100, brush == BR_PENCIL || brush == BR_SPRAY ? 200 : 14, bt_code(tx, ty), 0);
    int x0 = 999, x1 = -1, y0 = 999, y1 = -1, n = 0;
    for (int y = 0; y < 200; y++)
        for (int x = 0; x < 200; x++)
            if (!cv_get(&c, x, y)) n++, x0 = x < x0 ? x : x0, x1 = x > x1 ? x : x1, y0 = y < y0 ? y : y0, y1 = y > y1 ? y : y1;
    return (fp_t){ n ? x1 - x0 + 1 : 0, n ? y1 - y0 + 1 : 0, n };
}

static void test_pencil(void)
{
    fp_t up = stamp(BR_PENCIL, 30, 0, 0, 0), tx = stamp(BR_PENCIL, 30, 48, 0, 0), ty = stamp(BR_PENCIL, 30, 0, 48, 0), d = stamp(BR_PENCIL, 30, 36, 36, 0);
    CHECK(up.n > 100);
    CHECK(tx.w > up.w * 3 / 2);       /* wider along the lean ... */
    CHECK(tx.h <= up.h);              /* ... and no taller across it */
    CHECK(ty.h > up.h * 3 / 2 && ty.w <= up.w);
    CHECK(d.w > up.w && d.h > up.h);  /* a diagonal lean stretches both */
    CHECK(tx.n * 100 / (tx.w * tx.h) < up.n * 100 / (up.w * up.h)); /* lighter: sparser per area */
    fp_t mid = stamp(BR_PENCIL, 30, 24, 0, 0);
    CHECK(mid.w > up.w && mid.w < tx.w); /* more lean, more stretch */
}

static void test_others(void)
{
    static const int ignore[] = { BR_MARKER, BR_SPRAY, BR_ERASER };
    for (unsigned k = 0; k < sizeof(ignore) / sizeof(ignore[0]); k++)
        for (int tx = -42; tx <= 42; tx += 42) {
            fp_t a = stamp(ignore[k], 20, 0, 0, 3), b = stamp(ignore[k], 20, tx, 30, 3);
            CHECK(a.w == b.w && a.h == b.h && a.n == b.n);
        }
    fp_t up = stamp(BR_PEN, 20, 0, 0, 0), t = stamp(BR_PEN, 20, 42, 0, 0);
    CHECK(t.w > up.w && t.w <= up.w * 3 / 2); /* lightly wider */
}

static unsigned page_hash(const rig_t *r)
{
    unsigned h = 2166136261u;
    for (int i = 0; i < TS * TH; i++) h = (h ^ r->s.page.px[i]) * 16777619u;
    return h;
}

static unsigned draw(int brush, int tilt_x, int tilt_y, rig_t *r)
{
    rig_new(r, APP_ORIENT_BAR_BOTTOM);
    sk_set_size(&r->s, 2, r->now += 50);
    sk_set_brush(&r->s, brush, r->now += 50);
    for (int i = 0; i < 30; i++) {
        sk_pen_tilt(&r->s, tilt_x * i / 29, tilt_y);
        rig_pen(r, 300 + i * 20, 500 + (i % 3) * 4, APP_PEN_TIP | APP_PEN_IN_RANGE, 30000);
    }
    sk_pen_tilt(&r->s, 0, 0);
    rig_pen(r, 880, 500, APP_PEN_IN_RANGE, 0);
    return page_hash(r);
}

static void test_in_the_app(void)
{
    static rig_t r;
    unsigned up = draw(BR_PENCIL, 0, 0, &r);
    rig_end(&r);
    unsigned tilted = draw(BR_PENCIL, 45, 20, &r);
    CHECK(tilted != up);
    /* undo and redo replay the tilt: the same pixels */
    box_t b;
    CHECK(hist_undo(&r.s.hist, &b) && page_hash(&r) != tilted);
    CHECK(hist_redo(&r.s.hist, &b) && page_hash(&r) == tilted);
    /* a restart's reload of the log is not needed: the page is the pixels. The marker ignores the tilt through the app too */
    rig_end(&r);
    unsigned f0 = draw(BR_MARKER, 0, 0, &r);
    rig_end(&r);
    CHECK(draw(BR_MARKER, 45, 20, &r) == f0);
    rig_end(&r);
}

/* the lean turns with the page: a lean along the tablet's x is along the direction the pen moves on the page for a move along the tablet's x */
static void test_frames(void)
{
    for (int o = 0; o < 4; o++)
        for (int turn = 0; turn < 3; turn++) {
            if (turn && o != APP_ORIENT_BAR_LEFT && o != APP_ORIENT_BAR_RIGHT) continue;
            penmap_t m;
            pm_init(&m, o, 1872, 1404, 254000, 158750, 1, turn != 0);
            int ax, ay, bx, by, lx, ly;
            pm_map(&m, 20000, 30000, &ax, &ay);
            pm_map(&m, 30000, 30000, &bx, &by); /* a move along the tablet's x */
            bt_decode(sk_tilt_code(&m, 42, 0), &lx, &ly);
            int mx = bx - ax, my = by - ay;
            CHECK(lx * mx + ly * my > 0 && abs(lx * my - ly * mx) <= 6 * (abs(mx) + abs(my))); /* parallel, same way */
            pm_map(&m, 30000, 40000, &bx, &by); /* along the tablet's y */
            bt_decode(sk_tilt_code(&m, 0, 42), &lx, &ly);
            pm_map(&m, 30000, 30000, &ax, &ay);
            mx = bx - ax, my = by - ay;
            CHECK(lx * mx + ly * my > 0 && abs(lx * my - ly * mx) <= 6 * (abs(mx) + abs(my)));
        }
}

int main(void)
{
    test_code();
    test_pencil();
    test_others();
    test_in_the_app();
    test_frames();
    return CHECK_DONE("test_tilt");
}
