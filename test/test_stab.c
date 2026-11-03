/* The stabiliser: smooths, lags a bounded distance, ends where the pen ended, is on by default. SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 */
#include "check.h"
#include <math.h>
#include <stdlib.h>
#include "skstab.h"
#include "util.h"

/* A straight line with the hand shaking: +-3 px across, +-1 along */
static void shaky(int i, int *x, int *y)
{
    static const int jx[8] = { 1, -1, 0, 1, -1, 1, 0, -1 }, jy[8] = { 3, -3, 2, -2, 3, -3, 1, -1 };
    *x = 200 + i * 3 / 5 + jx[i & 7];
    *y = 400 + jy[(i * 5) & 7];
}

static double rough(int level)
{
    stab_t f;
    int x, y, ox, oy;
    shaky(0, &x, &y);
    stab_begin(&f, level, x, y);
    double sum = 0;
    int py = y, ppy = y;
    for (int i = 1; i < 200; i++) {
        shaky(i, &x, &y);
        stab_push(&f, x, y, &ox, &oy);
        if (i > 20) sum += fabs((double)oy - 2.0 * py + ppy); /* second difference across the line */
        ppy = py, py = oy;
    }
    return sum;
}

static void test_smooths(void)
{
    CHECK(rough(0) > 2 * rough(3));
    CHECK(rough(3) > rough(6));
    CHECK(rough(6) > rough(10));
    CHECK(rough(6) < rough(0) / 4); /* the line is calm at 6 */
    stab_t f;
    int ox, oy;
    stab_begin(&f, 0, 5, 5);
    for (int i = 0; i < 50; i++) {
        stab_push(&f, i * 7, 100 - i, &ox, &oy);
        CHECK(ox == i * 7 && oy == 100 - i); /* 0: off, exact */
    }
}

/* A steady pen at v px per event: the point trails by at most the bound, at every level and speed */
static void test_lag(void)
{
    static const double speed[] = { 0.1, 0.3, 1, 2, 4, 8 }; /* px per 5 ms event: 20 px/s to 1600 px/s */
    for (int level = 1; level <= SK_SMOOTH_MAX; level++)
        for (unsigned k = 0; k < sizeof(speed) / sizeof(speed[0]); k++) {
            double v = speed[k], worst = 0;
            stab_t f;
            int ox = 0, oy = 0;
            stab_begin(&f, level, 100, 100);
            for (int i = 1; i <= 600; i++) {
                stab_push(&f, (int)lround(100 + v * i), 100, &ox, &oy);
                if (i > 400) worst = fmax(worst, 100 + v * i - ox);
            }
            double bound = 2 + 6 * v > 12 ? 12 : 2 + 6 * v;
            if (v >= 4) bound = 14;
            CHECK(worst >= -1 && worst <= bound);
            CHECK(oy == 100);
        }
}

static void set_smooth(rig_t *r, int level) { r->s.smooth = level; }

static unsigned pt_x(const rig_t *r, int i) { return (unsigned)r->s.hist.pts[r->s.hist.st[r->s.hist.st_cur - 1].first + (unsigned)i].x; }

/* The stroke's last point is the pen's own, however much the filter trails; the first too */
static void test_end_exact(void)
{
    static rig_t r;
    for (int level = 1; level <= 10; level += 3) {
        rig_new(&r, APP_ORIENT_BAR_BOTTOM);
        set_smooth(&r, level);
        sk_set_brush(&r.s, BR_MARKER, r.now += 50); /* (the Pen adds a tapering tail) */
        CHECK(r.s.smooth == level);
        int xs[80];
        for (int i = 0; i < 40; i++) shaky(i, &xs[2 * i], &xs[2 * i + 1]), xs[2 * i + 1] += 300;
        rig_stroke(&r, xs, 40, 30000, 0);
        hist_t *h = &r.s.hist;
        const hstroke_t *st = &h->st[h->st_cur - 1];
        CHECK(h->st_cur == 1 && st->n >= 2);
        CHECK(h->pts[st->first].x == xs[0] && h->pts[st->first].y == xs[1]);
        const hpt_t *last = &h->pts[st->first + st->n - 1];
        CHECK(abs(last->x - xs[78]) <= 1 && abs(last->y - xs[79]) <= 1); /* rig_pt rounds the tablet unit */
        CHECK(cv_get(&r.s.page, last->x, last->y) == 0); /* ink at the end */
        CHECK(pt_x(&r, 0) == (unsigned)xs[0]);
        rig_end(&r);
    }
}

/* The app's own level is on (SK_SMOOTH_DEF, no setting): a shaky stroke is not the pen's raw points */
static void test_on_by_default(void)
{
    static rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    r.keep_smooth = 1;
    rig_start(&r, APP_ORIENT_BAR_BOTTOM);
    CHECK(r.s.smooth == SK_SMOOTH_DEF && SK_SMOOTH_DEF > 0);
    int xs[80], moved = 0;
    for (int i = 0; i < 40; i++) shaky(i, &xs[2 * i], &xs[2 * i + 1]), xs[2 * i + 1] += 300;
    rig_stroke(&r, xs, 40, 30000, 0);
    const hstroke_t *s = &r.s.hist.st[r.s.hist.st_cur - 1];
    for (uint32_t i = 1; i + 1 < s->n && i < 38; i++) moved += abs(r.s.hist.pts[s->first + i].y - xs[2 * i + 1]) > 0;
    CHECK(moved > 5);
    rig_end(&r);
}

int main(void)
{
    test_smooths();
    test_lag();
    test_end_exact();
    test_on_by_default();
    return CHECK_DONE("stab");
}
