/* make shots (pro feel): a shaky line with the stabiliser at 0 and 6, pencil shading upright and tilted 50 degrees,
   the toolbar with its Smooth cell. SPDX-License-Identifier: MIT */
#include <math.h>
#include <stdio.h>
#include "util.h"

int png_write(const char *path, const gfx_t *g);
void pro_shots(const char *dir, int *bad);

static void put(rig_t *r, const char *dir, const char *name, int *bad)
{
    gfx_t g;
    gfx_init(&g, r->fb, TW, TH, TS, r->s.orient);
    char path[256];
    snprintf(path, sizeof(path), "%s/%s.png", dir, name);
    if (png_write(path, &g)) fprintf(stderr, "cannot write %s\n", path), *bad = 1;
    printf("%s\n", path);
}

static void settle(rig_t *r)
{
    rect_t o[SK_MAX_RECTS];
    r->now += 4000;
    sk_tick(&r->s, r->now);
    for (int i = 0; i < 8; i++) rig_frame(r, o);
}

static void smooth_to(rig_t *r, int level) { r->s.smooth = level; }

/* a hand that shakes: a slow diagonal line, about 2 px of tremor at 200 events a second */
static void shaky(rig_t *r, int y0)
{
    unsigned seed = 12345;
    for (int i = 0; i < 260; i++) {
        seed = seed * 1103515245u + 12345u;
        int jx = (int)(seed >> 16 & 7) - 3, jy = (int)(seed >> 8 & 7) - 3;
        rig_pen(r, 200 + i * 3 + jx, y0 + (int)(60.0 * sin(i * 0.03)) + jy, APP_PEN_TIP | APP_PEN_IN_RANGE, 30000);
    }
    rig_pen(r, 980, y0, APP_PEN_IN_RANGE, 0);
}

/* white ink: a black band, then each brush in white over it,  */
static void white_shot(const char *dir, int *bad)
{
    static rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    sk_set_brush(&r.s, BR_MARKER, r.now += 50);
    sk_set_size(&r.s, 2, r.now += 50);
    for (int k = 0; k < 9; k++) rig_line(&r, 150, 280 + k * 45, 1550, 280 + k * 45, 40000, 0);
    sk_set_shade(&r.s, SH_WHITE, r.now += 50);
    static const int br[4] = { BR_PEN, BR_PENCIL, BR_MARKER, BR_SPRAY };
    for (int k = 0; k < 4; k++) {
        sk_set_brush(&r.s, br[k], r.now += 50);
        sk_set_size(&r.s, 1, r.now += 50);
        for (int i = 0; i < 70; i++)
            rig_pen(&r, 250 + i * 18, 330 + k * 60 + (int)(14.0 * sin(i * 0.2)), APP_PEN_TIP | APP_PEN_IN_RANGE, 8000 + (unsigned)i * 500);
        rig_pen(&r, 1500, 330 + k * 60, APP_PEN_IN_RANGE, 0);
    }
    sk_pen_key(&r.s, 1u << 3, 1u << 3, r.now += 50);
    sk_pen_key(&r.s, 0, 1u << 3, r.now);
    settle(&r);
    put(&r, dir, "pro4-white-ink-over-black", bad);
    rig_end(&r);
}

void pro_shots(const char *dir, int *bad)
{
    white_shot(dir, bad);
    static rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    for (int k = 0; k < 2; k++) {
        smooth_to(&r, k ? 6 : 0);
        shaky(&r, 300 + k * 250);
    }
    settle(&r);
    put(&r, dir, "pro1-shaky-smooth-0-above-6-below", bad);
    rig_end(&r);

    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    sk_set_brush(&r.s, BR_PENCIL, r.now += 50);
    sk_set_size(&r.s, 2, r.now += 50);
    static const int tilt[3] = { 0, 25, 50 };
    for (int k = 0; k < 3; k++) {
        for (int i = 0; i < 90; i++) {
            sk_pen_tilt(&r.s, 0, tilt[k]); /* leaning toward the page's bottom: across the strokes */
            rig_pen(&r, 250 + i * 8, 350 + k * 220 + (int)(20.0 * sin(i * 0.1)), APP_PEN_TIP | APP_PEN_IN_RANGE, 34000);
        }
        sk_pen_tilt(&r.s, 0, 0);
        rig_pen(&r, 970, 350 + k * 220, APP_PEN_IN_RANGE, 0);
    }
    settle(&r);
    put(&r, dir, "pro2-pencil-tilt-0-25-50-degrees", bad);
    rig_end(&r);
}
