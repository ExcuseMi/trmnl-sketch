/* Undo and redo on the X, costed (host/cost.c's COST_X_RATIO): owner 2026-11-03, "I did a big fill and then undo and it
   all got so laggy. Took like minutes". Every undo rebuilt the page from the base image and stamped every stroke again.

   Without arguments: the cost of one undo press after N Big Brush strokes, with and without a full-page fill first.
   --check: 60 Big Brush strokes and 3 full-page fills, then 40 undo and 40 redo presses (tablet buttons 1 and 2) at 5 a
   second through the app's frame loop (the glue's order: flush, tick, the frame's events, sk_frame, sk_frame_cost). It
   fails when a press takes over UNDO_PRESS_MS from its report to the end of the frame that showed it, or when the page
   after a press is not the page as it was drawn at that point of the history (a snapshot taken while drawing).
   SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "check.h"
#include "util.h"

#define COST_X_RATIO 40
#define UNDO_PRESS_MS 300
#define STROKE_EVENTS 200 /* a second of a 200 Hz Brush scribble */
#define ACTIONS 63        /* 60 strokes and 3 fills */
#define PRESS_MS 200      /* 5 presses a second */

static double cpu_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    return (double)ts.tv_sec * 1e6 + (double)ts.tv_nsec / 1e3;
}

/* a Big Brush scribble inside x 300..1500, y 400..900 (the rest of the page stays one region for a fill) */
static void scribble(rig_t *r, int k)
{
    sk_set_brush(&r->s, BR_BRUSH, r->now += 50);
    sk_set_size(&r->s, 2, r->now += 50);
    sk_set_shade(&r->s, k % 3 == 2 ? SH_WHITE : k % 3 ? SH_GREY : SH_BLACK, r->now += 50);
    int x = 300 + (k * 97) % 1100, y = 400 + (k * 53) % 450, dx = 15 - (k % 5), dy = 3 + k % 4;
    rect_t out[SK_MAX_RECTS];
    rig_pen(r, x, y, APP_PEN_IN_RANGE, 0);
    for (int i = 0; i < STROKE_EVENTS; i++) {
        x += dx, y += dy;
        if (x > 1500 || x < 300) dx = -dx, x += 2 * dx;
        if (y > 900 || y < 400) dy = -dy, y += 2 * dy;
        rig_pen(r, x, y, APP_PEN_TIP | APP_PEN_IN_RANGE, 40000);
        if (i % 6 == 5) rig_flush(r, r->now, out);
    }
    rig_pen(r, x, y, APP_PEN_IN_RANGE, 0);
    rig_flush(r, r->now += 20, out);
}

/* a full-page fill at the left edge: the tap asks, the second tap fills */
static void fill_page(rig_t *r, int shade)
{
    sk_set_brush(&r->s, BR_FILL, r->now += 50);
    sk_set_shade(&r->s, shade, r->now += 50);
    for (int k = 0; k < 2; k++) {
        rig_pen(r, 100, 700, APP_PEN_IN_RANGE, 0);
        rig_pen(r, 100, 700, APP_PEN_TIP | APP_PEN_IN_RANGE, 30000);
        rig_pen(r, 100, 700, APP_PEN_IN_RANGE, 0);
    }
}

static void quiet(rig_t *r) /* the toasts gone and the page saved: nothing pending before a measurement */
{
    rect_t out[SK_MAX_RECTS];
    r->now += 20000;
    for (int k = 0; k < 4; k++) sk_tick(&r->s, r->now += 1000), rig_flush(r, r->now, out);
}

/* the X's ms of one undo press (and its frame) after n strokes, a full-page fill first when `fill` */
static double one_undo_ms(int n, int fill)
{
    static rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    if (fill) fill_page(&r, SH_GREY);
    for (int k = 0; k < n; k++) scribble(&r, k);
    quiet(&r);
    double c0 = cpu_us();
    sk_pen_key(&r.s, 1, 1, r.now += 10);
    sk_pen_key(&r.s, 0, 1, r.now += 10);
    sk_frame(&r.s, r.now);
    double ms = (cpu_us() - c0) * COST_X_RATIO / 1000.0;
    rig_end(&r);
    return ms;
}

