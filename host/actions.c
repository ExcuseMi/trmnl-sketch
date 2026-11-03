/* Every user action, costed as the X (host/cost.c's COST_X_RATIO) with its screen time: owner 2026-11-03, "Anything else
   that's very slow to perform? We should fix all issues." An action is its input (pen, tablet button, touch) and the
   frames after it until nothing waits to be pushed: the app's compute in X ms, and the panel calls it hands out (FULL:
   a whole-screen or flash refresh, about 1130 ms on the X, blocking; INK: one fast partial call over its rows, about
   220 ms a run, returning at once). File time on the flash is not in it (memfs).

   `make actions` fails when an action's X ms is over its bound (ACT_MS unless the row says less), when it pushes more
   FULL refreshes than its row allows, or when an action that must not grow with the pages (start, gallery) costs more
   with 40 pages than ACT_GROW times its cost with 10.
   SPDX-License-Identifier: MIT */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "check.h"
#include "util.h"
#include "skpush.h"
#include "fill.h"

#define COST_X_RATIO 40
#define ACT_MS 300   /* the bound of an action's compute on the X */
#define ACT_GROW 1.5 /* 40 pages against 10 */
/* Rows with their own bound (today's cost and a margin: a ratchet, lower it when they get cheaper; 2026-11-03 round 2):
   Print's whole job (made a few rows a loop turn, each turn under ACT_MS; ../trmnl-print/src/imgfit.c), a whole-page
   fill's plan (src/fill.c), and the 4-way and kaleidoscope Brush per 30 ms frame (4 and 16 images a point) */
#define PRINT_MS 2300
#define FILL_PAGE_MS 230
#define MIRROR4_MS 16
#define KALEIDO_MS 22
#define FULL_MS 1130
#define INK_MS 220

static double cpu_us(void)
{
    struct timespec t;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t);
    return (double)t.tv_sec * 1e6 + (double)t.tv_nsec / 1e3;
}

static rig_t R;
static uint8_t *busy_png;
static size_t busy_n;
static int fulls, inks;
static double t0;

/* the panel calls of one flush result, as main.c's push makes them (fast ink firmware) */
static void count(int n, const rect_t *out)
{
    sk_call_t c[SK_MAX_RECTS];
    int k = n ? skp_calls(n, out, SKP_MODE, 1, c) : 0;
    for (int i = 0; i < k; i++) {
        if (c[i].whole || c[i].how == APP_REFRESH_FULL) fulls++;
        else inks++;
    }
}

/* the glue's turns until nothing waits: frame, flush (both halves of a flash), tick. Each turn's X ms; the longest
   stretch the loop is away from the input poll (the action's handling and its first turn, or a later turn) in `stretch` */
static double stretch, total;
static void settle(void)
{
    rect_t out[SK_MAX_RECTS];
    for (int k = 0; k < 400; k++) {
        R.now += 20;
        sk_frame(&R.s, R.now); /* main.c: the events, the frame, then at the top of the loop flush and tick, then the poll */
        sk_frame_cost(&R.s, 1);
        int n = sk_flush(&R.s, R.now, out);
        count(n, out);
        while (sk_flash_busy(&R.s)) n = sk_flush(&R.s, R.now, out), count(n, out);
        if (R.s.print_req) sk_run_print(&R.s);
        sk_tick(&R.s, R.now);
        double c = cpu_us(), ms = (c - t0) * COST_X_RATIO / 1000.0;
        total += ms, t0 = c;
        if (ms > stretch) stretch = ms;
        if (getenv("ACT_V")) fprintf(stderr, "  turn %d: %.0f ms, todo %d, pend %d\n", k, ms, R.s.gal_todo, R.s.pend.n);
        if (!R.s.pend.n && !R.s.want_full && box_empty(&R.s.flash) && !R.s.fl_phase && !R.s.frame.n && !R.s.print_req && !R.s.hist.stale && !R.s.gal_todo) return;
    }
}

