/* SPDX-License-Identifier: MIT */
#include "toolbar.h"
#include "fmath.h"
#include "font.h"
#include "penmap.h"
#include "tbicons.h"
#include "brush.h"
#include "sk_cfg.h"

#define TB_PAD 8
#define TB_MARGIN 16
#define TB_GAP 24
#define TB_LINE 4
#define TB_CH 104 /* glyph 64 + label */
enum { TB_BLK = GFX_BLACK, TB_WHT = GFX_WHITE };

static const char *const tb_label[TI_N] = { "Fine", "Brush", "Pencil", "Marker", "Calli", "Spray", "Eraser", "Black", "Dark", "Light", "", "", "", "Undo", "Redo", "Clear" };

void tb_layout(tb_layout_t *L, int orient, int sw, int sh)
{
    int cell = (sw - 2 * TB_MARGIN - 3 * TB_GAP) / TI_N;
    if (cell > 100) cell = 100;
    L->cell = cell;
    L->ch = TB_CH;
    int total = cell * TI_N + 3 * TB_GAP, x = (sw - total) / 2, h = TB_CH + 2 * TB_PAD + TB_LINE;
    for (int i = 0; i < TI_N; i++) {
        if (i == TI_SHADE || i == TI_SIZE || i == TI_UNDO) L->gap_x[i == TI_SHADE ? 0 : i == TI_SIZE ? 1 : 2] = x + TB_GAP / 2, x += TB_GAP;
        L->cx[i] = x;
        x += cell;
    }
    L->sl_x0 = L->cx[TI_SIZE] + 28, L->sl_x1 = L->cx[TI_SIZE + 2] + cell - 28, L->sl_y = 0; /* sl_y is set with cy below */
    L->top = orient != APP_ORIENT_BAR_TOP;
    L->x0 = 0, L->x1 = sw - 1;
    L->y0 = L->top ? 0 : sh - h, L->y1 = L->top ? h - 1 : sh - 1;
    L->cy = L->top ? TB_PAD : L->y0 + TB_PAD + TB_LINE;
    L->sl_y = L->cy + 44;
}

int tb_in(const tb_layout_t *L, int x, int y) { return x >= L->x0 && x <= L->x1 && y >= L->y0 && y <= L->y1; }

int tb_hit(const tb_layout_t *L, int x, int y)
{
    if (y < L->cy || y >= L->cy + L->ch) return -1;
    for (int i = 0; i < TI_N; i++)
        if (x >= L->cx[i] && x < L->cx[i] + L->cell) return i > TI_SIZE && i < TI_UNDO ? TI_SIZE : i;
    return -1;
}

/* size = 1 + 47 (dx / W)^2 rounded: monotonic, one step at most per px at the thick end for W >= 94 */
static int sl_size(const tb_layout_t *L, int dx)
{
    long w = L->sl_x1 - L->sl_x0;
    if (dx < 0) dx = 0;
    if (dx > w) dx = (int)w;
    return SK_SIZE_MIN + (int)(((long)(SK_SIZE_MAX - SK_SIZE_MIN) * dx * dx + w * w / 2) / (w * w));
}
int tb_slider_size(const tb_layout_t *L, int x) { return sl_size(L, x - L->sl_x0); }
int tb_slider_x(const tb_layout_t *L, int size)
{
    int w = L->sl_x1 - L->sl_x0, dx = 0;
    while (dx < w && sl_size(L, dx) < size) dx++;
    return L->sl_x0 + dx;
}
void tb_slider_box(const tb_layout_t *L, int *x0, int *y0, int *x1, int *y1)
{
    *x0 = L->cx[TI_SIZE], *x1 = L->cx[TI_SIZE + 2] + L->cell - 1, *y0 = L->cy, *y1 = L->cy + L->ch - 1;
}

