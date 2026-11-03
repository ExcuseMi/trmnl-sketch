/* SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 */
#include "mrpick.h"
#include "font.h"
#include "mirror.h"
#include "tbicons.h"

#define MP_CELL 140
#define MP_PAD 8
#define MP_GAP 6 /* from the strip */
#define MP_LINE 4
#define MP_CH 104
enum { MP_BLK = GFX_BLACK, MP_WHT = GFX_WHITE };

const char *mp_label(int mode)
{
    static const char *const n[MR_N] = { "Off", "Left-right", "Top-bottom", "4-way", "Kaleido" };
    return n[mode < 0 || mode >= MR_N ? 0 : mode];
}

void mp_layout(mp_layout_t *L, const tb_layout_t *tb, int sw)
{
    int w = MR_N * MP_CELL + 2 * MP_PAD;
    L->cell = MP_CELL, L->ch = MP_CH;
    L->x1 = tb->cx[TI_MIRROR] + tb->cell + MP_PAD - 1;
    if (L->x1 > sw - 1) L->x1 = sw - 1;
    L->x0 = L->x1 - w + 1;
    if (L->x0 < 0) L->x0 = 0, L->x1 = w - 1;
    int h = MP_CH + 2 * MP_PAD;
    if (tb->top) L->y0 = tb->y1 + 1 + MP_GAP;
    else L->y0 = tb->y0 - MP_GAP - h;
    L->y1 = L->y0 + h - 1;
    L->cy = L->y0 + MP_PAD;
    for (int i = 0; i < MR_N; i++) L->cx[i] = L->x0 + MP_PAD + i * MP_CELL;
}

int mp_in(const mp_layout_t *L, int x, int y) { return x >= L->x0 && x <= L->x1 && y >= L->y0 && y <= L->y1; }

int mp_hit(const mp_layout_t *L, int x, int y)
{
    if (y < L->y0 || y > L->y1) return -1;
    for (int i = 0; i < MR_N; i++)
        if (x >= L->cx[i] - (i ? 0 : MP_PAD) && x < L->cx[i] + L->cell + (i == MR_N - 1 ? MP_PAD : 0)) return i; /* the frame belongs to the end items */
    return -1;
}

void mp_draw(gfx_t *g, const mp_layout_t *L, int cur)
{
    const font_face_t *f = font_get(FONT_BOLD, 24);
    gfx_clip(g, L->x0, L->y0, L->x1, L->y1);
    gfx_rrect(g, (float)L->x0, (float)L->y0, (float)L->x1, (float)L->y1, 14, MP_LINE, MP_WHT, MP_BLK);
    for (int i = 0; i < MR_N; i++) {
        int x = L->cx[i], y = L->cy, sel = i == cur, c = sel ? MP_WHT : MP_BLK, bg = sel ? MP_BLK : MP_WHT;
        if (sel) gfx_rrect(g, (float)x, (float)y, (float)(x + L->cell - 1), (float)(y + L->ch - 1), 12, 0, MP_BLK, MP_BLK);
        tbi_mirror(g, i, x + (L->cell - TBI_SIZE) / 2, y + 4, c, bg);
        int w = (int)((font_width64(f, mp_label(i), 0) + 32) >> 6);
        gfx_text(g, f, (int32_t)(x + (L->cell - w) / 2) * 64, y + TBI_SIZE + 26, mp_label(i), 0, c);
    }
    gfx_noclip(g);
}
