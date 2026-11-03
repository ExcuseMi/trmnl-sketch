/* Drawing latency: the rolling statistics, their measurement in the pen path, the log line, the debug toast, and a
   batching that never drops a stroke. SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 */
#include "check.h"
#include <stdlib.h>
#include <string.h>
#include "sklat.h"
#include <math.h>
#include "util.h"

static lat_t mk(const int *v, int n)
{
    lat_t l;
    memset(&l, 0, sizeof(l));
    for (int i = 0; i < n; i++) lat_add(&l, (uint32_t)v[i]);
    return l;
}

static void test_stats(void)
{
    int m, p;
    lat_t e;
    memset(&e, 0, sizeof(e));
    CHECK(lat_stats(&e, &m, &p) == 0 && m == 0 && p == 0);
    int one[] = { 7 };
    lat_t a = mk(one, 1);
    CHECK(lat_stats(&a, &m, &p) == 1 && m == 7 && p == 7);
    int five[] = { 5, 1, 4, 2, 3 };
    a = mk(five, 5);
    CHECK(lat_stats(&a, &m, &p) == 5 && m == 3 && p == 5);
    int four[] = { 40, 10, 30, 20 };
    a = mk(four, 4);
    lat_stats(&a, &m, &p);
    CHECK(m == 30 && p == 40); /* an even count: the upper middle */
    int v[20];
    for (int i = 0; i < 20; i++) v[i] = 20 - i;
    a = mk(v, 20);
    lat_stats(&a, &m, &p);
    CHECK(m == 11 && p == 19); /* nearest rank: ceil(0.95 * 20) = 19th */
    /* a ring: only the last SK_LAT_N samples count */
    memset(&a, 0, sizeof(a));
    for (int i = 0; i < 1000; i++) lat_add(&a, i < 872 ? 5000 : (uint32_t)(i - 871));
    CHECK(lat_stats(&a, &m, &p) == SK_LAT_N && m == 65 && p == 122);
    lat_add(&a, 900000); /* capped, not wrapped */
    int worst;
    lat_stats(&a, &m, &worst);
    CHECK(worst <= 65535 && worst >= 122);
    char t[64], tiny[6];
    int ev[] = { 40, 78, 50 }, pu[] = { 3, 9 };
    lat_t le = mk(ev, 3), lp = mk(pu, 2);
    lat_text(t, sizeof(t), &le, &lp);
    CHECK(!strcmp(t, "ev>push 50/78 ms  push 9/9 ms  n 3"));
    lat_text(tiny, sizeof(tiny), &le, &lp);
    CHECK(strlen(tiny) <= 5); /* cut, never over */
}

/* The time from the first event of a batch to the flush that hands it out, counted here without the app's help */
static void test_measured(void)
{
    static rig_t r;
    for (int batch = 16; batch <= 40; batch += 12) {
        rig_new(&r, APP_ORIENT_BAR_BOTTOM);
        sk_set_brush(&r.s, BR_MARKER, r.now += 50); /* (the Pen adds a tapering tail) */
        r.s.batch_ms = batch;
        uint32_t t = 5000, first = 0;
        int expect[SK_LAT_N], ne = 0;
        rect_t out[SK_MAX_RECTS];
        for (int i = 0; i < 100; i++, t += 5) {
            unsigned x, y;
            rig_pt(&r, 300 + i * 6, 500 + (i % 7), &x, &y);
            sk_pen(&r.s, APP_PEN_TIP | APP_PEN_IN_RANGE, x, y, 30000, t);
            if (!first) first = t;
            int n = sk_flush(&r.s, t, out);
            if (n > 0 && ne < SK_LAT_N) expect[ne++] = (int)(t - first), first = 0;
        }
        int m, p, want_m, want_p;
        lat_t ref = mk(expect, ne);
        lat_stats(&ref, &want_m, &want_p);
        int n = lat_stats(&r.s.lat_ev, &m, &p);
        CHECK(n >= ne - 1 && n <= ne + 1); /* the start legend's own flush may add one */
        CHECK(abs(m - want_m) <= 5 && abs(p - want_p) <= 5);
        CHECK(p <= batch + 10); /* never held longer than the window (and a tick) */
        sk_lat_push(&r.s, 12);
        sk_lat_push(&r.s, 30);
        lat_stats(&r.s.lat_push, &m, &p);
        CHECK(m == 30 && p == 30);
        rig_end(&r);
    }
}

