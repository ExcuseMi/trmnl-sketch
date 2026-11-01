/* SPDX-License-Identifier: MIT */
#include "gallery.h"
#include <stdlib.h>
#include <string.h>

#define BAR 160   /* the touch bar's strip (APP_TOUCHBAR_DEPTH) */
#define MARGIN 60
#define TOP 170   /* the title above the grid */
#define GAPX 28
#define GAPY 24

void gal_layout(gal_layout_t *L, int orient, int sw, int sh)
{
    memset(L, 0, sizeof(*L));
    int side = orient == APP_ORIENT_BAR_LEFT || orient == APP_ORIENT_BAR_RIGHT;
    L->tw = side ? GAL_TH : GAL_TW;
    L->th = side ? GAL_TW : GAL_TH;
    L->ax0 = MARGIN + (orient == APP_ORIENT_BAR_LEFT ? BAR : 0);
    L->ax1 = sw - MARGIN - (orient == APP_ORIENT_BAR_RIGHT ? BAR : 0);
    L->ay0 = orient == APP_ORIENT_BAR_TOP ? BAR : 0;
    L->ay1 = sh - (orient == APP_ORIENT_BAR_BOTTOM ? BAR : 0) - 30;
    int aw = L->ax1 - L->ax0, ah = L->ay1 - L->ay0 - TOP;
    L->cols = (aw + GAPX) / (L->tw + GAPX);
    L->rowh = L->th + GAL_LABEL + GAPY;
    L->rows = (ah + GAPY) / L->rowh;
    if (L->cols < 1) L->cols = 1;
    if (L->rows < 1) L->rows = 1;
    L->gx = GAPX;
    L->per = L->cols * L->rows;
    L->ox = L->ax0 + (aw - (L->cols * L->tw + (L->cols - 1) * GAPX)) / 2;
    L->oy = L->ay0 + TOP;
}

void gal_tile_xy(const gal_layout_t *L, int slot, int *x, int *y)
{
    *x = L->ox + (slot % L->cols) * (L->tw + L->gx);
    *y = L->oy + (slot / L->cols) * L->rowh;
}

int gal_hit(const gal_layout_t *L, int x, int y)
{
    for (int i = 0; i < L->per; i++) {
        int tx, ty;
        gal_tile_xy(L, i, &tx, &ty);
        if (x >= tx && x < tx + L->tw && y >= ty && y < ty + L->th + GAL_LABEL) return i;
    }
    return -1;
}

void gal_from_fb(int orient, int fw, int fh, int fx, int fy, int *x, int *y)
{
    switch (orient) {
    case APP_ORIENT_BAR_LEFT: *x = fh - 1 - fy, *y = fx; break;
    case APP_ORIENT_BAR_TOP: *x = fw - 1 - fx, *y = fh - 1 - fy; break;
    case APP_ORIENT_BAR_RIGHT: *x = fy, *y = fw - 1 - fx; break;
    default: *x = fx, *y = fy; break;
    }
}

/* ---- thumbnails ---- */

void gal_cache_free(gal_cache_t *c)
{
    for (int i = 0; i < GAL_CACHE; i++) free(c->t[i].px);
    memset(c, 0, sizeof(*c));
}

void gal_invalidate(gal_cache_t *c, int num)
{
    for (int i = 0; i < GAL_CACHE; i++)
        if (c->t[i].num == num) c->t[i].num = 0; /* the buffer stays for the next one */
}

const uint8_t *gal_find(gal_cache_t *c, int num)
{
    for (int i = 0; i < GAL_CACHE; i++)
        if (c->t[i].num == num && c->t[i].px) return c->t[i].used = ++c->tick, c->t[i].px;
    return 0;
}

/* The area average of thumbnail cell (tx, ty) of src: 0 black .. 255 white */
static int cell_avg(const canvas_t *src, int tx, int ty)
{
    int y0 = ty * src->h / GAL_TH, y1 = (ty + 1) * src->h / GAL_TH, x0 = tx * src->w / GAL_TW, x1 = (tx + 1) * src->w / GAL_TW, white = 0, n = 0;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) white += (src->px[y * src->stride + (x >> 3)] >> (7 - (x & 7))) & 1, n++;
    return n ? (white * 255 + n / 2) / n : 255;
}

const uint8_t *gal_make(gal_cache_t *c, int num, const canvas_t *src)
{
    gal_thumb_t *slot = 0;
    for (int i = 0; i < GAL_CACHE && !slot; i++)
        if (!c->t[i].num) slot = &c->t[i];
    if (!slot) {
        slot = &c->t[0];
        for (int i = 1; i < GAL_CACHE; i++)
            if (c->t[i].used < slot->used) slot = &c->t[i];
    }
    if (!slot->px) slot->px = malloc(GAL_BYTES);
    int *err = malloc(sizeof(int) * 2 * (GAL_TW + 2));
    if (!slot->px || !err) return free(err), (const uint8_t *)0;
    memset(err, 0, sizeof(int) * 2 * (GAL_TW + 2));
    memset(slot->px, 0, GAL_BYTES);
    int *cur = err + 1, *nxt = err + GAL_TW + 3;
    for (int ty = 0; ty < GAL_TH; ty++) {
        for (int tx = 0; tx < GAL_TW; tx++) {
            int v = cell_avg(src, tx, ty), white;
            v += cur[tx]; /* Floyd-Steinberg: looked at against an ordered 8 x 8 matrix, which printed a dot grid on paper */
            white = v >= 128;
            int e = v - (white ? 255 : 0);
            cur[tx + 1] += e * 7 / 16;
            nxt[tx - 1] += e * 3 / 16;
            nxt[tx] += e * 5 / 16;
            nxt[tx + 1] += e / 16;
            if (white) slot->px[ty * GAL_ROW + (tx >> 3)] |= (uint8_t)(0x80 >> (tx & 7));
        }
        memcpy(cur - 1, nxt - 1, sizeof(int) * (GAL_TW + 2));
        memset(nxt - 1, 0, sizeof(int) * (GAL_TW + 2));
    }
    free(err);
    slot->num = num;
    slot->used = ++c->tick;
    c->made++;
    return slot->px;
}

/* ---- onto the screen ---- */

void gal_blit(canvas_t *dst, int orient, const uint8_t *t, int x, int y)
{
    int tw = orient == APP_ORIENT_BAR_LEFT || orient == APP_ORIENT_BAR_RIGHT ? GAL_TH : GAL_TW;
    int th = orient == APP_ORIENT_BAR_LEFT || orient == APP_ORIENT_BAR_RIGHT ? GAL_TW : GAL_TH;
    int ax, ay, bx, by;
    app_orient_to_fb(orient, dst->w, dst->h, x, y, &ax, &ay);
    app_orient_to_fb(orient, dst->w, dst->h, x + tw - 1, y + th - 1, &bx, &by);
    int f0x = ax < bx ? ax : bx, f0y = ay < by ? ay : by;
    for (int ty = 0; ty < GAL_TH; ty++) {
        int fy = f0y + ty;
        if (fy < 0 || fy >= dst->h) continue;
        for (int tx = 0; tx < GAL_TW; tx++) {
            int fx = f0x + tx;
            if (fx < 0 || fx >= dst->w) continue;
            uint8_t *d = &dst->px[fy * dst->stride + (fx >> 3)], m = (uint8_t)(0x80 >> (fx & 7));
            if (gal_bit(t, tx, ty)) *d |= m;
            else *d &= (uint8_t)~m;
        }
    }
}
