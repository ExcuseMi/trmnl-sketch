/* Refresh rectangles: bounded in number, aligned, and together they cover every pixel that changed. The fast-stroke
   run prints how many rectangles and pixels per second the app pushes. SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 */
#include "check.h"
#include <stdio.h>
#include "util.h"
#include "skpush.h"

static int covered(const rect_t *r, int n, int x, int y)
{
    for (int i = 0; i < n; i++)
        if (x >= r[i].x && x < r[i].x + r[i].w && y >= r[i].y && y < r[i].y + r[i].h) return 1;
    return 0;
}

static void test_rset(void)
{
    srand(5);
    for (int t = 0; t < 2000; t++) {
        rset_t s;
        rs_init(&s);
        box_t in[20];
        int n = 1 + rand() % 20;
        for (int i = 0; i < n; i++) {
            int x = rand() % TW, y = rand() % TH, w = 1 + rand() % (t % 3 ? 40 : 600), h = 1 + rand() % (t % 3 ? 40 : 400);
            in[i] = (box_t){ x, y, x + w, y + h };
            box_t c = in[i];
            box_clip(&c, TW, TH);
            in[i] = c;
            rs_add(&s, &in[i], TW, TH);
            CHECK(s.n <= SK_MAX_RECTS);
        }
        rect_t out[SK_MAX_RECTS];
        int m = rs_take(&s, out);
        CHECK(m <= SK_MAX_RECTS && s.n == 0);
        for (int i = 0; i < m; i++) {
            CHECK(out[i].x >= 0 && out[i].y >= 0 && out[i].x + out[i].w <= TW && out[i].y + out[i].h <= TH);
            CHECK(out[i].x % 8 == 0 && out[i].w % 8 == 0);
            CHECK(out[i].w > 0 && out[i].h > 0);
        }
        for (int i = 0; i < n; i++) /* every added pixel is in a pushed rectangle (corners and middle) */
            for (int k = 0; k < 5; k++) {
                int x = k & 1 ? in[i].x1 : in[i].x0, y = k & 2 ? in[i].y1 : in[i].y0;
                if (k == 4) x = (in[i].x0 + in[i].x1) / 2, y = (in[i].y0 + in[i].y1) / 2;
                CHECK(covered(out, m, x, y));
            }
    }
    rset_t s;
    rs_init(&s);
    box_t none;
    box_clear(&none);
    rs_add(&s, &none, TW, TH);
    CHECK(s.n == 0); /* an empty box adds nothing */
}

typedef struct {
    uint8_t *panel;
    long pushes, rects;
    double px;
    long maxrects;
    double maxpx;
} sim_t;

static void panel_apply(sim_t *sm, rig_t *r, const rect_t *rc, int n)
{
    for (int i = 0; i < n; i++)
        for (int y = rc[i].y; y < rc[i].y + rc[i].h; y++)
            memcpy(sm->panel + y * TS + rc[i].x / 8, r->fb + y * TS + rc[i].x / 8, (size_t)rc[i].w / 8);
}

/* One run of the loop: a pen report every 5 ms (a 200 Hz tablet), a push when due */
static void run_stroke(const char *name, float px_per_ms, int hover, double max_avg_px)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    sim_t sm = { malloc((size_t)TS * TH), 0, 0, 0, 0, 0 };
    rect_t out[SK_MAX_RECTS];
    CHECK(rig_flush(&r, r.now, out) == SK_FULL); /* the first screen is one full refresh */
    memcpy(sm.panel, r.fb, (size_t)TS * TH);
    float x = 150, y = 150, vx = px_per_ms * 0.8f, vy = px_per_ms * 0.6f;
    uint32_t t0 = r.now;
    int reports = 0;
    unsigned fl = hover ? APP_PEN_IN_RANGE : APP_PEN_TIP | APP_PEN_IN_RANGE;
    for (int i = 0; i < 400; i++) { /* 2 seconds */
        x += vx * 5, y += vy * 5;
        if (x < 100 || x > TW - 100) vx = -vx, x += vx * 10;
        if (y < 100 || y > TH - 100) vy = -vy, y += vy * 10;
        rig_pen(&r, (int)x, (int)y, fl, 20000 + (unsigned)(i * 97) % 40000);
        reports++;
        int n = sk_flush(&r.s, r.now, out);
        CHECK(n >= 0 && n <= SK_MAX_RECTS);
        if (n > 0) {
            sm.pushes++, sm.rects += n;
            if (n > sm.maxrects) sm.maxrects = n;
            double px = 0;
            for (int k = 0; k < n; k++) px += (double)out[k].w * out[k].h;
            sm.px += px;
            if (px > sm.maxpx) sm.maxpx = px;
            for (int k = 0; k < n; k++) CHECK(out[k].x % 8 == 0 && out[k].w % 8 == 0 && out[k].x >= 0 && out[k].x + out[k].w <= TW && out[k].y >= 0 && out[k].y + out[k].h <= TH);
            panel_apply(&sm, &r, out, n);
        }
    }
    rig_pen(&r, (int)x, (int)y, 0, 0); /* the pen leaves */
    for (int i = 0; i < 4; i++) { /* the last batch */
        r.now += SK_BATCH_MS;
        int n = sk_flush(&r.s, r.now, out);
        if (n == SK_FULL) memcpy(sm.panel, r.fb, (size_t)TS * TH); /* the legend left the page: a whole-page flash */
        else if (n > 0) panel_apply(&sm, &r, out, n);
    }
    double secs = (double)(r.now - t0) / 1000.0;
    /* every pixel that changed on the frame is on the panel */
    CHECK(!memcmp(sm.panel, r.fb, (size_t)TS * TH));
    /* bounded: no more pushes than the batch time allows, a handful of rectangles, and far less than the screen */
    CHECK(sm.pushes <= (long)(secs * 1000 / SK_BATCH_MS) + 1);
    CHECK(sm.maxrects <= SK_MAX_RECTS);
    CHECK(sm.px / (double)sm.pushes < max_avg_px); /* what a push holds follows the path of 40 ms of pen, not the screen */
    printf("  %-22s %5.0f px/s pen  %4.1f pushes/s  %4.1f rects/s  %8.0f px/s pushed (%.2f%% of the screen per push, max %.0f px)\n", name,
        (double)px_per_ms * 1000, (double)sm.pushes / secs, (double)sm.rects / secs, sm.px / secs, 100.0 * sm.px / (double)sm.pushes / (TW * TH), sm.maxpx);
    (void)reports;
    free(sm.panel);
    rig_end(&r);
}