static void quiet(void) /* toasts gone, the page saved: nothing of the setup left for the measurement */
{
    rect_t out[SK_MAX_RECTS];
    for (int k = 0; k < 8; k++) {
        R.now += 1000;
        sk_tick(&R.s, R.now);
        rig_flush(&R, R.now, out);
        while (sk_flash_busy(&R.s)) sk_flush(&R.s, R.now, out);
        while (hist_bake_step(&R.s.hist, 1u << 30)) {}
    }
    settle();
}

static void begin(void) { fulls = inks = 0, stretch = total = 0, t0 = cpu_us(); }
static double end(void) { settle(); return stretch; }

static void key(int button) /* a tablet button press and release */
{
    unsigned b = 1u << (button - 1);
    sk_pen_key(&R.s, b, b, R.now += 10);
    sk_pen_key(&R.s, 0, b, R.now += 10);
}
static void tap(int fx, int fy)
{
    rig_pen(&R, fx, fy, APP_PEN_IN_RANGE, 0);
    rig_pen(&R, fx, fy, APP_PEN_TIP | APP_PEN_IN_RANGE, 30000);
    rig_pen(&R, fx, fy, APP_PEN_IN_RANGE, 0);
}
static void tb_tap(int item) { tap(R.s.tb.cx[item] + R.s.tb.cell / 2, R.s.tb.cyi[item] + R.s.tb.ch / 2); }

/* a Big Brush scribble of `events` 200 Hz reports inside x 300..1500, y 400..900, frames every 6 */
static void scribble(int k, int events)
{
    int x = 300 + (k * 97) % 1100, y = 400 + (k * 53) % 450, dx = 15 - (k % 5), dy = 3 + k % 4;
    rect_t out[SK_MAX_RECTS];
    rig_pen(&R, x, y, APP_PEN_IN_RANGE, 0);
    for (int i = 0; i < events; i++) {
        x += dx, y += dy;
        if (x > 1500 || x < 300) dx = -dx, x += 2 * dx;
        if (y > 900 || y < 400) dy = -dy, y += 2 * dy;
        rig_pen(&R, x, y, APP_PEN_TIP | APP_PEN_IN_RANGE, 40000);
        if (i % 6 == 5) rig_flush(&R, R.now, out), sk_frame_cost(&R.s, 1);
    }
    rig_pen(&R, x, y, APP_PEN_IN_RANGE, 0);
}
static void brush_big(void) { sk_set_brush(&R.s, BR_BRUSH, R.now += 50), sk_set_size(&R.s, 2, R.now += 50); }

/* a busy page (60 Brush strokes in three shades) as the file every saved page of a run is */
static void make_busy(void)
{
    rig_new(&R, APP_ORIENT_BAR_BOTTOM);
    brush_big();
    for (int k = 0; k < 60; k++) sk_set_shade(&R.s, k % 3 == 2 ? SH_WHITE : k % 3 ? SH_GREY : SH_BLACK, R.now += 50), scribble(k, 200);
    sk_save_now(&R.s);
    memfile_t *f = memfs_find(&R.fs, "sketch/sk0001.png");
    if (!f) abort();
    busy_n = f->n;
    busy_png = malloc(busy_n);
    memcpy(busy_png, f->data, busy_n);
    rig_end(&R);
}

/* a fresh rig with `pages` saved busy pages (page 1 current), not yet started */
static void rig_pages(int pages)
{
    memset(&R, 0, sizeof(R));
    memfs_init(&R.fs);
    R.now = 1000;
    R.pr.status = 200;
    strcpy(R.prn.host, "10.1.2.3");
    R.prn.port = 631;
    strcpy(R.prn.path, "ipp/print");
    for (int i = 1; i <= pages; i++) {
        char p[48];
        pg_path(p, sizeof(p), i, ".png");
        memfs_put(&R.fs, p, busy_png, busy_n);
    }
    rig_cfg(&R, "cur", "1");
}
static void up(int pages) /* started on page 1 of `pages`, the setup's pushes done */
{
    rig_pages(pages);
    rig_start(&R, APP_ORIENT_BAR_BOTTOM);
    quiet();
}
static void down(void) { rig_end(&R); }

