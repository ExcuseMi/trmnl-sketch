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

static const char *const tb_label[TI_N] = { "Fine", "Brush", "Pencil", "Marker", "Calli", "Spray", "Eraser", "Fill", "Line", "Box", "Oval", "Text", "Tone", "", "", "", "", "", "Mirror", "Undo", "Redo", "Clear", "", "", "", "", "" };

void tb_layout(tb_layout_t *L, int orient, int sw, int sh)
{
    static const int row_n[TB_ROWS] = { TI_TONE, TI_N - TI_TONE }, row_gaps[TB_ROWS] = { 1, 3 };
    int cell = 100;
    for (int r = 0; r < TB_ROWS; r++) {
        int c = (sw - 2 * TB_MARGIN - row_gaps[r] * TB_GAP) / row_n[r];
        if (c < cell) cell = c;
    }
    L->cell = cell;
    L->ch = TB_CH;
    int h = 2 * TB_CH + 2 * TB_PAD + TB_LINE;
    L->top = orient != APP_ORIENT_BAR_TOP;
    L->x0 = 0, L->x1 = sw - 1;
    L->y0 = L->top ? 0 : sh - h, L->y1 = L->top ? h - 1 : sh - 1;
    L->cy = L->top ? TB_PAD : L->y0 + TB_PAD + TB_LINE;
    int g = 0;
    for (int r = 0; r < TB_ROWS; r++) {
        int first = r ? TI_TONE : 0, last = r ? TI_N : TI_TONE, x = (sw - (row_n[r] * cell + row_gaps[r] * TB_GAP)) / 2;
        for (int i = first; i < last; i++) {
            if (i == TI_LINE || i == TI_SIZE || i == TI_MIRROR || i == TI_RECENT) L->gap_x[g] = x + TB_GAP / 2, L->gap_row[g++] = r, x += TB_GAP;
            L->cx[i] = x, L->cyi[i] = L->cy + r * TB_CH;
            x += cell;
        }
    }
    L->tn_x0 = L->cx[TI_TONE] + 36, L->tn_x1 = L->cx[TI_TONE + 2] + cell - 36, L->tn_y = L->cyi[TI_TONE] + 44;
    L->sl_x0 = L->cx[TI_SIZE] + 28, L->sl_x1 = L->cx[TI_SIZE + 2] + cell - 28;
    L->sl_y = L->cyi[TI_SIZE] + 44;
}

int tb_in(const tb_layout_t *L, int x, int y) { return x >= L->x0 && x <= L->x1 && y >= L->y0 && y <= L->y1; }

