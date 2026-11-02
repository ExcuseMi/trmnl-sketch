/* make shots (tools part): papers, the colour and size buttons, cursors. SPDX-License-Identifier: MIT */
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

static void paper_shots(const char *dir, int *bad)
{
    static const char *const names[PP_N] = { "p1-paper-blank", "p2-paper-grid", "p3-paper-lines", "p4-paper-dots" };
    for (int k = 0; k < PP_N; k++) {
        rig_t r;
        rig_new(&r, APP_ORIENT_BAR_BOTTOM);
        sk_set_paper(&r.s, k, r.now);
        r.s.paper_sp = 48;
        sk_show_page(&r.s);
        sk_set_size(&r.s, 2, r.now);
        drag(&r, 300, 300, 900, 600, 0, 1);
        drag(&r, 1000, 700, 1500, 300, 0, 1);
        drag(&r, 300, 800, 1500, 900, 0, 1);
        frames(&r);
        put(&r, dir, names[k], bad);
        rig_end(&r);
    }
}

/* the toolbar with the three colours and three sizes: a stroke of each size in black, grey and white over black */
static void tone_shots(const char *dir, int *bad)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    sk_toolbar(&r.s, r.now);
    sk_set_brush(&r.s, BR_MARKER, r.now);
    sk_set_shade(&r.s, SH_BLACK, r.now);
    sk_set_size(&r.s, 2, r.now);
    for (int k = 0; k < 6; k++) drag(&r, 150, 1000 + k * 36, 1700, 1000 + k * 36, 0, 1); /* a black band for the white strokes */
    for (int c = 0; c < 3; c++) {
        sk_set_shade(&r.s, c == 0 ? SH_BLACK : c == 1 ? SH_GREY : SH_WHITE, r.now);
        for (int z = 0; z < 3; z++) {
            sk_set_size(&r.s, z, r.now);
            int y = c == 2 ? 1030 + z * 70 : 400 + c * 220 + z * 60;
            drag(&r, 200, y, 1600, y + 20, 0, 1);
        }
    }
    sk_set_shade(&r.s, SH_GREY, r.now);
    sk_set_size(&r.s, 1, r.now);
    frames(&r);
    put(&r, dir, "k1-toolbar-colours-and-sizes", bad);
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
    static const int ids[] = { BR_PEN, BR_PENCIL, BR_MARKER, BR_SPRAY, BR_ERASER, BR_FILL, BR_PEN, BR_MARKER };
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

/* The paintbrush at a light, a medium and a hard touch (three sizes each): black over white above, white over black below, and a crossing pass */
static void brush_wave(rig_t *r, int x0, int x1, int y, int amp, unsigned pressure)
{
    int p[2 * 25];
    for (int k = 0; k < 25; k++) p[2 * k] = x0 + (x1 - x0) * k / 24, p[2 * k + 1] = y + (int)(amp * sin(k * 0.52));
    rig_stroke(r, p, 25, pressure, 0);
}
static void paintbrush_shots(const char *dir, int *bad)
{
    static const unsigned pr[3] = { 8000, 30000, 65000 };
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    for (int y = 700; y < 1404; y++) cv_span_s(&r.s.page, y, 0, 1871, 0, 0);
    sk_show_page(&r.s);
    sk_set_brush(&r.s, BR_BRUSH, r.now += 50);
    for (int half = 0; half < 2; half++) {
        sk_set_shade(&r.s, half ? SH_WHITE : SH_BLACK, r.now += 50);
        for (int sz = 0; sz < 3; sz++) {
            sk_set_size(&r.s, sz, r.now += 50);
            for (int k = 0; k < 3; k++) brush_wave(&r, 100 + sz * 600, 600 + sz * 600, 120 + half * 700 + k * 190 + sz * 20, 30, pr[k]);
        }
    }
    sk_set_shade(&r.s, SH_BLACK, r.now += 50); /* overlap: a light pass over a hard one and a hard over a light, in grey too */
    sk_set_shade(&r.s, SH_GREY, r.now += 50);
    sk_set_size(&r.s, 2, r.now += 50);
    brush_wave(&r, 1300, 1800, 620, 20, pr[0]);
    brush_wave(&r, 1300, 1800, 640, 20, pr[2]);
    frames(&r);
    put(&r, dir, "br1-paintbrush-pressure-black-white-grey", bad);
    rig_end(&r);
}

void tool_shots(const char *dir, int *bad)
{
    cursor_shots(dir, bad);
    tone_shots(dir, bad);
    paper_shots(dir, bad);
    paintbrush_shots(dir, bad);
}