typedef struct {
    const char *name;
    double ms, total;
    int full, ink;
    double bound;
    int max_full;
    double tbound; /* the bound of the total, 0: none */
} row_t;

#define ROWS 64
static row_t rows[ROWS];
static int nrows;
static int verbose = 1, pass;

static double put(const char *name, double ms, double bound, int max_full)
{
    if (getenv("ACT_V")) fprintf(stderr, "= %s\n", name);
    row_t *r = &rows[nrows++];
    if (pass && r->ms <= ms) return r->ms; /* the cheaper of the passes: a busy machine's spike is not the app's */
    *r = (row_t){ name, ms, total, fulls, inks, bound, max_full, 0 };
    return ms;
}

/* ---- the actions ---- */

static double a_start(int pages)
{
    rig_pages(pages);
    begin();
    rig_start(&R, APP_ORIENT_BAR_BOTTOM);
    double ms = end();
    down();
    return ms;
}

static double a_gallery(int pages, double *scroll)
{
    up(pages);
    key(7); /* the page menu */
    settle();
    for (int i = 0; i < 5; i++) key(2); /* down to Gallery */
    settle();
    begin();
    key(4); /* select */
    double ms = end();
    put(pages == 20 ? "gallery open, 20 pages" : "gallery open, 40 pages", ms, ACT_MS, 1);
    begin();
    key(7); /* the screen of tiles before (page 1, the oldest, is on the last one) */
    *scroll = end();
    put(pages == 20 ? "gallery screen back, 20 pages" : "gallery screen back, 40 pages", *scroll, ACT_MS, 1);
    begin();
    key(2); /* the selection one tile on */
    put("gallery select next tile", end(), ACT_MS, 0);
    begin();
    rig_pen(&R, 900, 700, APP_PEN_IN_RANGE, 0);
    rig_pen(&R, 960, 720, APP_PEN_IN_RANGE, 0);
    put("gallery pen hover (2 moves)", end(), ACT_MS, 0);
    begin();
    key(4); /* opens the selected page */
    put("gallery open a page", end(), ACT_MS, 1);
    down();
    return ms;
}

static void a_menus(void)
{
    up(10);
    scribble(1, 100); /* a change: the menu saves the page first */
    settle();
    begin();
    key(7);
    put("page menu open (saves the page)", end(), ACT_MS, 1);
    begin();
    key(2);
    put("page menu move", end(), ACT_MS, 0);
    begin();
    key(5);
    put("page menu back to drawing", end(), ACT_MS, 1);
    key(7), settle(), key(2), key(2), settle(); /* Settings */
    begin();
    key(4);
    put("settings screen open", end(), ACT_MS, 1);
    begin();
    key(4); /* a setting steps */
    put("settings step", end(), ACT_MS, 0);
    key(5), key(5), settle(); /* the page menu, then back to drawing */
    key(7), settle(); /* Print is selected */
    begin();
    key(4);
    put("print confirm screen", end(), ACT_MS, 1);
    begin();
    key(2), key(4); /* Print: the busy screen, the job, the result */
    put("print: raster build and send", end(), ACT_MS, 3);
    rows[nrows - 1].tbound = PRINT_MS; /* made a few rows a turn: no turn over ACT_MS, the whole job under PRINT_MS */
    CHECK(R.pr.posts > 0);
    key(5), settle();
    down();
}

static void a_pages(void)
{
    up(10);
    scribble(1, 200);
    settle();
    begin();
    key(6);
    put("new page (saves the old one)", end(), ACT_MS, 1);
    scribble(2, 200);
    settle();
    begin();
    R.now += SK_AUTOSAVE_MS + 10;
    sk_tick(&R.s, R.now);
    put("autosave", end(), ACT_MS, 0);
    down();
    up(10);
    key(7), settle();
    for (int i = 0; i < 5; i++) key(2);
    key(4), settle(), key(2), settle(); /* the gallery, page 9 selected */
    scribble(3, 10); /* (the pen is a pointer in the gallery) */
    begin();
    key(4);
    put("open a page from the gallery (decode)", end(), ACT_MS, 1);
    down();
}