int tb_hit(const tb_layout_t *L, int x, int y)
{
    for (int i = 0; i < TI_N; i++)
        if (y >= L->cyi[i] && y < L->cyi[i] + L->ch && x >= L->cx[i] && x < L->cx[i] + L->cell) return i > TI_SIZE && i < TI_MIRROR ? TI_SIZE : i > TI_TONE && i < TI_SIZE ? TI_TONE : i;
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
/* 8 steps along the track, the shade of the nearest: the left end is the lightest (shade SH_N - 1) */
int tb_tone_of(const tb_layout_t *L, int x)
{
    long w = L->tn_x1 - L->tn_x0;
    int dx = x < L->tn_x0 ? 0 : x > L->tn_x1 ? (int)w : x - L->tn_x0, step = (int)(((long)dx * (SH_N - 1) + w / 2) / w);
    return SH_N - 1 - step;
}
int tb_tone_x(const tb_layout_t *L, int shade)
{
    long w = L->tn_x1 - L->tn_x0;
    int step = SH_N - 1 - (shade < 0 ? 0 : shade >= SH_N ? SH_N - 1 : shade);
    return L->tn_x0 + (int)(((long)step * w + (SH_N - 1) / 2) / (SH_N - 1));
}
void tb_tone_box(const tb_layout_t *L, int *x0, int *y0, int *x1, int *y1)
{
    *x0 = L->cx[TI_TONE], *x1 = L->cx[TI_TONE + 2] + L->cell - 1, *y0 = L->cyi[TI_TONE], *y1 = L->cyi[TI_TONE] + L->ch - 1;
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
    *x0 = L->cx[TI_SIZE], *x1 = L->cx[TI_SIZE + 2] + L->cell - 1, *y0 = L->cyi[TI_SIZE], *y1 = L->cyi[TI_SIZE] + L->ch - 1;
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
    gfx_text(g, f, (int32_t)((x0 + x1 - w) / 2) * 64, L->cyi[TI_SIZE] + 4 + TBI_SIZE + 26, t, 0, TB_BLK);
}

/* The tone slider: a track with a tick for each of the 9 steps, the knob a swatch of the tone as it will be inked */
static void tb_tone(gfx_t *g, const tb_layout_t *L, const bstyle_t *cur)
{
    int x0, y0, x1, y1, y = L->tn_y, kx = tb_tone_x(L, cur->shade), q = 44, sx = kx - q / 2, sy = y - q / 2;
    tb_tone_box(L, &x0, &y0, &x1, &y1);
    gfx_fill_rect(g, (float)L->tn_x0, (float)(y - 2), (float)L->tn_x1, (float)(y + 2), TB_BLK);
    for (int k = 0; k < SH_N; k++) { /* the steps */
        int tx = tb_tone_x(L, SH_N - 1 - k);
        gfx_fill_rect(g, (float)(tx - 1), (float)(y - 7), (float)tx, (float)(y + 7), TB_BLK);
    }
    gfx_rrect(g, (float)(sx - 4), (float)(sy - 4), (float)(sx + q + 3), (float)(sy + q + 3), 3, 3, TB_WHT, TB_BLK);
    for (int py = 0; py < q; py++)
        for (int px = 0; px < q; px++) gfx_px(g, sx + px, sy + py, cv_shade_black(cur->shade, sx + px, sy + py) ? TB_BLK : TB_WHT);
    char t[16] = "Tone ";
    int n = 5, lv = cv_shade_level(cur->shade);
    if (lv == 16) t[n++] = 'B', t[n++] = 'l', t[n++] = 'a', t[n++] = 'c', t[n++] = 'k';
    else {
        if (lv >= 10) t[n++] = '1';
        t[n++] = (char)('0' + lv % 10), t[n++] = '/', t[n++] = '1', t[n++] = '6';
    }
    t[n] = 0;
    const font_face_t *f = font_get(FONT_BOLD, 24);
    int w = (int)((font_width64(f, t, 0) + 32) >> 6);
    gfx_text(g, f, (int32_t)((x0 + x1 - w) / 2) * 64, L->cyi[TI_TONE] + 4 + TBI_SIZE + 26, t, 0, TB_BLK);
}

static void tb_caption(gfx_t *g, const tb_layout_t *L, int i, int x, int y, int c)
{
    const font_face_t *f = font_get(FONT_BOLD, 24);
    int w = (int)((font_width64(f, tb_label[i], 0) + 32) >> 6);
    gfx_text(g, f, (int32_t)(x + (L->cell - w) / 2) * 64, y + TBI_SIZE + 26, tb_label[i], 0, c);
}

/* A Recent slot's caption: the pen size */
static void tb_recent_caption(gfx_t *g, const tb_layout_t *L, const bstyle_t *e, int x, int y, int c)
{
    char t[8] = "";
    if (e) {
        int n = 0;
        if (e->size >= 10) t[n++] = (char)('0' + e->size / 10);
        t[n++] = (char)('0' + e->size % 10), t[n] = 0;
    }
    const font_face_t *f = font_get(FONT_BOLD, 24);
    int w = (int)((font_width64(f, t, 0) + 32) >> 6);
    gfx_text(g, f, (int32_t)(x + (L->cell - w) / 2) * 64, y + TBI_SIZE + 26, t, 0, c);
}

void tb_draw(gfx_t *g, const tb_layout_t *L, const bstyle_t *cur, int hot, const bstyle_t *rc, int rc_n)
{
    gfx_clip(g, L->x0, L->y0, L->x1, L->y1);
    gfx_fill_rect(g, (float)L->x0, (float)L->y0, (float)L->x1, (float)L->y1, TB_WHT);
    int ly = L->top ? L->y1 - TB_LINE + 1 : L->y0;
    gfx_fill_rect(g, (float)L->x0, (float)ly, (float)L->x1, (float)(ly + TB_LINE - 1), TB_BLK);
    for (int k = 0; k < 4; k++) { /* a thin divider between the groups */
        int dy = L->cy + L->gap_row[k] * TB_CH;
        gfx_fill_rect(g, (float)(L->gap_x[k] - 1), (float)(dy + 8), (float)L->gap_x[k], (float)(dy + L->ch - 8), TB_BLK);
    }
    for (int i = 0; i < TI_N; i++) {
        int x = L->cx[i], y = L->cyi[i], sel = 0, gx = x + (L->cell - TBI_SIZE) / 2, gy = y + 4;
        if (i < TI_LINE) sel = cur->brush == i && cur->tool == TL_DRAW;
        else if (i < TI_TONE) sel = cur->tool == i - TI_LINE + TL_LINE;
        else if (i < TI_MIRROR) continue; /* the sliders */
        else if (i == TI_MIRROR) sel = cur->mirror != 0;
        else if (i >= TI_RECENT) sel = i - TI_RECENT < rc_n && rc[i - TI_RECENT].brush == cur->brush && rc[i - TI_RECENT].shade == cur->shade && rc[i - TI_RECENT].size == cur->size;
        int x1 = x + L->cell - 1, y1 = y + L->ch - 1;
        if (sel) gfx_rrect(g, (float)x, (float)y, (float)x1, (float)y1, 12, 0, TB_BLK, TB_BLK);
        if (hot == i) { /* the pen is over it: a frame (on a picked item an inset white one) */
            if (sel) gfx_rrect(g, (float)(x + 5), (float)(y + 5), (float)(x1 - 5), (float)(y1 - 5), 8, 3, TB_BLK, TB_WHT);
            else gfx_rrect(g, (float)x, (float)y, (float)x1, (float)y1, 12, 4, TB_WHT, TB_BLK);
        }
        int c = sel ? TB_WHT : TB_BLK, bg = sel ? TB_BLK : TB_WHT;
        if (i < TI_LINE) tbi_brush(g, i, gx, gy, c, bg);
        else if (i < TI_TONE) tbi_tool(g, i - TI_LINE + TL_LINE, gx, gy, c, bg);
        else if (i == TI_MIRROR) tbi_mirror(g, cur->mirror, gx, gy, c, bg);
        else if (i >= TI_RECENT) tbi_recent(g, i - TI_RECENT < rc_n ? &rc[i - TI_RECENT] : 0, gx, gy, c, bg);
        else if (i == TI_UNDO) tbi_undo(g, gx, gy, c, bg);
        else if (i == TI_REDO) tbi_redo(g, gx, gy, c, bg);
        else tbi_clear(g, gx, gy, c, bg);
        if (i >= TI_RECENT) tb_recent_caption(g, L, i < TI_RECENT + rc_n ? &rc[i - TI_RECENT] : 0, x, y, c);
        else tb_caption(g, L, i, x, y, c);
    }
    tb_tone(g, L, cur);
    tb_slider(g, L, cur, hot);
    gfx_noclip(g);
}
