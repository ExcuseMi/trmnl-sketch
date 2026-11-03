/* Helpers shared by the host tests: a sketch on a memory file system, pen strokes, a panel copy. SPDX-License-Identifier: MIT */
#ifndef UTIL_H
#define UTIL_H
#include "check.h"
#include <stdlib.h>
#include <string.h>
#include <app_api.h>
#include "memfs.h"
#include "sketch.h"
#include "tbfw.h"
#include "printjob.h"

/* Tones the shade machinery still draws (the toolbar offers only black, grey and white): tests use them where two different tones are needed */
#define SH_DARK 3
#define SH_LIGHT 6

#define TW 1872
#define TH 1404
#define TS (TW / 8)
/* a Deco 01 V2: 10 x 6.25 in */
#define UM_W 254000u
#define UM_H 158750u

/* a printer that records what is posted */
typedef struct {
    int status, ipp, posts;
    char url[96];
    uint8_t *body[4];
    size_t len[4];
} fprinter_t;

typedef struct {
    memfs_t fs;
    fprinter_t pr;
    int fulls, parts;   /* full and partial refreshes the glue would have pushed */
    int pr_rc;          /* what the default printer lookup answers (0, PJ_NO_PRINTER, ...) */
    pj_printer_t prn;   /* the default printer (printer.cfg) */
    sketch_t s;
    uint8_t *fb;
    uint32_t now;
    int started;
    struct { char k[16], v[16]; } cfg[24]; /* the settings the app stored (cfg_set); a test may pre-set any key with rig_cfg */
    int keep_smooth;    /* a test that wants the app's own stabiliser level (SK_SMOOTH_DEF) sets this before rig_start */
    int keep_tool;      /* a test that wants the tool the app opens with (the Brush) sets this before rig_start */
    int log_n, log_ev_med, log_ev_p95, log_push_med, log_push_p95, log_samples; /* the latency lines sketch handed to its log */
    int inverted;       /* inverted halves seen (rig_flush) */
    int bare_fulls;     /* full refreshes handed out without their inverted half first: the flash helper was bypassed */
    box_t pre;          /* the area of the last inverted half */
} rig_t;

static int fake_post(void *u, const char *url, const void *body, size_t n, uint8_t *resp, size_t cap, size_t *rn)
{
    fprinter_t *p = u;
    if (p->posts < 4) {
        p->body[p->posts] = malloc(n);
        memcpy(p->body[p->posts], body, n);
        p->len[p->posts] = n;
    }
    p->posts++;
    strncpy(p->url, url, sizeof(p->url) - 1);
    *rn = 0;
    if (p->status == 200 && cap >= 8) {
        memset(resp, 0, 8);
        resp[0] = 1, resp[1] = 1, resp[2] = (uint8_t)(p->ipp >> 8), resp[3] = (uint8_t)p->ipp, *rn = 8;
    }
    return p->status;
}

static void rig_log(void *u, int ev_med, int ev_p95, int push_med, int push_p95, int n)
{
    rig_t *r = u;
    r->log_n++, r->log_ev_med = ev_med, r->log_ev_p95 = ev_p95, r->log_push_med = push_med, r->log_push_p95 = push_p95, r->log_samples = n;
}

static int rig_printer(void *u, pj_printer_t *out)
{
    rig_t *r = u;
    *out = r->prn;
    return r->pr_rc;
}

static int rig_get(void *u, const char *k, char *b, int cap)
{
    rig_t *r = u;
    for (int i = 0; i < 24; i++) {
        if (strcmp(r->cfg[i].k, k)) continue;
        int n = (int)strlen(r->cfg[i].v);
        if (n > cap) n = cap;
        memcpy(b, r->cfg[i].v, (size_t)n);
        return n ? n : -1;
    }
    return -1;
}
static int rig_set(void *u, const char *k, const char *v, int n)
{
    rig_t *r = u;
    int at = -1;
    for (int i = 0; i < 24 && at < 0; i++)
        if (!strcmp(r->cfg[i].k, k) || !r->cfg[i].k[0]) at = i;
    if (at < 0 || n > 15) return -1;
    strcpy(r->cfg[at].k, k);
    memcpy(r->cfg[at].v, v, (size_t)n);
    r->cfg[at].v[n] = 0;
    return 0;
}
/* The value the app stored for a setting, or "" */
static inline const char *rig_cfgv(const rig_t *r, const char *k)
{
    for (int i = 0; i < 24; i++)
        if (!strcmp(r->cfg[i].k, k)) return r->cfg[i].v;
    return "";
}
/* A setting as an earlier or a later session left it (read at the next rig_start) */
static inline void rig_cfg(rig_t *r, const char *k, const char *v) { rig_set(r, k, v, (int)strlen(v)); }