static void tb_slider(gfx_t *g, const tb_layout_t *L, const bstyle_t *cur, int hot)
{
    int x0, y0, x1, y1, kx = tb_slider_x(L, cur->size), d = cur->size, y = L->sl_y;
    tb_slider_box(L, &x0, &y0, &x1, &y1);
    gfx_fill_rect(g, (float)L->sl_x0, (float)(y - 2), (float)L->sl_x1, (float)(y + 2), TB_BLK); /* the track */
    gfx_fill_rect(g, (float)(L->sl_x0 - 2), (float)(y - 7), (float)(L->sl_x0 - 1), (float)(y + 7), TB_BLK); /* ends */
    gfx_fill_rect(g, (float)(L->sl_x1 + 1), (float)(y - 7), (float)(L->sl_x1 + 2), (float)(y + 7), TB_BLK);
    if (hot == TI_SIZE) { /* the brush's footprint at the knob, clipped to the slider */
        gfx_clip(g, x0, y0, x1, y1);
        for (int dy = -48; dy <= 48; dy++)
            for (int dx = -48; dx <= 48; dx++)
                if (brush_shape_px(cur->brush, cur->size, dx, dy)) gfx_px(g, kx + dx, y + dy, TB_BLK);
        gfx_clip(g, L->x0, L->y0, L->x1, L->y1);
    }
    gfx_ellipse(g, (float)(kx - d / 2), (float)(y - d / 2), (float)(kx + d / 2), (float)(y + d / 2), 0, TB_BLK, TB_BLK);
    if (d < 14) gfx_ellipse(g, (float)(kx - 8), (float)(y - 8), (float)(kx + 8), (float)(y + 8), 2, TB_WHT, TB_BLK); /* a grip round a small dot */
    char t[12] = "Size ";
    int n = 5, v = cur->size;
    if (v >= 10) t[n++] = (char)('0' + v / 10);
    t[n++] = (char)('0' + v % 10), t[n] = 0;
    const font_face_t *f = font_get(FONT_BOLD, 24);
    int w = (int)((font_width64(f, t, 0) + 32) >> 6);
    gfx_text(g, f, (int32_t)((x0 + x1 - w) / 2) * 64, L->cy + 4 + TBI_SIZE + 26, t, 0, TB_BLK);
}

/* The pattern as it will be inked (black dots on paper), in a bordered swatch; picked: a white border on the black cell */
static void tb_swatch(gfx_t *g, int shade, int x, int y, int sel)
{
    int q = 44, sx = x + (TBI_SIZE - q) / 2, sy = y + (TBI_SIZE - q) / 2;
    gfx_rrect(g, (float)(sx - 4), (float)(sy - 4), (float)(sx + q + 3), (float)(sy + q + 3), 3, 3, TB_WHT, sel ? TB_WHT : TB_BLK);
    for (int py = 0; py < q; py++)
        for (int px = 0; px < q; px++) gfx_px(g, sx + px, sy + py, cv_shade_black(shade, sx + px, sy + py) ? TB_BLK : TB_WHT);
}

static void tb_caption(gfx_t *g, const tb_layout_t *L, int i, int x, int y, int c)
{
    const font_face_t *f = font_get(FONT_BOLD, 24);
    int w = (int)((font_width64(f, tb_label[i], 0) + 32) >> 6);
    gfx_text(g, f, (int32_t)(x + (L->cell - w) / 2) * 64, y + TBI_SIZE + 26, tb_label[i], 0, c);
}

void tb_draw(gfx_t *g, const tb_layout_t *L, const bstyle_t *cur, int hot)
{
    gfx_clip(g, L->x0, L->y0, L->x1, L->y1);
    gfx_fill_rect(g, (float)L->x0, (float)L->y0, (float)L->x1, (float)L->y1, TB_WHT);
    int ly = L->top ? L->y1 - TB_LINE + 1 : L->y0;
    gfx_fill_rect(g, (float)L->x0, (float)ly, (float)L->x1, (float)(ly + TB_LINE - 1), TB_BLK);
    for (int k = 0; k < 3; k++) /* a thin divider between the groups */
        gfx_fill_rect(g, (float)(L->gap_x[k] - 1), (float)(L->cy + 8), (float)L->gap_x[k], (float)(L->cy + L->ch - 8), TB_BLK);
    for (int i = 0; i < TI_N; i++) {
        int x = L->cx[i], y = L->cy, sel = 0, gx = x + (L->cell - TBI_SIZE) / 2, gy = y + 4;
        if (i < TI_SHADE) sel = cur->brush == i;
        else if (i < TI_SIZE) sel = cur->shade == i - TI_SHADE;
        else if (i < TI_UNDO) continue; /* the slider */
        int x1 = x + L->cell - 1, y1 = y + L->ch - 1;
        if (sel) gfx_rrect(g, (float)x, (float)y, (float)x1, (float)y1, 12, 0, TB_BLK, TB_BLK);
        if (hot == i) { /* the pen is over it: a frame (on a picked item an inset white one) */
            if (sel) gfx_rrect(g, (float)(x + 5), (float)(y + 5), (float)(x1 - 5), (float)(y1 - 5), 8, 3, TB_BLK, TB_WHT);
            else gfx_rrect(g, (float)x, (float)y, (float)x1, (float)y1, 12, 4, TB_WHT, TB_BLK);
        }
        int c = sel ? TB_WHT : TB_BLK, bg = sel ? TB_BLK : TB_WHT;
        if (i < TI_SHADE) tbi_brush(g, i, gx, gy, c, bg);
        else if (i < TI_SIZE) tb_swatch(g, i - TI_SHADE, gx, gy, sel);
        else if (i == TI_UNDO) tbi_undo(g, gx, gy, c, bg);
        else if (i == TI_REDO) tbi_redo(g, gx, gy, c, bg);
        else tbi_clear(g, gx, gy, c, bg);
        tb_caption(g, L, i, x, y, c);
    }
    tb_slider(g, L, cur, hot);
    gfx_noclip(g);
}
