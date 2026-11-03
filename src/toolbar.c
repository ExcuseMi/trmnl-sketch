/* SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 */
#include "toolbar.h"
#include "font.h"
#include "penmap.h"
#include "tbicons.h"
#include "brush.h"
#include "sk_cfg.h"

#define TB_PAD 8
#define TB_MARGIN 4 /* (the row must keep a 90 px cell on the 1404 px wide screen with 15 items: test_toolbar.c) */
#define TB_GAP 10
#define TB_LINE 4
#define TB_CH 104 /* glyph 64 + label */
enum { TB_BLK = GFX_BLACK, TB_WHT = GFX_WHITE };

static const char *const tb_other[TI_N - TI_BLACK] = { "Black", "Grey", "White", "Small", "Medium", "Big", "Mirror", "Menu" };
const char *tb_label(int i) { return i < TI_BLACK ? brush_name(i) : tb_other[i - TI_BLACK]; } /* the tools named by brush.c, in its order */

int tb_shade_of(int item) { return item == TI_BLACK ? SH_BLACK : item == TI_GREY ? SH_GREY : item == TI_WHITE ? SH_WHITE : -1; }
int tb_item_of_shade(int shade) { return shade == SH_BLACK ? TI_BLACK : shade == SH_GREY ? TI_GREY : shade == SH_WHITE ? TI_WHITE : -1; }

void tb_layout(tb_layout_t *L, int orient, int sw, int sh)
{
    int cell = (sw - 2 * TB_MARGIN - TB_GAPS * TB_GAP) / TI_N;
    if (cell > 100) cell = 100;
    L->cell = cell;
    L->ch = TB_CH;
    int h = TB_CH + 2 * TB_PAD + TB_LINE;
    L->top = orient != APP_ORIENT_BAR_TOP;
    L->x0 = 0, L->x1 = sw - 1;
    L->y0 = L->top ? 0 : sh - h, L->y1 = L->top ? h - 1 : sh - 1;
    L->cy = L->top ? TB_PAD : L->y0 + TB_PAD + TB_LINE;
    int g = 0, x = (sw - (TI_N * cell + TB_GAPS * TB_GAP)) / 2;
    for (int i = 0; i < TI_N; i++) {
        if (i == TI_BLACK || i == TI_SMALL || i == TI_MIRROR || i == TI_MENU) L->gap_x[g++] = x + TB_GAP / 2, x += TB_GAP;
        L->cx[i] = x, L->cyi[i] = L->cy;
        x += cell;
    }
}

int tb_in(const tb_layout_t *L, int x, int y) { return x >= L->x0 && x <= L->x1 && y >= L->y0 && y <= L->y1; }

int tb_hit(const tb_layout_t *L, int x, int y)
{
    for (int i = 0; i < TI_N; i++)
        if (y >= L->cyi[i] && y < L->cyi[i] + L->ch && x >= L->cx[i] && x < L->cx[i] + L->cell) return i;
    return -1;
}

static void tb_caption(gfx_t *g, const tb_layout_t *L, int i, int x, int y, int c)
{
    const font_face_t *f = font_get(FONT_BOLD, 24);
    int w = (int)((font_width64(f, tb_label(i), 0) + 32) >> 6);
    gfx_text(g, f, (int32_t)(x + (L->cell - w) / 2) * 64, y + TBI_SIZE + 26, tb_label(i), 0, c);
}

void tb_draw(gfx_t *g, const tb_layout_t *L, const bstyle_t *cur, int hot)
{
    gfx_clip(g, L->x0, L->y0, L->x1, L->y1);
    gfx_fill_rect(g, (float)L->x0, (float)L->y0, (float)L->x1, (float)L->y1, TB_WHT);
    int ly = L->top ? L->y1 - TB_LINE + 1 : L->y0;
    gfx_fill_rect(g, (float)L->x0, (float)ly, (float)L->x1, (float)(ly + TB_LINE - 1), TB_BLK);
    for (int k = 0; k < TB_GAPS; k++) /* a thin divider between the groups */
        gfx_fill_rect(g, (float)(L->gap_x[k] - 1), (float)(L->cy + 8), (float)L->gap_x[k], (float)(L->cy + L->ch - 8), TB_BLK);
    int cls = brush_size_cls(cur->brush, cur->size);
    for (int i = 0; i < TI_N; i++) {
        int x = L->cx[i], y = L->cyi[i], sel = 0, gx = x + (L->cell - TBI_SIZE) / 2, gy = y + 4;
        if (i < TI_BLACK) sel = cur->brush == i;
        else if (i < TI_SMALL) sel = tb_shade_of(i) == cur->shade;
        else if (i < TI_MIRROR) sel = i - TI_SMALL == cls;
        else if (i == TI_MIRROR) sel = cur->mirror != 0;
        int x1 = x + L->cell - 1, y1 = y + L->ch - 1;
        if (sel) gfx_rrect(g, (float)x, (float)y, (float)x1, (float)y1, 12, 0, TB_BLK, TB_BLK);
        if (hot == i) { /* the pen is over it: a frame (on a picked item an inset white one) */
            if (sel) gfx_rrect(g, (float)(x + 5), (float)(y + 5), (float)(x1 - 5), (float)(y1 - 5), 8, 3, TB_BLK, TB_WHT);
            else gfx_rrect(g, (float)x, (float)y, (float)x1, (float)y1, 12, 4, TB_WHT, TB_BLK);
        }
        int c = sel ? TB_WHT : TB_BLK, bg = sel ? TB_BLK : TB_WHT;
        if (i < TI_BLACK) tbi_brush(g, i, gx, gy, c, bg);
        else if (i < TI_SMALL) tbi_colour(g, tb_shade_of(i), gx, gy, c, bg);
        else if (i < TI_MIRROR) tbi_size(g, i - TI_SMALL, gx, gy, c, bg);
        else if (i == TI_MIRROR) tbi_mirror(g, cur->mirror, gx, gy, c, bg);
        else tbi_menu(g, gx, gy, c, bg);
        tb_caption(g, L, i, x, y, c);
    }
    gfx_noclip(g);
}
