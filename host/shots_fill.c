/* make shots (fill part): a kids' house with a sun, filled in the three shades and mixed; the toolbar with Fill; the
   bucket cursor over paper, black and a shade. SPDX-License-Identifier: MIT */
#include <math.h>
#include <stdio.h>
#include "util.h"

int png_write(const char *path, const gfx_t *g);
void fill_shots(const char *dir, int *bad);

static void put(rig_t *r, const char *dir, const char *name, int *bad)
{
    gfx_t g;
    gfx_init(&g, r->fb, TW, TH, TS, r->s.orient);
    char path[256];
    snprintf(path, sizeof(path), "%s/%s.png", dir, name);
    if (png_write(path, &g)) fprintf(stderr, "cannot write %s\n", path), *bad = 1;
    printf("%s\n", path);
}

static void seg(rig_t *r, int x0, int y0, int x1, int y1) { rig_line(r, x0, y0, x1, y1, 60000, 0); }

static void house(rig_t *r)
{
    sk_set_brush(&r->s, BR_FINELINER, r->now += 50);
    sk_set_size(&r->s, 14, r->now += 50);
    seg(r, 700, 700, 1100, 700), seg(r, 1100, 700, 1100, 1100), seg(r, 1100, 1100, 700, 1100), seg(r, 700, 1100, 700, 700); /* the wall */
    seg(r, 650, 700, 900, 450), seg(r, 900, 450, 1150, 700), seg(r, 1150, 700, 650, 700);                                    /* the roof */
    seg(r, 860, 1100, 860, 950), seg(r, 860, 950, 940, 950), seg(r, 940, 950, 940, 1100);                                    /* the door */
    seg(r, 740, 780, 820, 780), seg(r, 820, 780, 820, 860), seg(r, 820, 860, 740, 860), seg(r, 740, 860, 740, 780);        /* a window */
    seg(r, 300, 1100, 1700, 1100);                                                                                            /* the ground */
    for (int i = 0; i <= 60; i++) { /* the sun, its circle not quite closed (a small gap: the default closes it) */
        float a = 0.15f + (float)i / 60.0f * 6.1f;
        rig_pen(r, 1500 + (int)(cosf(a) * 110.0f), 400 + (int)(sinf(a) * 110.0f), APP_PEN_TIP | APP_PEN_IN_RANGE, 60000);
    }
    rig_pen(r, 1500, 400, 0, 0);
    for (int i = 0; i < 8; i++) {
        float a = (float)i * 0.7854f;
        seg(r, 1500 + (int)(cosf(a) * 140.0f), 400 + (int)(sinf(a) * 140.0f), 1500 + (int)(cosf(a) * 200.0f), 400 + (int)(sinf(a) * 200.0f));
    }
}

static void tap(rig_t *r, int x, int y)
{
    rig_pen(r, x, y, APP_PEN_TIP | APP_PEN_IN_RANGE, 30000);
    rig_pen(r, x, y, 0, 0);
    rect_t o[SK_MAX_RECTS];
    for (int i = 0; i < 4; i++) rig_frame(r, o);
}

static void colour(rig_t *r, int roof, int wall, int door, int sun, int win)
{
    sk_set_brush(&r->s, BR_FILL, r->now += 50);
    int at[5][3] = { { 900, 600, 0 }, { 900, 900, 0 }, { 900, 1050, 0 }, { 1500, 400, 0 }, { 780, 820, 0 } };
    int sh[5] = { roof, wall, door, sun, win };
    for (int i = 0; i < 5; i++) {
        sk_set_shade(&r->s, sh[i], r->now += 50);
        tap(r, at[i][0], at[i][1]);
    }
    rig_pen(r, 10, 10, 0, 0);
    r->now += 4000;
    sk_tick(&r->s, r->now);
    rect_t o[SK_MAX_RECTS];
    for (int i = 0; i < 6; i++) rig_frame(r, o);
}

void fill_shots(const char *dir, int *bad)
{
    static rig_t r;
    static const char *const names[3] = { "f1-fill-house-black", "f2-fill-house-dark", "f3-fill-house-light" };
    static const int tones[3] = { SH_BLACK, SH_DARK, SH_LIGHT };
    for (int k = 0; k < 3; k++) {
        int sh = tones[k];
        rig_new(&r, APP_ORIENT_BAR_BOTTOM);
        house(&r);
        colour(&r, sh, sh, sh, sh, sh);
        put(&r, dir, names[k], bad);
        rig_end(&r);
    }
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    house(&r);
    colour(&r, SH_BLACK, SH_DARK, SH_LIGHT, SH_LIGHT, SH_DARK);
    put(&r, dir, "f4-fill-house-mixed", bad);
    sk_pen_key(&r.s, 1u << 3, 1u << 3, r.now += 50); /* the toolbar with Fill picked */
    sk_pen_key(&r.s, 0, 1u << 3, r.now);
    r.now += 3000;
    sk_tick(&r.s, r.now);
    rect_t o[SK_MAX_RECTS];
    for (int i = 0; i < 6; i++) rig_frame(&r, o);
    put(&r, dir, "f5-toolbar-with-fill", bad);
    sk_pen_key(&r.s, 1u << 3, 1u << 3, r.now += 50);
    sk_pen_key(&r.s, 0, 1u << 3, r.now);
    /* the cursor over white, black ink and the shades */
    rig_pen(&r, 10, 10, 0, 0);
    for (int i = 0; i < 6; i++) rig_frame(&r, o);
    const int sx[4] = { 250, 600, 950, 1300 };
    box_t bx;
    ui_cursor(&r.s.fb, 150, 1230, BR_FILL, 0, &bx); /* over white */
    ui_cursor(&r.s.fb, 740, 1000, BR_FILL, 0, &bx); /* over the dark wall */
    ui_cursor(&r.s.fb, 1500, 400, BR_FILL, 0, &bx);  /* over the light sun */
    ui_cursor(&r.s.fb, 900, 450, BR_FILL, 0, &bx);   /* over black: the roof's tip */
    (void)sx;
    put(&r, dir, "f6-bucket-cursor", bad);
    rig_end(&r);
    rig_new(&r, APP_ORIENT_BAR_TOP);
    sk_pen_key(&r.s, 1u << 3, 1u << 3, r.now += 50);
    sk_pen_key(&r.s, 0, 1u << 3, r.now);
    sk_set_brush(&r.s, BR_FILL, r.now += 50);
    r.now += 3000;
    sk_tick(&r.s, r.now);
    for (int i = 0; i < 6; i++) rig_frame(&r, o);
    put(&r, dir, "f7-toolbar-with-fill-portrait-bar-top", bad);
    rig_end(&r);
}
