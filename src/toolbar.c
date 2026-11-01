/* SPDX-License-Identifier: MIT */
#include "toolbar.h"
#include "fmath.h"

#define PAD 8
#define MARGIN 16
#define GROUP_GAP 20
#define LINE 4
enum { BLK = GFX_BLACK, WHT = GFX_WHITE };

void tb_layout(tb_layout_t *L, int orient, int sw, int sh)
{
    int cell = (sw - 2 * MARGIN - 3 * GROUP_GAP) / TI_N;
    if (cell > 104) cell = 104;
    L->cell = cell;
    int total = cell * TI_N + 3 * GROUP_GAP, x = (sw - total) / 2, h = cell + 2 * PAD + LINE;
    for (int i = 0; i < TI_N; i++) {
        if (i == TI_SHADE || i == TI_SIZE || i == TI_UNDO) x += GROUP_GAP;
        L->cx[i] = x;
        x += cell;
    }
    L->top = orient != APP_ORIENT_BAR_TOP;
    L->x0 = 0, L->x1 = sw - 1;
    L->y0 = L->top ? 0 : sh - h, L->y1 = L->top ? h - 1 : sh - 1;
    L->cy = L->top ? PAD : L->y0 + PAD + LINE;
}

int tb_in(const tb_layout_t *L, int x, int y) { return x >= L->x0 && x <= L->x1 && y >= L->y0 && y <= L->y1; }

int tb_hit(const tb_layout_t *L, int x, int y)
{
    if (y < L->cy || y >= L->cy + L->cell) return -1;
    for (int i = 0; i < TI_N; i++)
        if (x >= L->cx[i] && x < L->cx[i] + L->cell) return i;
    return -1;
}

static void shape(gfx_t *g, const tb_layout_t *L, int brush, int x, int y, int c)
{
    int r = brush_cursor_r(brush, 1), x0 = r, x1 = -r, y0 = r, y1 = -r;
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++)
            if (brush_cursor_px(brush, 1, dx, dy)) {
                if (dx < x0) x0 = dx;
                if (dx > x1) x1 = dx;
                if (dy < y0) y0 = dy;
                if (dy > y1) y1 = dy;
            }
    int dim = (x1 - x0 > y1 - y0 ? x1 - x0 : y1 - y0) + 1, sc = L->cell * 70 / 100 / dim;
    if (sc < 2) sc = 2;
    if (sc > 4) sc = 4;
    int ox = x + L->cell / 2 - (x0 + x1 + 1) * sc / 2, oy = y + L->cell / 2 - (y0 + y1 + 1) * sc / 2;
    for (int dy = y0; dy <= y1; dy++)
        for (int dx = x0; dx <= x1; dx++)
            if (brush_cursor_px(brush, 1, dx, dy))
                gfx_fill_rect(g, (float)(ox + dx * sc), (float)(oy + dy * sc), (float)(ox + dx * sc + sc - 1), (float)(oy + dy * sc + sc - 1), c);
}

/* The pattern as it will be inked (black dots on paper), in a frame; picked: the frame is white on the black cell */
static void swatch(gfx_t *g, const tb_layout_t *L, int shade, int x, int y, int sel)
{
    int q = L->cell * 55 / 100, sx = x + (L->cell - q) / 2, sy = y + (L->cell - q) / 2;
    gfx_rrect(g, sx - 3, sy - 3, sx + q + 2, sy + q + 2, 0, 3, WHT, sel ? WHT : BLK);
    for (int py = 0; py < q; py++)
        for (int px = 0; px < q; px++) gfx_px(g, sx + px, sy + py, cv_shade_black(shade, sx + px, sy + py) ? BLK : WHT);
}

static void arrow(gfx_t *g, const tb_layout_t *L, int x, int y, int dir, int c)
{
    float m = (float)L->cell / 2, cx = (float)x + m, cy = (float)y + m, s = (float)L->cell * 0.26f;
    float tip = cx + dir * s, back = cx - dir * s * 0.1f;
    float p[6] = { tip, cy, back, cy - s * 0.8f, back, cy + s * 0.8f };
    gfx_poly(g, p, 3, c);
    gfx_line(g, back, cy, cx - dir * s, cy, 6, c);
    gfx_line(g, cx - dir * s, cy, cx - dir * s, cy + s * 0.9f, 6, c);
}

static void cross(gfx_t *g, const tb_layout_t *L, int x, int y, int c)
{
    float m = (float)L->cell / 2, cx = (float)x + m, cy = (float)y + m, s = (float)L->cell * 0.22f;
    gfx_line(g, cx - s, cy - s, cx + s, cy + s, 7, c);
    gfx_line(g, cx - s, cy + s, cx + s, cy - s, 7, c);
}

void tb_draw(gfx_t *g, const tb_layout_t *L, const bstyle_t *cur)
{
    gfx_clip(g, L->x0, L->y0, L->x1, L->y1);
    gfx_fill_rect(g, (float)L->x0, (float)L->y0, (float)L->x1, (float)L->y1, WHT);
    int ly = L->top ? L->y1 - LINE + 1 : L->y0;
    gfx_fill_rect(g, (float)L->x0, (float)ly, (float)L->x1, (float)(ly + LINE - 1), BLK);
    for (int i = 0; i < TI_N; i++) {
        int x = L->cx[i], y = L->cy, sel = 0;
        if (i < TI_SHADE) sel = cur->brush == i;
        else if (i < TI_SIZE) sel = cur->shade == i - TI_SHADE;
        else if (i < TI_UNDO) sel = cur->size == i - TI_SIZE;
        if (sel) gfx_rrect(g, (float)x, (float)y, (float)(x + L->cell - 1), (float)(y + L->cell - 1), 12, 0, BLK, BLK);
        int c = sel ? WHT : BLK;
        if (i < TI_SHADE) shape(g, L, i, x, y, c);
        else if (i < TI_SIZE) swatch(g, L, i - TI_SHADE, x, y, sel);
        else if (i < TI_UNDO) {
            static const int dia[3] = { 10, 22, 38 };
            int d = dia[i - TI_SIZE] * L->cell / 104, mx = x + L->cell / 2, my = y + L->cell / 2;
            if (d < 4) d = 4;
            gfx_ellipse(g, (float)(mx - d / 2), (float)(my - d / 2), (float)(mx + d / 2), (float)(my + d / 2), 0, c, c);
        } else if (i == TI_UNDO) arrow(g, L, x, y, -1, c);
        else if (i == TI_REDO) arrow(g, L, x, y, 1, c);
        else cross(g, L, x, y, c);
    }
    gfx_noclip(g);
}
