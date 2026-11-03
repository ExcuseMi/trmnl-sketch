/* Drawing speed on the X, simulated: pen streams at 200 Hz go through Sketch's real controller and the glue's push
   (src/skpush.c) on a virtual clock, with screen_refresh costing what the X's driver costs.

   The model (trmnl-fw-files and its pinned FastEPD a5aa810; "guess" where not measured):
   | what | value | evidence |
   |---|---|---|
   | partial call blocks the app until the panel is done | yes | include/app_api.h:430 "blocks until done"; src/app_api_table.cpp:92-111 (panel_lock, synchronous partialUpdate) |
   | rows only, x and w ignored | yes | app_api_table.cpp:105 bbep.partialUpdate(true, r.y, r.y + r.h - 1) |
   | scans per partial | pp + 1 = 4 full-panel scans (all 1404 rows each, whatever the range) | display.cpp:177 setPasses(3, 3); FastEPD.inl:3619-3645 (every pass loops i < native_height), :3648 one neutral bbepClear |
   | one scan | 51 ms | tiny-paper docs/review/x-wake-time.md:209, measured on the X: 14 passes 768 ms, 6 passes 359 ms -> 51 ms a pass, rest 53 (= the neutral scan) |
   | rows in range | 0.034 ms a row (LUT per row, FastEPD.inl:3572-3606) | guess: the same log, rows range 768 -> 720 ms for 1404 rows |
   | panel power | stays on between calls (bKeepOn true) | app_api_table.cpp:105 |
   | a full refresh (clean) | 450 ms | guess; only the flashes use it, not ink |
   | APP_REFRESH_INK (API 2.14, "Fast ink") | 2 passes: (2+1)*51 ms + rows, the app is NOT blocked | trmnl-fw-files include/app_api.h:435 "returns at once", docs/apps.md |
   | INK while the panel is busy | one request waits; later ones merge into it (rows union); the panel runs it when free | docs/apps.md "Fast ink" |
   | a blocking call (full, plain partial) | waits for the running ink first | guess: one panel lock |
   | pen events | queued in order while the app is blocked, 512 deep | lib/trmnl/include/usb_pen.h:82-88 |
   | app CPU (ink drawing, the loop) | 0 | guess: not modelled |
   Shown = the end of the call whose rows hold the pen point (the passes darken it during the call). The app's loop is
   apps/trmnl-sketch/main.c's: push, tick, then up to 64 events (the first poll waits sk_next_due, at most 500 ms).

   build/speed: the table of variants. build/speed --check FILE: the default only, fails when a stream's p95 shown
   latency is more than 20 % over FILE's bound or a pixel that changed is not on the panel. SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 */
#include "check.h"
#include <math.h>
#include <stdio.h>
#include "util.h"
#include "skpush.h"

#ifndef PASSES
#define PASSES 3 /* the X sets 3 (display.cpp:177); -DPASSES=1 shows a faster driver */
#endif
#define INK_PASSES 2
#define SCAN_MS 51.0
#define PI 3.14159265358979
#define ROW_MS 0.034
#define FULL_MS 450.0
#define MAXEV 8192
#define MAXCALLS 4096

typedef struct {
    uint32_t t;
    int x, y; /* page pixel */
    unsigned flags, pressure;
} pev_t;

typedef struct {
    pev_t ev[MAXEV];
    int n;
} stream_t;

static void add(stream_t *s, uint32_t t, double x, double y, unsigned fl, unsigned p)
{
    if (s->n < MAXEV) s->ev[s->n++] = (pev_t){ t, (int)lround(x), (int)lround(y), fl, p };
}
#define TIP (APP_PEN_TIP | APP_PEN_IN_RANGE)
#define HOV APP_PEN_IN_RANGE

