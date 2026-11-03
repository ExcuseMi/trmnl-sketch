/* make shots (mirror part): a kaleidoscope drawing, a 4-way drawing and the toolbar with the Mirror item.
   SPDX-License-Identifier: MIT */
#include <math.h>
#include <stdio.h>
#include "util.h"

int png_write(const char *path, const gfx_t *g);
void mirror_shots(const char *dir, int *bad);

static void put(rig_t *r, const char *dir, const char *name, int *bad)
{
    gfx_t g;
    gfx_init(&g, r->fb, TW, TH, TS, r->s.orient);
    char path[256];
    snprintf(path, sizeof(path), "%s/%s.png", dir, name);
    if (png_write(path, &g)) fprintf(stderr, "cannot write %s\n", path), *bad = 1;
    printf("%s\n", path);
}

static void frames(rig_t *r)
{
    rect_t o[SK_MAX_RECTS];
    r->now += 4000;
    sk_tick(&r->s, r->now);
    for (int i = 0; i < 8; i++) rig_frame(r, o);
}

/* a loop and a leaf near the page centre: points along curves, pressure rising and falling */
static void curve(rig_t *r, int cx, int cy, float a0, float a1, float r0, float r1, float wob)
{
    int n = 70;
    for (int i = 0; i <= n; i++) {
        float t = (float)i / (float)n, a = a0 + (a1 - a0) * t, rad = r0 + (r1 - r0) * t + wob * sinf(t * 18.0f);
        unsigned p = (unsigned)(8000.0f + 50000.0f * sinf(t * 3.14159f));
        rig_pen(r, cx + (int)(cosf(a) * rad), cy + (int)(sinf(a) * rad), APP_PEN_TIP | APP_PEN_IN_RANGE, p);
    }
    rig_pen(r, cx, cy, 0, 0);
}

static void flower(rig_t *r, int mode, const char *dir, const char *name, int *bad)
{
    sk_set_mirror(&r->s, mode, r->now += 50);
    sk_set_shade(&r->s, SH_BLACK, r->now += 50);
    sk_set_brush(&r->s, BR_PEN, r->now += 50);
    sk_set_size(&r->s, 2, r->now += 50);
    int cx = TW / 2, cy = TH / 2;
    curve(r, cx, cy, -1.5f, -0.2f, 260, 560, 24);
    sk_set_brush(&r->s, BR_PEN, r->now += 50);
    sk_set_size(&r->s, 0, r->now += 50);
    curve(r, cx, cy, -1.2f, 0.3f, 120, 300, 12);
    sk_set_brush(&r->s, BR_MARKER, r->now += 50);
    sk_set_shade(&r->s, SH_GREY, r->now += 50);
    sk_set_size(&r->s, 2, r->now += 50);
    curve(r, cx, cy, -2.4f, -2.0f, 420, 520, 0);
    sk_set_brush(&r->s, BR_SPRAY, r->now += 50);
    sk_set_shade(&r->s, SH_BLACK, r->now += 50);
    sk_set_size(&r->s, 2, r->now += 50);
    curve(r, cx, cy, -0.8f, -0.7f, 600, 640, 0);
    frames(r);
    put(r, dir, name, bad);
}

/* the Big Brush in every mode, black and grey loops (the batch of 2026-11-03 must look as stamp by stamp) */
static void big_brush(int mode, const char *dir, int *bad)
{
    static rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    sk_set_mirror(&r.s, mode, r.now += 50);
    sk_set_brush(&r.s, BR_BRUSH, r.now += 50);
    sk_set_size(&r.s, 2, r.now += 50);
    int cx = TW / 2, cy = TH / 2;
    sk_set_shade(&r.s, SH_BLACK, r.now += 50);
    curve(&r, cx, cy, -1.5f, -0.2f, 260, 560, 24);
    sk_set_shade(&r.s, SH_GREY, r.now += 50);
    curve(&r, cx, cy, -2.6f, -1.6f, 150, 420, 30);
    frames(&r);
    char name[64];
    snprintf(name, sizeof(name), "m6-bigbrush-%d-%s", mode, mode == MR_OFF ? "off" : mode == MR_LR ? "lr" : mode == MR_TB ? "tb" : mode == MR_4 ? "4way" : "kaleido");
    put(&r, dir, name, bad);
    rig_end(&r);
}

void mirror_shots(const char *dir, int *bad)
{
    for (int m = MR_OFF; m < MR_N; m++) big_brush(m, dir, bad);
    static rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    flower(&r, MR_K8, dir, "m1-mirror-kaleidoscope", bad);
    rig_end(&r);
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    flower(&r, MR_4, dir, "m2-mirror-4way", bad);
    sk_pen_key(&r.s, 1u << 3, 1u << 3, r.now += 50); /* the toolbar: Mirror is picked while a symmetry is on */
    sk_pen_key(&r.s, 0, 1u << 3, r.now);
    frames(&r);
    put(&r, dir, "m3-toolbar-with-mirror", bad);
    rig_end(&r);
    for (int m = MR_LR; m < MR_N; m++) { /* the glyphs of every mode, the portrait strip */
        rig_new(&r, APP_ORIENT_BAR_LEFT);
        sk_set_mirror(&r.s, m, r.now += 50);
        sk_pen_key(&r.s, 1u << 3, 1u << 3, r.now += 50);
        sk_pen_key(&r.s, 0, 1u << 3, r.now);
        frames(&r);
        char n[64];
        snprintf(n, sizeof(n), "m4-toolbar-portrait-mode-%d", m);
        put(&r, dir, n, bad);
        rig_end(&r);
    }
    for (int o = 0; o < 2; o++) { /* the open picker: landscape (bar bottom, strip on top) and portrait */
        rig_new(&r, o ? APP_ORIENT_BAR_LEFT : APP_ORIENT_BAR_BOTTOM);
        sk_set_mirror(&r.s, MR_4, r.now += 50);
        sk_pen_key(&r.s, 1u << 3, 1u << 3, r.now += 50);
        sk_pen_key(&r.s, 0, 1u << 3, r.now);
        frames(&r);
        int x = r.s.tb.cx[TI_MIRROR] + r.s.tb.cell / 2, y = r.s.tb.cyi[TI_MIRROR] + 30;
        rig_pen_user(&r, x, y, APP_PEN_TIP | APP_PEN_IN_RANGE, 30000);
        rig_pen_user(&r, x, y, APP_PEN_IN_RANGE, 0);
        rig_pen_user(&r, r.s.mp.cx[3] + 50, r.s.mp.cy + 40, APP_PEN_IN_RANGE, 0);
        frames(&r);
        put(&r, dir, o ? "m5-mirror-picker-portrait" : "m5-mirror-picker-landscape", bad);
        rig_end(&r);
    }
}
