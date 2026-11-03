/* What a pen event costs the app, per tool and size, on a 200 Hz scribble: the CPU time of sk_pen and the frame's
   flush on this machine, scaled to the X. Owner's X 2026-11-02 (fw-lag28 pulse lines): the app task at 60 to 84 % of its
   core while drawing with the Brush, 255 reports a second, the firmware's pen queue dropping thousands of moves and
   "sometimes 10 s for a change to appear". The X has 3.9 ms a report at that rate; the bound leaves half of it.

   The scale: test/test_lag.c measured one toolbar paint at 0.87 ms on a PC and tens of ms on the X, so a host
   microsecond is taken as COST_X_RATIO microseconds on the X (an Xtensa LX7 at 240 MHz against a PC core). Built
   without the sanitizers (Makefile: cost), -O2 as the app is.

   `make cost` fails when a tool's cost per event is over test/cost.bounds; `make cost-table` prints them all.
   SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "check.h"
#include "util.h"

#define COST_X_RATIO 40
#define EVENTS 400 /* 2 s of a 200 Hz stream */
#define PX_PER_EVENT 15 /* 3 px/ms: a scribble (host/speed.c p_zig) */
#define EVENTS_PER_FRAME 6 /* the X's loop pushed about 40 times a second on fw-lag28 */

static double cpu_us(void)
{
    struct timespec t;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t);
    return (double)t.tv_sec * 1e6 + (double)t.tv_nsec / 1e3;
}

/* the host microseconds of one pen event with `brush` at size class `cls`, the frames included */
static double cost_us(int brush, int cls)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    sk_set_brush(&r.s, brush, r.now += 50);
    sk_set_size(&r.s, cls, r.now += 50);
    rect_t out[SK_MAX_RECTS];
    while (rig_frame(&r, out) != 0) {}
    int x = 300, y = 500, dx = PX_PER_EVENT, dy = 3;
    rig_pen(&r, x, y, APP_PEN_IN_RANGE, 0);
    double t0 = cpu_us();
    for (int i = 0; i < EVENTS; i++) {
        x += dx;
        y += dy;
        if (x > 1500 || x < 300) dx = -dx, x += 2 * dx;
        if (y > 900 || y < 400) dy = -dy, y += 2 * dy;
        rig_pen(&r, x, y, APP_PEN_TIP | APP_PEN_IN_RANGE, 40000);
        if (i % EVENTS_PER_FRAME == EVENTS_PER_FRAME - 1) rig_flush(&r, r.now, out);
    }
    double us = (cpu_us() - t0) / EVENTS;
    rig_pen(&r, x, y, APP_PEN_IN_RANGE, 0);
    rig_end(&r);
    return us;
}

static const char *cls_name(int cls) { return cls == 0 ? "small" : cls == 1 ? "medium" : "big"; }

/* test/cost.bounds: "<tool> <size>=<host us>" lines */
static double bound_for(const char *path, const char *tool, const char *cls)
{
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char line[128], key[64];
    snprintf(key, sizeof(key), "%s %s=", tool, cls);
    double b = -1;
    while (fgets(line, sizeof(line), f))
        if (!strncmp(line, key, strlen(key))) b = atof(line + strlen(key));
    fclose(f);
    return b;
}

int main(int argc, char **argv)
{
    const char *bounds = argc > 2 && !strcmp(argv[1], "--check") ? argv[2] : NULL;
    int bad = 0;
    printf("%-8s %-7s %9s %9s %s\n", "tool", "size", "host us", "X ms", bounds ? "bound us" : "");
    for (int b = 0; b < BR_N; b++) {
        if (brush_is_fill(b)) continue;
        for (int cls = 0; cls < BR_SIZES; cls++) {
            double us = cost_us(b, cls);
            double bound = bounds ? bound_for(bounds, brush_name(b), cls_name(cls)) : -1;
            int over = bounds && (bound < 0 || us > bound);
            printf("%-8s %-7s %9.1f %9.2f", brush_name(b), cls_name(cls), us, us * COST_X_RATIO / 1000);
            if (bounds) printf(" %8.0f%s", bound, over ? " OVER" : "");
            printf("\n");
            bad |= over;
        }
    }
    if (bad) fprintf(stderr, "cost: a tool is over test/cost.bounds (or has no bound)\n");
    return bad;
}
