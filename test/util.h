/* Helpers shared by the host tests: a sketch on a memory file system, pen strokes, a panel copy. SPDX-License-Identifier: MIT */
#ifndef UTIL_H
#define UTIL_H
#include <stdlib.h>
#include <string.h>
#include <app_api.h>
#include "memfs.h"
#include "sketch.h"
#include "tbfw.h"
#include "printjob.h"

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
    char cfg_printer[40];
    sketch_t s;
    uint8_t *fb;
    uint32_t now;
    int started;
    char cfg_cur[8], cfg_size[8], cfg_brush[8], cfg_shade[8];
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

static int rig_get(void *u, const char *k, char *b, int cap)
{
    rig_t *r = u;
    const char *v = !strcmp(k, "cur") ? r->cfg_cur : !strcmp(k, "size") ? r->cfg_size : !strcmp(k, "brush") ? r->cfg_brush : !strcmp(k, "shade") ? r->cfg_shade : !strcmp(k, "printer") ? r->cfg_printer : "";
    int n = (int)strlen(v);
    if (n > cap) n = cap;
    memcpy(b, v, (size_t)n);
    return n ? n : -1;
}
static int rig_set(void *u, const char *k, const char *v, int n)
{
    rig_t *r = u;
    char *d = !strcmp(k, "cur") ? r->cfg_cur : !strcmp(k, "brush") ? r->cfg_brush : !strcmp(k, "shade") ? r->cfg_shade : r->cfg_size;
    memcpy(d, v, (size_t)n);
    d[n] = 0;
    return 0;
}

/* (Re)starts the app on the rig's file system: a restart keeps the files and settings, as the X does */
static inline void rig_start(rig_t *r, int orient)
{
    if (r->started) sk_free(&r->s), free(r->fb);
    r->fb = malloc((size_t)TS * TH);
    memset(r->fb, 0xff, (size_t)TS * TH);
    app_touchbar_t bar;
    tbfw_layout(orient, &bar);
    sk_env_t env = { &r->fs.fs, r, rig_get, rig_set, { PJ_PWG, fake_post, &r->pr } };
    if (sk_init(&r->s, &env, r->fb, TW, TH, TS, orient, &bar)) abort();
    sk_set_tablet(&r->s, UM_W, UM_H, 1);
    r->started = 1;
}
static inline void rig_new(rig_t *r, int orient)
{
    memset(r, 0, sizeof(*r));
    memfs_init(&r->fs);
    r->now = 1000;
    r->pr.status = 200;
    rig_start(r, orient);
}
/* What the glue does each turn: flush, and count the full and partial refreshes it would push */
static inline int rig_frame(rig_t *r, rect_t *out)
{
    int n = sk_flush(&r->s, r->now += 100, out);
    if (n == SK_FULL) r->fulls++;
    else if (n > 0) r->parts += n;
    return n;
}
static inline void rig_end(rig_t *r)
{
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
    float u = (float)fx + (m->wt - (float)m->sw) / 2, v = (float)fy + (m->ht - (float)m->sh) / 2;
    *x = (unsigned)(u / m->wt * 65535.0f + 0.5f);
    *y = (unsigned)(v / m->ht * 65535.0f + 0.5f);
}

/* the pen at a point of the user's frame (any position of the X): the inverse of pm_map */
static inline void rig_pen_user(rig_t *r, int ux, int uy, unsigned flags, unsigned pressure)
{
    const penmap_t *m = &r->s.map;
    float u = (float)ux + (m->wt - (float)m->sw) / 2, v = (float)uy + (m->ht - (float)m->sh) / 2;
    r->now += 5;
    sk_pen(&r->s, flags, (unsigned)(u / m->wt * 65535.0f + 0.5f), (unsigned)(v / m->ht * 65535.0f + 0.5f), pressure, r->now);
}

static inline void rig_pen(rig_t *r, int fx, int fy, unsigned flags, unsigned pressure)
{
    unsigned x, y;
    rig_pt(r, fx, fy, &x, &y);
    r->now += 5;
    sk_pen(&r->s, flags, x, y, pressure, r->now);
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
static inline int same_page_and_fb(const rig_t *r) { return !memcmp(r->fb, r->s.page.px, (size_t)TS * TH); }
#endif