static uint8_t snap[ACTIONS + 1][TS * TH];

static int presses(void)
{
    static rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    memcpy(snap[0], r.s.page.px, TS * TH);
    uint32_t pool_max = 0;
    for (int a = 1, k = 0; a <= ACTIONS; a++) {
        if (a % 21 == 0) {
            fill_page(&r, a == 21 ? SH_GREY : a == 42 ? SH_WHITE : SH_BLACK);
            if (!memcmp(snap[a - 1], r.s.page.px, TS * TH)) fprintf(stderr, "fill %d did nothing\n", a), check_fail++; /* the fill happened */
        } else scribble(&r, k++);
        memcpy(snap[a], r.s.page.px, TS * TH);
        uint32_t d = hist_delta_bytes(&r.s.hist);
        if (d > pool_max) pool_max = d;
    }
    printf("undo: deltas in the pool max %u KB of %u KB, the last stroke's %u KB\n", pool_max / 1024, r.s.hist.pool_cap / 1024,
           r.s.hist.st_cur ? r.s.hist.st[r.s.hist.st_cur - 1].dlen / 1024 : 0);
    quiet(&r);
    int n = 80, pos = ACTIONS, done = 0, bad_px = 0, over = 0;
    double t = r.now, max_ms = 0, sum_ms = 0, t0 = r.now + 100;
    rect_t out[SK_MAX_RECTS];
    while (done < n) {
        double c0 = cpu_us();
        rig_flush(&r, (uint32_t)t, out);
        sk_tick(&r.s, (uint32_t)t);
        t += (cpu_us() - c0) * COST_X_RATIO / 1000.0;
        double due = t0 + done * PRESS_MS; /* the next press */
        if (due > t) t = due;
        double frame_end = t + SK_FRAME_MS;
        int first = done;
        c0 = cpu_us();
        for (; done < n && t0 + done * PRESS_MS <= frame_end; done++) {
            unsigned key = done < 40 ? 1u : 2u;
            uint32_t at = (uint32_t)(t0 + done * PRESS_MS);
            sk_pen_key(&r.s, key, key, at);
            sk_pen_key(&r.s, 0, key, at + 1);
            pos += done < 40 ? -1 : 1;
        }
        t = frame_end;
        sk_frame(&r.s, (uint32_t)t);
        sk_frame_cost(&r.s, 1);
        t += (cpu_us() - c0) * COST_X_RATIO / 1000.0;
        for (int k = first; k < done; k++) {
            double ms = t - (t0 + k * PRESS_MS);
            if (ms > max_ms) max_ms = ms;
            sum_ms += ms;
            over += ms > UNDO_PRESS_MS;
        }
        if (memcmp(r.s.page.px, snap[pos], TS * TH)) bad_px++;
    }
    printf("undo: 60 Brush strokes + 3 page fills, 40 undo + 40 redo at 5/s: press to shown ms avg %.0f max %.0f, over %d ms %d, pages not as drawn %d\n",
           sum_ms / n, max_ms, UNDO_PRESS_MS, over, bad_px);
    rig_end(&r);
    int bad = over || bad_px || check_fail;
    if (bad) fprintf(stderr, "undo: a press over %d ms, or a page after undo/redo not the page as drawn\n", UNDO_PRESS_MS);
    return bad;
}

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "--check")) return presses();
    printf("%-8s %12s %12s\n", "strokes", "undo X ms", "after a fill");
    static const int ns[] = { 1, 10, 24, 47 };
    for (int i = 0; i < 4; i++) printf("%-8d %12.0f %12.0f\n", ns[i], one_undo_ms(ns[i], 0), one_undo_ms(ns[i], 1));
    return presses();
}
