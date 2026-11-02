/* make shots (tools part): shapes, papers, text, the tone slider, cursors, the recent row. SPDX-License-Identifier: MIT */
#include <math.h>
#include <stdio.h>
#include "util.h"

int png_write(const char *path, const gfx_t *g);
void tool_shots(const char *dir, int *bad);

#define DOWN (APP_PEN_TIP | APP_PEN_IN_RANGE)
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
static void drag(rig_t *r, int x0, int y0, int x1, int y1, unsigned extra, int lift)
{
    for (int k = 0; k <= 8; k++) rig_pen(r, x0 + (x1 - x0) * k / 8, y0 + (y1 - y0) * k / 8, DOWN | extra, 30000);
    if (lift) rig_pen(r, x1, y1, APP_PEN_IN_RANGE | extra, 0);
}

static void shape_shots(const char *dir, int *bad)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    sk_toolbar(&r.s, r.now);
    sk_set_tool(&r.s, TL_LINE, r.now);
    drag(&r, 200, 400, 700, 600, 0, 1);
    drag(&r, 200, 700, 700, 740, APP_PEN_BARREL, 1); /* constrained flat */
    drag(&r, 200, 800, 560, 1160, APP_PEN_BARREL, 1); /* 45 degrees */
    sk_set_size(&r.s, 9, r.now);
    sk_set_tool(&r.s, TL_RECT, r.now);
    drag(&r, 900, 300, 1500, 560, 0, 1);
    sk_set_brush(&r.s, BR_MARKER, r.now);
    sk_set_tool(&r.s, TL_OVAL, r.now);
    drag(&r, 900, 640, 1500, 800, 0, 1);
    sk_set_brush(&r.s, BR_FINELINER, r.now);
    sk_set_tool(&r.s, TL_OVAL, r.now);
    drag(&r, 1050, 880, 1500, 980, APP_PEN_BARREL, 1); /* a circle from a wide drag */
    frames(&r);
    put(&r, dir, "s1-shapes", bad);
    drag(&r, 400, 1200, 800, 1350, 0, 0); /* the rubber band, tip still down */
    frames(&r);
    put(&r, dir, "s2-shape-preview", bad);
    rig_end(&r);
}

static void paper_shots(const char *dir, int *bad)
{
    static const char *const names[PP_N] = { "p1-paper-blank", "p2-paper-grid", "p3-paper-lines", "p4-paper-dots" };
    for (int k = 0; k < PP_N; k++) {
        rig_t r;
        rig_new(&r, APP_ORIENT_BAR_BOTTOM);
        sk_set_paper(&r.s, k, r.now);
        r.s.paper_sp = 48;
        sk_show_page(&r.s);
        sk_set_tool(&r.s, TL_RECT, r.now);
        drag(&r, 300, 300, 900, 600, 0, 1);
        sk_set_tool(&r.s, TL_OVAL, r.now);
        drag(&r, 1000, 300, 1500, 700, 0, 1);
        sk_set_tool(&r.s, TL_LINE, r.now);
        drag(&r, 300, 800, 1500, 900, 0, 1);
        frames(&r);
        put(&r, dir, names[k], bad);
        rig_end(&r);
    }
}

static void say(rig_t *r, const char *s)
{
    for (; *s; s++) {
        char c[2] = { *s, 0 };
        r->now += 20;
        sk_key(&r->s, 1, *s == '\n' ? APP_KEY_ENTER : APP_KEY_CHAR, 0, 0, *s == '\n' ? "" : c, r->now);
    }
}

static void text_shots(const char *dir, int *bad)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    sk_set_paper(&r.s, PP_LINES, r.now);
    r.s.paper_sp = 96;
    sk_show_page(&r.s);
    sk_keyboard(&r.s, APP_KBD_USB, r.now);
    sk_set_tool(&r.s, TL_TEXT, r.now);
    sk_set_size(&r.s, 40, r.now);
    rig_pen(&r, 200, 200, DOWN, 30000);
    rig_pen(&r, 200, 200, APP_PEN_IN_RANGE, 0);
    say(&r, "Shopping list");
    sk_text_end(&r.s, r.now);
    sk_set_size(&r.s, 24, r.now);
    rig_pen(&r, 200, 480, DOWN, 30000);
    rig_pen(&r, 200, 480, APP_PEN_IN_RANGE, 0);
    say(&r, "bread\nmilk\neggs, and\nsome apples");
    sk_text_end(&r.s, r.now);
    sk_set_size(&r.s, 8, r.now);
    sk_set_shade(&r.s, SH_DARK, r.now);
    rig_pen(&r, 900, 480, DOWN, 30000);
    rig_pen(&r, 900, 480, APP_PEN_IN_RANGE, 0);
    say(&r, "small gray note");
    frames(&r);
    put(&r, dir, "x1-text-on-lines", bad);
    rig_end(&r);
}

static void tone_shots(const char *dir, int *bad)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    sk_toolbar(&r.s, r.now);
    sk_set_size(&r.s, 20, r.now);
    sk_set_tool(&r.s, TL_LINE, r.now);
    for (int k = 0; k < SH_N; k++) { /* one stroke of every tone */
        sk_set_shade(&r.s, SH_N - 1 - k, r.now);
        drag(&r, 200 + k * 170, 400, 200 + k * 170, 1200, 0, 1);
    }
    sk_set_shade(&r.s, 5, r.now);
    frames(&r);
    put(&r, dir, "k1-toolbar-tone-and-size", bad);
    rig_end(&r);
}

void tool_shots(const char *dir, int *bad)
{
    tone_shots(dir, bad);
    text_shots(dir, bad);
    shape_shots(dir, bad);
    paper_shots(dir, bad);
}