/* (Re)starts the app on the rig's file system: a restart keeps the files and settings, as the X does */
static inline void rig_start(rig_t *r, int orient)
{
    if (r->started) sk_free(&r->s), free(r->fb);
    r->fb = malloc((size_t)TS * TH);
    memset(r->fb, 0xff, (size_t)TS * TH);
    app_touchbar_t bar;
    tbfw_layout(orient, &bar);
    sk_env_t env = { &r->fs.fs, r, rig_get, rig_set, { PJ_PWG, fake_post, &r->pr }, rig_log, rig_printer };
    if (sk_init(&r->s, &env, r->fb, TW, TH, TS, orient, &bar)) abort();
    if (!r->keep_smooth) r->s.smooth = 0; /* the stabiliser is off in the tests (the app's is SK_SMOOTH_DEF): the points are exactly the pen's */
    if (!r->keep_tool) r->s.sty.brush = r->s.last_ink = BR_PEN, r->s.sty.size = (uint8_t)brush_size(BR_PEN, r->s.size_cls); /* the tests draw with the solid Pen (the app opens with the Brush: test_oldpage) */
    sk_set_tablet(&r->s, UM_W, UM_H, 1);
    r->started = 1;
}
static inline void rig_new(rig_t *r, int orient)
{
    memset(r, 0, sizeof(*r));
    memfs_init(&r->fs);
    r->now = 1000;
    r->pr.status = 200;
    memset(&r->prn, 0, sizeof(r->prn));
    strcpy(r->prn.host, "10.1.2.3");
    r->prn.port = 631;
    strcpy(r->prn.path, "ipp/print");
    rig_start(r, orient);
}
/* What the glue does each turn: flush (a flash is its inverted half, then the clean one at once), and count the full and
   partial refreshes it would push. A full refresh without the inverted half of the same area before it is counted in
   bare_fulls (rig_end checks it is 0: every clean-up goes through sk_flash) */
static inline int rig_flush(rig_t *r, uint32_t now, rect_t *out)
{
    int n = sk_flush(&r->s, now, out);
    if (sk_flash_busy(&r->s)) { /* the inverted half: n == 1, a partial rectangle */
        CHECK(n == 1 && out[0].flash == 0);
        r->inverted++;
        box_clear(&r->pre);
        box_add(&r->pre, out[0].x, out[0].y);
        box_add(&r->pre, out[0].x + out[0].w - 1, out[0].y + out[0].h - 1);
        r->parts++;
        rect_t o2[SK_MAX_RECTS];
        n = sk_flush(&r->s, now, o2);
        CHECK(!sk_flash_busy(&r->s));
        if (n == SK_FULL) CHECK(r->pre.x0 == 0 && r->pre.y0 == 0 && r->pre.x1 == TW - 1 && r->pre.y1 == TH - 1);
        else CHECK(n == 1 && o2[0].flash == 1 && o2[0].x == out[0].x && o2[0].y == out[0].y && o2[0].w == out[0].w && o2[0].h == out[0].h);
        if (n == 1) out[0] = o2[0];
        if (n == SK_FULL) r->fulls++;
        return n;
    }
    if (n == SK_FULL || (n > 0 && out[0].flash)) r->bare_fulls++;
    if (n == SK_FULL) r->fulls++;
    else if (n > 0) r->parts += n;
    return n;
}
static inline int rig_frame(rig_t *r, rect_t *out) { return rig_flush(r, r->now += 100, out); }
/* The gallery's tiles get their thumbnails a file a frame (sk_gal_step), after the screen's refresh: here all of them at
   once, into the screen still to be pushed (the tests look at the screen as the full refresh shows it) */
