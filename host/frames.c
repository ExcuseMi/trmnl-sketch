/* Three minutes of a 200 Hz Brush scribble at Big size through the app's frame loop (skframe.h) on a virtual clock,
   with the app's own compute costed as the X would pay it: the host CPU time of sk_pen and sk_frame times COST_X_RATIO
   (host/cost.c). Models the firmware's pen queue (usb_pen_queue: 512 moves, more are dropped) and the frame the app
   takes every SK_FRAME_MS. The ink's own panel time is not in it (host/speed.c has the panel); this is the app side:
   from a report's arrival to the frame that stamped and marked it.

   Owner 2026-11-02: "draw whatever happened in the last moment, consistently": the latency must stay bounded and no
   report may be dropped. `make frames` fails when p95 is over FRAMES_P95_MS or a report was dropped.

   Owner 2026-11-02: "for the cursor we only need to show the latest position each time": a second run hovers for the
   same three minutes (no tip). It fails when a frame drew the cursor more than once, the cursor is not at the last
   report of a hover frame, pushes a second exceed the frame rate, or the report-to-cursor p95 is over FRAMES_HOVER_P95_MS.
   The drawing run checks the first two as well.
   SPDX-License-Identifier: MIT */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "check.h"
#include "util.h"

#define COST_X_RATIO 40
#define HZ 200
#define SECS 180
#define QUEUE 512      /* the firmware's pen queue (usb_pen.h) */
#define FRAMES_P95_MS 100
#define FRAMES_HOVER_P95_MS 60
#define STROKE_MS 2000 /* a stroke, then a hover */
#define HOVER_MS 300

typedef struct { double t; int x, y, flags; unsigned p; } ev_t;

static double cpu_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    return (double)ts.tv_sec * 1e6 + (double)ts.tv_nsec / 1e3;
}

static int cmp(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }

static int run(int hover_only, int check)
{
    int n = HZ * SECS, i = 0;
    ev_t *ev = malloc(sizeof(ev_t) * (size_t)n);
    double *lat = malloc(sizeof(double) * (size_t)n);
    int x = 300, y = 500, dx = 15, dy = 3;
    for (int k = 0; k < n; k++) { /* a scribble at 3 px/ms, lifted every STROKE_MS for HOVER_MS */
        double t = 1000.0 + k * (1000.0 / HZ);
        int in_stroke = !hover_only && (int)(t - 1000.0) % (STROKE_MS + HOVER_MS) < STROKE_MS;
        if (in_stroke || hover_only) {
            x += dx, y += dy;
            if (x > 1500 || x < 300) dx = -dx, x += 2 * dx;
            if (y > 900 || y < 400) dy = -dy, y += 2 * dy;
        }
        ev[k] = (ev_t){ t, x, y, APP_PEN_IN_RANGE | (in_stroke ? APP_PEN_TIP : 0), in_stroke ? 40000u : 0u };
    }
    static rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    sk_set_brush(&r.s, BR_BRUSH, r.now += 50);
    sk_set_size(&r.s, 2, r.now += 50);
    rect_t out[SK_MAX_RECTS];
    double t = 1000;
    int drops = 0, frames = 0, max_q = 0, max_scale = 1, taken = 0;
    double max_cost = 0, sum_cost = 0;
    int multi_draw = 0, off_target = 0, hover_frames = 0, pushes = 0, max_pushes = 0, sec = -1, hq_bad = 0;
    while (i < n) {
        int np = rig_flush(&r, (uint32_t)t, out); /* the push returns at once (fast ink) */
        if ((int)(t / 1000) != sec) sec = (int)(t / 1000), pushes = 0;
        if (np && ++pushes > max_pushes) max_pushes = pushes;
        sk_tick(&r.s, (uint32_t)t);
        uint32_t wait = sk_wait_ms(&r.s, (uint32_t)t);
        if (wait > 500) wait = 500;
        if (ev[i].t > t) { /* the first poll waits for an event or the due time */
            if (ev[i].t > t + wait) {
                t += wait ? wait : 1;
                continue;
            }
            t = ev[i].t;
        }
        double frame_end = t + SK_FRAME_MS, c0 = cpu_us();
        int first = i, draws0 = r.s.cur_draws;
        int queued = 0;
        for (int k = i; k < n && ev[k].t <= t; k++) queued++; /* waiting in the firmware's queue when the frame starts */
        if (queued > max_q) max_q = queued;
        for (; i < n && ev[i].t <= frame_end; i++) {
            unsigned px, py;
            if (i - taken >= QUEUE) drops++; /* arrived with the queue full: the firmware dropped it */
            rig_pt(&r, ev[i].x, ev[i].y, &px, &py);
            if (ev[i].t > t) t = ev[i].t;
            double e0 = cpu_us();
            int q0 = r.s.frame.n, was_hover = !(ev[i].flags & APP_PEN_TIP) && !r.s.stroke;
            sk_pen(&r.s, (unsigned)ev[i].flags, px, py, ev[i].p, (uint32_t)t);
            if (was_hover && r.s.frame.n != q0) hq_bad++; /* a hover report never enters the stroke queue */
            if (getenv("FRAMES_V") && (cpu_us() - e0) * COST_X_RATIO / 1000.0 > 50)
                fprintf(stderr, "  event %d flags %x: %.0f ms, stroke now %d, queue %d\n", i, ev[i].flags,
                        (cpu_us() - e0) * COST_X_RATIO / 1000.0, r.s.stroke, r.s.frame.n);
        }
        taken = i;
        t = frame_end;
        double c1 = cpu_us();
        sk_frame(&r.s, (uint32_t)t);
        double cost = (cpu_us() - c0) * COST_X_RATIO / 1000.0; /* the X's milliseconds for this frame */
        double c2 = cpu_us();
        if (getenv("FRAMES_V") && cost > 40)
            fprintf(stderr, "t %.0f: %d points, scale %d, pen %.1f ms, frame %.1f ms, stroke %d\n", t, i - first, r.s.frame.scale,
                    (c1 - c0) * COST_X_RATIO / 1000.0, (cpu_us() - c1) * COST_X_RATIO / 1000.0, r.s.stroke);
        if (r.s.cur_draws - draws0 > 1) multi_draw++;
        if (i > first && !(ev[i - 1].flags & APP_PEN_TIP) && (ev[i - 1].flags & APP_PEN_IN_RANGE)) { /* the frame ended hovering */
            int ddx = r.s.cx - ev[i - 1].x, ddy = r.s.cy - ev[i - 1].y;
            hover_frames++;
            if (!r.s.cursor || ddx * ddx + ddy * ddy >= 9) off_target++;
        }
        sk_frame_cost(&r.s, (uint32_t)cost); /* the bake of an old stroke's slice runs in it: costed, not in `cost` */
        cost += (cpu_us() - c2) * COST_X_RATIO / 1000.0;
        t += cost;
        if (r.s.frame.scale > max_scale) max_scale = r.s.frame.scale;
        if (cost > max_cost) max_cost = cost;
        sum_cost += cost;
        frames++;
        for (int k = first; k < i; k++) lat[k] = t - ev[k].t;
    }
    qsort(lat, (size_t)n, sizeof(double), cmp);
    double p50 = lat[n / 2], p95 = lat[n * 95 / 100], max = lat[n - 1];
    printf("%s: frames %d, X ms a frame avg %.1f max %.1f, step scale max %d, queue max %d, dropped %d, latency ms p50 %.0f p95 %.0f max %.0f\n",
           hover_only ? "hover" : "draw", frames, sum_cost / frames, max_cost, max_scale, max_q, drops, p50, p95, max);
    printf("  cursor: frames hovering %d, drawn twice in a frame %d, not at the last report %d, pushes a second max %d, hover reports queued %d\n",
           hover_frames, multi_draw, off_target, max_pushes, hq_bad);
    int limit = hover_only ? FRAMES_HOVER_P95_MS : FRAMES_P95_MS;
    int bad = check && (p95 > limit || drops > 0 || multi_draw || off_target || hq_bad || (hover_only && max_pushes > 1000 / SK_FRAME_MS + 1));
    if (bad) fprintf(stderr, "frames (%s): p95 over %d ms, reports dropped, or the cursor drawn more than once a frame / off the last report / pushed too often\n", hover_only ? "hover" : "draw", limit);
    rig_end(&r);
    free(ev);
    free(lat);
    return bad;
}

int main(int argc, char **argv)
{
    int check = argc > 1 && !strcmp(argv[1], "--check");
    int bad = run(0, check);
    return run(1, check) || bad;
}