/* The pen comes in range at a point, hovers 100 ms, then draws path(u) for ms milliseconds, then lifts and hovers 100 ms */
typedef void (*path_fn)(double u, double *x, double *y, const double *a);
static uint32_t stroke(stream_t *s, uint32_t t, int ms, path_fn f, const double *a)
{
    double x, y;
    f(0, &x, &y, a);
    for (int k = 0; k < 20; k++, t += 5) add(s, t, x - 20 + k, y - 20 + k, HOV, 0);
    for (int k = 0; k <= ms / 5; k++, t += 5) {
        f((double)k * 5 / ms, &x, &y, a);
        add(s, t, x, y, TIP, 22000 + (unsigned)(9000 * sin(k * 0.07)));
    }
    for (int k = 0; k < 20; k++, t += 5) add(s, t, x + k, y - k, HOV, 0);
    return t;
}
static void p_word(double u, double *x, double *y, const double *a) /* cursive loops, about 0.35 px/ms */
{
    double s = u * a[2] / 1000;
    *x = a[0] + 60 * s + 18 * sin(2 * PI * 3 * s), *y = a[1] - 18 * cos(2 * PI * 3 * s);
}
static void p_zig(double u, double *x, double *y, const double *a) /* a scribble in a 300 x 200 box, about 3 px/ms */
{
    double s = u * a[2] / 1000;
    *x = a[0] + 150 + 150 * sin(2 * PI * 2.1 * s), *y = a[1] + 100 + 100 * sin(2 * PI * 3.4 * s + 0.5);
}
static void p_curve(double u, double *x, double *y, const double *a) /* across the page, about 1.5 px/ms */
{
    *x = a[0] + 1300 * u, *y = a[1] + 220 * sin(2 * PI * 1.5 * u + a[2]);
}

static void make(int which, stream_t *s)
{
    s->n = 0;
    uint32_t t = 0;
    if (which == 0) /* slow writing: 3 lines of 4 words */
        for (int l = 0; l < 3; l++)
            for (int w = 0; w < 4; w++) t = stroke(s, t, 1200, p_word, (double[]){ 250.0 + w * 330, 400.0 + l * 200, 1200 }) + 150;
    else if (which == 1) /* fast scribble: 3 of 1.5 s */
        for (int k = 0; k < 3; k++) t = stroke(s, t, 1500, p_zig, (double[]){ 300.0 + k * 420, 500, 1500 }) + 200;
    else /* long curves: 3 of 2 s */
        for (int k = 0; k < 3; k++) t = stroke(s, t, 2000, p_curve, (double[]){ 280, 450.0 + k * 220, k * 1.3 }) + 300;
}
static const char *const names[3] = { "slow writing", "fast scribble", "long curves" };

typedef struct {
    double start, end;
    int y0, y1; /* rows on the glass; y0 < 0: none */
} call_t;

typedef struct {
    double push_med, push_p95, push_max, shown_med, shown_p95, shown_max, calls_s, rows_call;
    int ok;
} result_t;

static int cmpd(const void *a, const void *b) { return *(const double *)a < *(const double *)b ? -1 : *(const double *)a > *(const double *)b; }
static double pct(double *v, int n, double p) { return n ? v[(int)((n - 1) * p + 0.5)] : 0; }

static call_t calls[MAXCALLS];
static double deliv[MAXEV];
static double busy_until; /* the panel is busy with ink until then */
static int pend = -1;     /* index of the waiting (merged) ink call, -1 none */

/* The glue: hands the calls to the "panel" (the rows copied from the framebuffer), returns the app's time. Ink returns
   at once; a blocking call waits for the panel first */
static double glue(rig_t *r, uint8_t *panel, int n, const rect_t *out, int mode, int ink, double t, int *nc)
{
    sk_call_t c[SK_MAX_RECTS];
    int k = n ? skp_calls(n, out, mode, ink, c) : 0;
    for (int i = 0; i < k; i++) {
        int y0 = c[i].whole ? 0 : c[i].r.y, y1 = c[i].whole ? TH - 1 : c[i].r.y + c[i].r.h - 1;
        if (c[i].how == APP_REFRESH_INK) {
            if (pend >= 0 && t < calls[pend].start) { /* merge into the waiting request */
                call_t *w = &calls[pend];
                y0 = y0 < w->y0 ? y0 : w->y0, y1 = y1 > w->y1 ? y1 : w->y1;
                w->y0 = y0, w->y1 = y1;
                w->end = w->start + (INK_PASSES + 1) * SCAN_MS + ROW_MS * (y1 - y0 + 1);
                busy_until = w->end;
            } else if (*nc < MAXCALLS) {
                double st = busy_until > t ? busy_until : t, e = st + (INK_PASSES + 1) * SCAN_MS + ROW_MS * (y1 - y0 + 1);
                pend = st > t ? *nc : -1;
                calls[(*nc)++] = (call_t){ st, e, y0, y1 };
                busy_until = e;
            }
            memcpy(panel + (size_t)y0 * TS, r->fb + (size_t)y0 * TS, (size_t)(y1 - y0 + 1) * TS);
            sk_lat_push(&r->s, 0); /* an ink call returns at once */
            continue;
        }
        if (t < busy_until) t = busy_until;
        double ms = c[i].how == APP_REFRESH_PARTIAL ? (PASSES + 1) * SCAN_MS + ROW_MS * (y1 - y0 + 1) : FULL_MS;
        memcpy(panel + (size_t)y0 * TS, r->fb + (size_t)y0 * TS, (size_t)(y1 - y0 + 1) * TS);
        if (*nc < MAXCALLS) calls[(*nc)++] = (call_t){ t, t + ms, y0, y1 };
        t += ms;
        busy_until = t;
        if (c[i].how == APP_REFRESH_PARTIAL) sk_lat_push(&r->s, (uint32_t)ms); /* a plain partial blocks the app for its whole time */
    }
    return t;
}