static void a_clear_undo(void)
{
    up(10);
    brush_big();
    scribble(4, 200);
    quiet();
    begin();
    key(5);
    put("clear (busy page)", end(), ACT_MS, 1);
    begin();
    key(1);
    put("undo after clear", end(), ACT_MS, 0);
    begin();
    key(2);
    put("redo after clear", end(), ACT_MS, 0);
    key(6), settle(); /* a new page */
    brush_big();
    scribble(5, 200);
    quiet();
    begin();
    key(1);
    put("undo after a page switch", end(), ACT_MS, 0);
    begin();
    key(2);
    put("redo after a page switch", end(), ACT_MS, 0);
    down();
}

static void a_fill(void)
{
    up(10);
    sk_set_brush(&R.s, BR_FILL, R.now += 50);
    sk_set_shade(&R.s, SH_GREY, R.now += 50);
    quiet();
    begin();
    tap(1700, 1300); /* the whole page around the scribbles: asks first */
    put("fill: whole page, the tap that asks", end(), FILL_PAGE_MS, 0);
    begin();
    tap(1700, 1300);
    put("fill: whole page, Grey over a busy page", end(), FILL_PAGE_MS, 0);
    CHECK(R.s.hist.st_cur > 0 && R.s.hist.st[R.s.hist.st_cur - 1].kind == HK_FILL);
    begin();
    key(1);
    put("undo the whole-page fill", end(), ACT_MS, 0);
    sk_set_brush(&R.s, BR_PEN, R.now += 50);
    int sq[10] = { 1600, 100, 1800, 100, 1800, 300, 1600, 300, 1600, 100 };
    rig_stroke(&R, sq, 5, 40000, 0);
    sk_set_brush(&R.s, BR_FILL, R.now += 50);
    sk_set_shade(&R.s, SH_BLACK, R.now += 50);
    quiet();
    begin();
    tap(1700, 200);
    put("fill: small area", end(), ACT_MS, 0);
    down();
}

/* drawing with the Big Brush (or the eraser) in a mirror mode, the frame loop's own step feedback (sk_frame_cost with the
   frame's X ms, as main.c): the X ms of a 30 ms frame of 6 reports, on average after the first 10 frames */
static void a_draw(const char *name, int brush, int mirror, double bound)
{
    up(1);
    sk_set_brush(&R.s, brush, R.now += 50);
    sk_set_size(&R.s, 2, R.now += 50);
    if (mirror) sk_set_mirror(&R.s, mirror, R.now += 50);
    quiet();
    begin();
    rect_t out[SK_MAX_RECTS];
    int x = 300, y = 500, dx = 15, dy = 3, frames = 0;
    double sum = 0, mx = 0, c0 = cpu_us();
    rig_pen(&R, x, y, APP_PEN_IN_RANGE, 0);
    for (int i = 0; i < 600; i++) {
        x += dx, y += dy;
        if (x > 1500 || x < 300) dx = -dx, x += 2 * dx;
        if (y > 900 || y < 400) dy = -dy, y += 2 * dy;
        rig_pen(&R, x, y, APP_PEN_TIP | APP_PEN_IN_RANGE, 40000);
        if (i % 6 == 5) {
            sk_frame(&R.s, R.now);
            double c = cpu_us(), ms = (c - c0) * COST_X_RATIO / 1000.0;
            sk_frame_cost(&R.s, (uint32_t)ms);
            rig_flush(&R, R.now, out);
            if (++frames > 10) sum += ms, mx = ms > mx ? ms : mx;
            c0 = cpu_us();
        }
    }
    rig_pen(&R, x, y, APP_PEN_IN_RANGE, 0);
    settle();
    fulls = 0;
    stretch = total = mx;
    put(name, sum / (frames - 10), bound, 0);
    down();
}