static void test_stroke_rates(void)
{
    run_stroke("slow writing", 0.15f, 0, 6000);
    run_stroke("writing", 0.6f, 0, 6000);
    run_stroke("fast stroke", 3.0f, 0, 20000);
    run_stroke("very fast stroke", 8.0f, 0, 90000);
    run_stroke("hover, fast", 3.0f, 1, 16000); /* bigger cursor on purpose (owner 2026-11-01): was 12000 */
}

/* A push that comes too soon is held back: the batch time is kept */
static void test_batch_time(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    rect_t out[SK_MAX_RECTS];
    rig_flush(&r, r.now, out);
    rig_pen(&r, 500, 500, APP_PEN_TIP | APP_PEN_IN_RANGE, 30000);
    CHECK(sk_flush(&r.s, r.s.last_push + 3, out) == 0); /* 3 ms after the last push: wait */
    CHECK(sk_next_due(&r.s, r.s.last_push + 3) <= SK_BATCH_MS - 3);
    CHECK(sk_flush(&r.s, r.s.last_push + SK_BATCH_MS, out) > 0);
    CHECK(sk_flush(&r.s, r.now + 1000, out) == 0); /* nothing new: nothing pushed */
    rig_end(&r);
}

/* The glue's calls (skpush.c): the partial rectangles of a push as one call over every row they hold, flash halves
   as they are, the whole screen as one; per-rect mode one call each */
static void test_push_calls(void)
{
    srand(9);
    for (int t = 0; t < 2000; t++) {
        rect_t r[SK_MAX_RECTS];
        int n = 1 + rand() % SK_MAX_RECTS, flashes = 0;
        for (int i = 0; i < n; i++) {
            int x = (rand() % (TW / 8)) * 8, y = rand() % TH, w = 8 * (1 + rand() % ((TW - x) / 8)), h = 1 + rand() % (TH - y);
            r[i] = (rect_t){ (int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h, (int16_t)(t % 7 == 0 && i == 0) };
            flashes += r[i].flash;
        }
        sk_call_t c[SK_MAX_RECTS];
        int ink = t & 1, k = skp_calls(n, r, SKP_ONE_CALL, ink, c), parts = 0;
        CHECK(k == flashes + (n > flashes));
        for (int j = 0; j < k; j++) {
            CHECK(!c[j].whole && c[j].r.x >= 0 && c[j].r.y >= 0 && c[j].r.x + c[j].r.w <= TW && c[j].r.y + c[j].r.h <= TH);
            parts += c[j].how == (ink ? APP_REFRESH_INK : APP_REFRESH_PARTIAL);
            CHECK(c[j].how != APP_REFRESH_PARTIAL || !ink); /* with fast ink no live partial is left */
        }
        CHECK(parts == (n > flashes));
        for (int i = 0; i < n; i++) { /* every rectangle's area is in a call of its kind */
            int in = 0;
            for (int j = 0; j < k; j++)
                in |= (c[j].how == APP_REFRESH_FULL) == (r[i].flash != 0) && c[j].r.x <= r[i].x && c[j].r.y <= r[i].y && c[j].r.x + c[j].r.w >= r[i].x + r[i].w && c[j].r.y + c[j].r.h >= r[i].y + r[i].h;
            CHECK(in);
        }
        CHECK(skp_calls(n, r, SKP_PER_RECT, ink, c) == n);
    }
    sk_call_t c[SK_MAX_RECTS];
    CHECK(skp_calls(SK_FULL, 0, SKP_MODE, 1, c) == 1 && c[0].whole && c[0].how == APP_REFRESH_FULL);
    CHECK(skp_calls(0, 0, SKP_MODE, 1, c) == 0);
    CHECK(SKP_MODE == SKP_ONE_CALL); /* the app pushes one call a push (host/speed.c: a call costs about 200 ms however small) */
}

int main(void)
{
    test_rset();
    printf("fast-stroke simulation (a 200 Hz tablet, 1872 x 1404 panel):\n");
    test_stroke_rates();
    test_batch_time();
    test_push_calls();
    return CHECK_DONE("test_refresh");
}