/* One loop step of main.c at virtual time *t (ms); events from *i on arrive at base + ev.t */
static void step(rig_t *r, uint8_t *panel, const stream_t *st, int *i, double base, double *t, int mode, int ink, int *nc)
{
    rect_t out[SK_MAX_RECTS];
    do *t = glue(r, panel, sk_flush(&r->s, (uint32_t)*t, out), out, mode, ink, *t, nc); while (sk_flash_busy(&r->s));
    sk_tick(&r->s, (uint32_t)*t);
    uint32_t wait = sk_wait_ms(&r->s, (uint32_t)*t);
    if (wait > 500) wait = 500;
    int have = st && *i < st->n;
    double next = have ? base + st->ev[*i].t : 1e18;
    if (next > *t) { /* the first poll waits */
        if (next > *t + wait) {
            *t += wait ? wait : 1;
            return;
        }
        *t = next;
    }
    double frame_end = *t + SK_FRAME_MS; /* as main.c: the frame takes every event of SK_FRAME_MS after the first */
    int took = 0;
    for (; st && *i < st->n && base + st->ev[*i].t <= frame_end; (*i)++, took++) {
        const pev_t *e = &st->ev[*i];
        unsigned x, y;
        rig_pt(r, e->x, e->y, &x, &y);
        if (base + e->t > *t) *t = base + e->t;
        sk_pen(&r->s, e->flags, x, y, e->pressure, (uint32_t)*t);
        deliv[*i] = *t;
    }
    if (took) sk_frame(&r->s, (uint32_t)*t);
}

static result_t run(const stream_t *st, int mode, int ink, int batch, int verbose)
{
    static rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    r.s.batch_ms = batch;
    uint8_t *panel = malloc((size_t)TS * TH);
    memset(panel, 0xff, (size_t)TS * TH);
    double t = 1000;
    int nc = 0, i = 0;
    busy_until = 0, pend = -1;
    stream_t *warm = malloc(sizeof(*warm)); /* a first stroke takes the start legend away; its clean-up flash */
    warm->n = 0;
    stroke(warm, 0, 300, p_curve, (double[]){ 200, 800, 0 });
    double wb = t;
    while (i < warm->n || t < wb + 7000) step(&r, panel, warm, &i, wb, &t, mode, ink, &nc);
    free(warm);
    nc = 0, i = 0, pend = -1;
    double base = t + 3;
    while (i < st->n || t < base + st->ev[st->n - 1].t + 2000) step(&r, panel, st, &i, base, &t, mode, ink, &nc);
    for (int k = 0; k < 20; k++) step(&r, panel, 0, &i, base, &t, mode, ink, &nc); /* settle */
    result_t res;
    memset(&res, 0, sizeof(res));
    res.ok = !memcmp(panel, r.fb, (size_t)TS * TH); /* every changed pixel reached the glass */
    double *lp = malloc(sizeof(double) * MAXEV), *ls = malloc(sizeof(double) * MAXEV);
    int m = 0, c = 0;
    for (int e = 0; e < st->n; e++) {
        if (!(st->ev[e].flags & APP_PEN_TIP)) continue;
        while (c < nc && calls[c].start < deliv[e]) c++; /* calls started before it was read: not its ink */
        int k = c;
        while (k < nc && !(st->ev[e].y >= calls[k].y0 && st->ev[e].y <= calls[k].y1)) k++;
        if (k == nc) {
            res.ok = 0;
            continue;
        }
        lp[m] = calls[k].start - (base + st->ev[e].t), ls[m] = calls[k].end - (base + st->ev[e].t), m++;
    }
    qsort(lp, (size_t)m, sizeof(double), cmpd);
    qsort(ls, (size_t)m, sizeof(double), cmpd);
    res.push_med = pct(lp, m, 0.5), res.push_p95 = pct(lp, m, 0.95), res.push_max = m ? lp[m - 1] : 0;
    res.shown_med = pct(ls, m, 0.5), res.shown_p95 = pct(ls, m, 0.95), res.shown_max = m ? ls[m - 1] : 0;
    double secs = (st->ev[st->n - 1].t + 2000) / 1000.0, rows = 0;
    int during = 0;
    for (int k = 0; k < nc; k++)
        if (calls[k].start <= base + st->ev[st->n - 1].t + 2000) during++, rows += calls[k].y1 - calls[k].y0 + 1;
    res.calls_s = during / secs, res.rows_call = during ? rows / during : 0;
    if (verbose) {
        char lt[80];
        lat_text(lt, sizeof(lt), &r.s.lat_ev, &r.s.lat_push);
        printf("    %d pen events with the tip down, %d calls; the app's own log (sklat.h), last %d samples: %s\n", m, nc, SK_LAT_N, lt);
    }
    free(lp), free(ls), free(panel);
    rig_end(&r);
    return res;
}