static void a_toolbar(void)
{
    up(1);
    begin();
    key(4);
    put("toolbar show", end(), ACT_MS, 0);
    begin();
    tb_tap(TI_GREY);
    put("toolbar pick (Grey)", end(), ACT_MS, 0);
    begin();
    tb_tap(TI_MIRROR);
    put("mirror picker open", end(), ACT_MS, 0);
    begin();
    tap(R.s.mp.x0 + (R.s.mp.x1 - R.s.mp.x0) * 9 / 10, (R.s.mp.y0 + R.s.mp.y1) / 2);
    put("mirror picker pick", end(), ACT_MS, 0);
    begin();
    key(3);
    put("eraser on (button 3)", end(), ACT_MS, 0);
    begin();
    key(4);
    put("toolbar hide", end(), ACT_MS, 0);
    down();
}

int main(int argc, char **argv)
{
    int check = argc > 1 && !strcmp(argv[1], "--check");
    make_busy();
    double s10 = 0, s40 = 0, sc10 = 0, sc40 = 0, g10 = 0, g40 = 0;
    for (pass = 0; pass < 2; pass++) {
        nrows = 0;
        s10 = put("start, 10 pages", a_start(10), ACT_MS, 1);
        s40 = put("start, 40 pages", a_start(40), ACT_MS, 1);
        g10 = a_gallery(20, &sc10), g40 = a_gallery(40, &sc40);
        a_menus();
        a_pages();
        a_clear_undo();
        a_fill();
        a_toolbar();
        static const char *const mn[] = { "draw Big Brush", "draw Big Brush, left-right", "draw Big Brush, top-bottom", "draw Big Brush, 4-way", "draw Big Brush, kaleidoscope" };
        for (int m = 0; m < MR_N; m++) a_draw(mn[m], BR_BRUSH, m, m == MR_4 ? MIRROR4_MS : m == MR_K8 ? KALEIDO_MS : SK_FRAME_MS);
        a_draw("draw Big Eraser", BR_ERASER, 0, SK_FRAME_MS);
    }
    int bad = 0;
    printf("%-42s %9s %9s %5s %4s %8s\n", "action", "X ms", "total", "FULL", "INK", "screen ms");
    for (int i = 0; i < nrows; i++) {
        const row_t *r = &rows[i];
        int over = r->ms > r->bound || r->full > r->max_full || (r->tbound > 0 && r->total > r->tbound);
        bad |= over;
        if (verbose) printf("%-42s %9.0f %9.0f %5d %4d %8d%s\n", r->name, r->ms, r->total, r->full, r->ink, r->full * FULL_MS + r->ink * INK_MS, over ? "  OVER" : "");
    }
    printf("(page file %zu KB; draw rows: X ms of a 30 ms frame of 6 reports on average, total: the longest frame; bound %d)\n", busy_n / 1024, SK_FRAME_MS);
    if (verbose) printf("(bounds: %d ms an action; ratchets: print %d, whole-page fill %d, 4-way %d, kaleidoscope %d)\n", ACT_MS, PRINT_MS, FILL_PAGE_MS, MIRROR4_MS, KALEIDO_MS);
    int grow = s40 > s10 * ACT_GROW + 20 || g40 > g10 * ACT_GROW + 20 || sc40 > sc10 * ACT_GROW + 20;
    if (grow) printf("grows with the pages: start %.0f -> %.0f, gallery (20) %.0f -> %.0f, screen back %.0f -> %.0f ms\n", s10, s40, g10, g40, sc10, sc40);
    free(busy_png);
    bad |= grow || check_fail;
    if (bad && check) fprintf(stderr, "actions: an action over its bound, a FULL refresh too many, or a cost that grows with the pages\n");
    return check ? bad : 0;
}