static inline void rig_gal_fill(rig_t *r)
{
    for (int k = 0; k < 200 && r->s.gal_todo; k++) sk_gal_step(&r->s, 1);
}
static inline void rig_end(rig_t *r)
{
    CHECK(r->bare_fulls == 0);
    for (int i = 0; i < 4; i++) free(r->pr.body[i]);
    sk_free(&r->s);
    free(r->fb);
    memfs_free(&r->fs);
}

/* tablet units 0..65535 of a page point given as fraction of the page, for the default (landscape) mapping */
static inline void rig_pt(const rig_t *r, int fx, int fy, unsigned *x, unsigned *y)
{
    /* inverse of pm_map for the landscape position by search on the tablet axis: exact enough for tests */
    const penmap_t *m = &r->s.map;
    float u = (float)fx + m->ox, v = (float)fy + m->oy;
    *x = (unsigned)(u / m->wt * 65535.0f + 0.5f);
    *y = (unsigned)(v / m->ht * 65535.0f + 0.5f);
}

/* the pen at a point of the user's frame (any position of the X): the inverse of pm_map */
static inline void rig_pen_user(rig_t *r, int ux, int uy, unsigned flags, unsigned pressure)
{
    const penmap_t *m = &r->s.map;
    float u = (float)ux + m->ox, v = (float)uy + m->oy;
    r->now += 5;
    unsigned x = (unsigned)(u / m->wt * 65535.0f + 0.5f), y = (unsigned)(v / m->ht * 65535.0f + 0.5f), t = x;
    if (m->turn == 1) x = y, y = 65535 - t; /* the tablet turned clockwise: undo it (penmap.c) */
    else if (m->turn == 2) x = 65535 - y, y = t;
    sk_pen(&r->s, flags, x, y, pressure, r->now);
    if (!(flags & APP_PEN_TIP)) sk_frame(&r->s, r->now);
}

static inline void rig_pen(rig_t *r, int fx, int fy, unsigned flags, unsigned pressure)
{
    unsigned x, y;
    rig_pt(r, fx, fy, &x, &y);
    r->now += 5;
    sk_pen(&r->s, flags, x, y, pressure, r->now);
    if (!(flags & APP_PEN_TIP)) sk_frame(&r->s, r->now); /* a hover report is a frame of its own: the frame draws the cursor (sk_pen only notes it); ink points stay queued */
}
/* a stroke along points, then the pen lifts */
static inline void rig_stroke(rig_t *r, const int *xy, int n, unsigned pressure, unsigned extra)
{
    for (int i = 0; i < n; i++) rig_pen(r, xy[2 * i], xy[2 * i + 1], APP_PEN_TIP | APP_PEN_IN_RANGE | extra, pressure);
    rig_pen(r, xy[2 * n - 2], xy[2 * n - 1], APP_PEN_IN_RANGE | extra, 0);
}
static inline void rig_line(rig_t *r, int x0, int y0, int x1, int y1, unsigned pressure, unsigned extra)
{
    int p[8] = { x0, y0, (x0 * 2 + x1) / 3, (y0 * 2 + y1) / 3, (x0 + x1 * 2) / 3, (y0 + y1 * 2) / 3, x1, y1 };
    rig_stroke(r, p, 4, pressure, extra);
}
static inline int fb_black(const rig_t *r) /* black pixels on the framebuffer */
{
    int n = 0;
    for (int i = 0; i < TS * TH; i++)
        for (int b = 0; b < 8; b++) n += !((r->fb[i] >> b) & 1);
    return n;
}
/* the framebuffer is the page (no pills: nothing is drawn over it but the toolbar, notes and cursors, which the callers exclude by their state) */
static inline int same_page_and_fb(const rig_t *r) { return !memcmp(r->fb, r->s.page.px, (size_t)TS * TH); }
#endif