static void row(const char *what, const result_t *x)
{
    printf("  %-26s pushed %4.0f/%4.0f/%4.0f  shown %4.0f/%4.0f/%4.0f ms  %4.1f calls/s  %5.0f rows/call%s\n", what, x->push_med, x->push_p95,
        x->push_max, x->shown_med, x->shown_p95, x->shown_max, x->calls_s, x->rows_call, x->ok ? "" : "  PIXELS MISSING");
}

int main(int argc, char **argv)
{
    static stream_t st;
    int check = argc > 2 && !strcmp(argv[1], "--check");
    double bound[3] = { 0, 0, 0 };
    if (check) {
        FILE *f = fopen(argv[2], "r");
        char line[256];
        if (!f) return fprintf(stderr, "speed: no bounds file %s\n", argv[2]), 1;
        while (fgets(line, sizeof(line), f)) /* "name=ms"; # starts a comment line */
            for (int k = 0; k < 3 && line[0] != '#'; k++) {
                size_t l = strlen(names[k]);
                if (!strncmp(line, names[k], l) && line[l] == '=') bound[k] = atof(line + l + 1);
            }
        fclose(f);
    }
    printf("Sketch on the X, simulated (partial call %.0f ms + %.3f ms a row; median/p95/max from the pen event)\n", (PASSES + 1) * SCAN_MS, ROW_MS);
    for (int w = 0; w < 3; w++) {
        make(w, &st);
        printf("%s (%d events, %.1f s)\n", names[w], st.n, st.ev[st.n - 1].t / 1000.0);
        if (!check) {
            result_t a = run(&st, SKP_PER_RECT, 0, 40, 0);
            row("before: a call a rect, 40", &a);
            static const int win[] = { 0, 10, 20, 30, 40 }, iw[] = { 8, 16, 24, 32, 40, 60 };
            for (unsigned k = 0; k < sizeof(iw) / sizeof(iw[0]); k++) {
                char lab[40];
                snprintf(lab, sizeof(lab), "fast ink (2.14), window %d", iw[k]);
                result_t ik = run(&st, SKP_ONE_CALL, 1, iw[k], 0);
                row(lab, &ik);
            }
            for (unsigned k = 0; k < sizeof(win) / sizeof(win[0]); k++) {
                char lab[40];
                snprintf(lab, sizeof(lab), "one call, window %d ms", win[k]);
                result_t b = run(&st, SKP_ONE_CALL, 0, win[k], 0);
                row(lab, &b);
            }
        }
        result_t d = run(&st, SKP_MODE, 1, SK_BATCH_MS, !check);
        row("default", &d);
        CHECK(d.ok);
        if (check) {
            CHECK(bound[w] > 0);
            if (d.shown_p95 > bound[w] * 1.2) printf("  p95 shown %.0f ms is over the bound %.0f ms + 20 %%\n", d.shown_p95, bound[w]);
            CHECK(d.shown_p95 <= bound[w] * 1.2);
        }
    }
    fflush(stdout);
    return CHECK_DONE("speed");
}
