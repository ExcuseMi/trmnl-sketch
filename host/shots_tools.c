/* make shots (tools part): shapes, papers, text, the tone slider, cursors, the recent row. SPDX-License-Identifier: MIT */
#include <math.h>
#include <stdio.h>
#include "skui.h"
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

/* every cursor over a mid tone (left) and over black ink (right), and over paper and white below */
static void cursor_shots(const char *dir, int *bad)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    canvas_t *p = &r.s.page;
    for (int y = 80; y < 700; y++) cv_span_s(p, y, 60, 920, 0, 5), cv_span_s(p, y, 950, 1810, 0, 0);
    box_t all = { 0, 0, TW - 1, TH - 1 };
    sk_set_paper(&r.s, PP_GRID, r.now);
    r.s.paper_sp = 24;
    sk_show_page(&r.s);
    (void)all;
    static const int ids[] = { BR_FINELINER, BR_MARKER, BR_SPRAY, BR_ERASER, BR_N + TL_LINE, BR_N + TL_RECT, BR_N + TL_OVAL, BR_N + TL_TEXT };
    for (int row = 0; row < 3; row++)
        for (int k = 0; k < 8; k++) {
            int cx = 130 + k * 108 + (row == 1 ? 0 : 0), cy = row == 0 ? 240 : row == 1 ? 520 : 900, size = row == 2 ? 9 : row == 1 ? 40 : 20;
            if (row == 1 && k >= 4) size = 40;
            box_t b;
            ui_cursor(&r.s.fb, cx, cy, ids[k], size, &b);
            ui_cursor(&r.s.fb, cx + 900 + (row == 2 ? 0 : 0), cy, ids[k], size, &b);
        }
    ui_arrow(&r.s.fb, r.s.orient, 400, 1150, &(box_t){ 0 });
    ui_arrow(&r.s.fb, r.s.orient, 1300, 1150, &(box_t){ 0 });
    put(&r, dir, "c9-cursors-over-tone-black-paper", bad);
    rig_end(&r);
}

static void recent_shots(const char *dir, int *bad)
{
    static const int combo[5][3] = { { BR_FINELINER, 0, 4 }, { BR_MARKER, 5, 30 }, { BR_PENCIL, 3, 12 }, { BR_BRUSHPEN, 7, 20 }, { BR_SPRAY, 1, 44 } };
    for (int o = 0; o < 2; o++) {
        rig_t r;
        rig_new(&r, o ? APP_ORIENT_BAR_LEFT : APP_ORIENT_BAR_BOTTOM);
        for (int k = 0; k < 5; k++) {
            sk_set_brush(&r.s, combo[k][0], r.now += 50);
            sk_set_shade(&r.s, combo[k][1], r.now += 50);
            sk_set_size(&r.s, combo[k][2], r.now += 50);
            if (o) {
                for (int i = 0; i <= 20; i++) rig_pen_user(&r, 200 + i * 30, 500 + k * 120, APP_PEN_TIP | APP_PEN_IN_RANGE, 40000);
                rig_pen_user(&r, 800, 500 + k * 120, APP_PEN_IN_RANGE, 0);
            } else {
                drag(&r, 300, 400 + k * 150, 1200, 420 + k * 150, 0, 1);
            }
        }
        sk_toolbar(&r.s, r.now);
        frames(&r);
        put(&r, dir, o ? "r2-recent-row-portrait" : "r1-recent-row", bad);
        rig_end(&r);
    }
}

void tool_shots(const char *dir, int *bad)
{
    recent_shots(dir, bad);
    cursor_shots(dir, bad);
    tone_shots(dir, bad);
    text_shots(dir, bad);
    shape_shots(dir, bad);
    paper_shots(dir, bad);
}