static void test_log_and_toast(void)
{
    static rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    uint32_t t = 3000;
    rect_t out[SK_MAX_RECTS];
    for (int i = 0; i < 2600; i++, t += 5) { /* 13 s of a stroke */
        unsigned x, y;
        rig_pt(&r, 200 + (i % 1200), 400 + i / 10 % 300, &x, &y);
        sk_pen(&r.s, APP_PEN_TIP | APP_PEN_IN_RANGE, x, y, 30000, t);
        sk_tick(&r.s, t);
        sk_flush(&r.s, t, out);
    }
    CHECK(r.log_n == 1); /* one line in 13 s: every 10 s */
    CHECK(r.log_samples > 20 && r.log_ev_med > 0 && r.log_ev_p95 >= r.log_ev_med && r.log_ev_p95 <= SK_BATCH_MS + 5);
    sk_pen(&r.s, APP_PEN_IN_RANGE, 0, 0, 0, t);
    for (int i = 0; i < 40; i++) t += 1000, sk_tick(&r.s, t), sk_flush(&r.s, t, out); /* not drawing: no more lines than the one still due */
    CHECK(r.log_n <= 2);
    int seen = r.log_n;
    for (int i = 0; i < 40; i++) t += 1000, sk_tick(&r.s, t), sk_flush(&r.s, t, out);
    CHECK(r.log_n == seen);
    /* tablet button 8: short steps the colour, long toggles the debug toast */
    r.s.mode == SK_DRAW ? (void)0 : abort();
    int shade0 = r.s.sty.shade;
    sk_pen_key(&r.s, 128, 128, t += 100);
    CHECK(r.s.mode == SK_DRAW && r.s.sty.shade == shade0); /* not yet: it may be a long press */
    sk_pen_key(&r.s, 0, 128, t += 900);
    CHECK(r.s.dbg == 1 && r.s.mode == SK_DRAW && r.s.note_on);
    sk_pen_key(&r.s, 128, 128, t += 2000);
    sk_pen_key(&r.s, 0, 128, t += 900);
    CHECK(r.s.dbg == 0 && r.s.mode == SK_DRAW);
    sk_pen_key(&r.s, 128, 128, t += 2000);
    sk_pen_key(&r.s, 0, 128, t += 100);
    CHECK(r.s.mode == SK_DRAW && r.s.dbg == 0 && r.s.sty.shade != shade0); /* the short press: the next colour, no menu */
    rig_end(&r);
}

static void to_glass(uint8_t *panel, const uint8_t *fb, int n, const rect_t *out)
{
    if (n == SK_FULL) memcpy(panel, fb, TS * TH);
    for (int k = 0; k < n && n > 0; k++)
        for (int row = out[k].y; row < out[k].y + out[k].h; row++) memcpy(panel + row * TS + out[k].x / 8, fb + row * TS + out[k].x / 8, (size_t)(out[k].w + 7) / 8);
}

/* Whatever the window, no stroke point is dropped and every pixel that changed is in a pushed rectangle */
static void test_batching_covers(void)
{
    static rig_t r;
    static uint8_t panel[TS * TH];
    for (int batch = 0; batch <= 40; batch += 8) {
        rig_new(&r, APP_ORIENT_BAR_BOTTOM);
        sk_set_brush(&r.s, BR_MARKER, r.now += 50); /* (the Pen adds a tapering tail) */
        r.s.batch_ms = batch;
        memset(panel, 0xff, sizeof(panel));
        memset(panel, 0, sizeof(panel)); /* the glass is unknown until the start's full refresh */
        uint32_t t = 9000;
        rect_t out[SK_MAX_RECTS];
        srand(7);
        int pts = 0, px = -1, py = -1;
        for (int s = 0; s < 6; s++)
            for (int i = 0; i < 120; i++, t += 5) {
                int x = 150 + s * 250 + (int)(100 * sin(i * 0.1 + s)) + rand() % 3, y = 300 + i * 8 + rand() % 3;
                unsigned ux, uy;
                rig_pt(&r, x, y, &ux, &uy);
                sk_pen(&r.s, APP_PEN_TIP | APP_PEN_IN_RANGE, ux, uy, 20000 + (unsigned)rand() % 20000, t);
                CHECK(r.s.cursor == 0); /* no hover cursor is drawn while the tip is down */
                if (x != px || y != py) pts++;
                px = x, py = y;
                to_glass(panel, r.fb, sk_flush(&r.s, t, out), out);
                if (i == 119) {
                    sk_pen(&r.s, APP_PEN_IN_RANGE, ux, uy, 0, t += 5);
                    px = py = -1;
                }
            }
        for (int k = 0; k < 20; k++) { /* settle: whatever is pending goes out */
            t += 100;
            sk_tick(&r.s, t);
            to_glass(panel, r.fb, sk_flush(&r.s, t, out), out);
        }
        CHECK(!memcmp(panel, r.fb, sizeof(panel))); /* every changed pixel reached the glass */
        uint32_t logged = 0;
        for (int q = 0; q < r.s.hist.st_n; q++) logged += r.s.hist.st[q].n;
        CHECK((int)logged == pts); /* every distinct point of the pen is in the log */
        rig_end(&r);
    }
}

int main(void)
{
    test_stats();
    test_measured();
    test_log_and_toast();
    test_batching_covers();
    return CHECK_DONE("test_lat");
}
