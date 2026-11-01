/* Helpers shared by the host tests: a sketch on a memory file system, pen strokes, a panel copy. SPDX-License-Identifier: MIT */
#ifndef UTIL_H
#define UTIL_H
#include <stdlib.h>
#include <string.h>
#include <app_api.h>
#include "memfs.h"
#include "sketch.h"
#include "tbfw.h"

#define TW 1872
#define TH 1404
#define TS (TW / 8)
/* a Deco 01 V2: 10 x 6.25 in */
#define UM_W 254000u
#define UM_H 158750u

typedef struct {
    memfs_t fs;
    sketch_t s;
    uint8_t *fb;
    uint32_t now;
    int started;
    char cfg_cur[8], cfg_size[8];
} rig_t;

static int rig_get(void *u, const char *k, char *b, int cap)
{
    rig_t *r = u;
    const char *v = !strcmp(k, "cur") ? r->cfg_cur : !strcmp(k, "size") ? r->cfg_size : "";
    int n = (int)strlen(v);
    if (n > cap) n = cap;
    memcpy(b, v, (size_t)n);
    return n ? n : -1;
}
static int rig_set(void *u, const char *k, const char *v, int n)
{
    rig_t *r = u;
    char *d = !strcmp(k, "cur") ? r->cfg_cur : r->cfg_size;
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
    sk_env_t env = { &r->fs.fs, r, rig_get, rig_set };
    if (sk_init(&r->s, &env, r->fb, TW, TH, TS, orient, &bar)) abort();
    sk_set_tablet(&r->s, UM_W, UM_H, 1);
    r->started = 1;
}
static inline void rig_new(rig_t *r, int orient)
{
    memset(r, 0, sizeof(*r));
    memfs_init(&r->fs);
    r->now = 1000;
    rig_start(r, orient);
}
static inline void rig_end(rig_t *r)
{
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
