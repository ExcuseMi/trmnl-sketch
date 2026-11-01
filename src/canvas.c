/* SPDX-License-Identifier: MIT */
#include "canvas.h"
#include <string.h>

void box_clear(box_t *b)
{
    b->x0 = b->y0 = 0x7fffffff;
    b->x1 = b->y1 = -1;
}
int box_empty(const box_t *b) { return b->x0 > b->x1 || b->y0 > b->y1; }
void box_add(box_t *b, int x, int y)
{
    if (x < b->x0) b->x0 = x;
    if (y < b->y0) b->y0 = y;
    if (x > b->x1) b->x1 = x;
    if (y > b->y1) b->y1 = y;
}
void box_union(box_t *b, const box_t *o)
{
    if (box_empty(o)) return;
    box_add(b, o->x0, o->y0);
    box_add(b, o->x1, o->y1);
}
void box_clip(box_t *b, int w, int h)
{
    if (b->x0 < 0) b->x0 = 0;
    if (b->y0 < 0) b->y0 = 0;
    if (b->x1 > w - 1) b->x1 = w - 1;
    if (b->y1 > h - 1) b->y1 = h - 1;
}

void cv_fill(canvas_t *c, int colour) { memset(c->px, colour ? 0xff : 0, (size_t)c->stride * (size_t)c->h); }

int cv_get(const canvas_t *c, int x, int y)
{
    if (x < 0 || y < 0 || x >= c->w || y >= c->h) return 1;
    return (c->px[y * c->stride + (x >> 3)] >> (7 - (x & 7))) & 1;
}

/* The 4 x 4 ordered dither (Bayer) of the shades: a pixel is black where its threshold is below the level */
static const uint8_t bayer[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };
static const uint8_t shade_level[SH_N] = { 16, 11, 5 };

int cv_shade_black(int shade, int x, int y)
{
    if (shade <= 0 || shade >= SH_N) return 1;
    return bayer[y & 3][x & 3] < shade_level[shade];
}

/* One row's byte mask of the black pixels of a shade (the pattern repeats every 4 pixels, MSB leftmost) */
static uint8_t shade_mask(int shade, int y)
{
    int nib = 0;
    for (int x = 0; x < 4; x++) nib = nib << 1 | cv_shade_black(shade, x, y);
    return (uint8_t)(nib << 4 | nib);
}

/* x0..x1 of row y, already clipped: colour 1 whitens, colour 0 blackens where `mask` (a byte of black pixels) says */
static void span_m(canvas_t *c, int y, int x0, int x1, int colour, uint8_t mask)
{
    uint8_t *row = c->px + y * c->stride;
    for (int b = x0 >> 3; b <= x1 >> 3; b++) {
        uint8_t m = mask;
        if (b == x0 >> 3) m &= (uint8_t)(0xff >> (x0 & 7));
        if (b == x1 >> 3) m &= (uint8_t)(0xff << (7 - (x1 & 7)));
        row[b] = colour ? (uint8_t)(row[b] | m) : (uint8_t)(row[b] & ~m);
    }
}

/* Sets x0..x1 of row y to the shade's pattern exactly (black and white dots; the bucket fill) */
void cv_span_pat(canvas_t *c, int y, int x0, int x1, int shade)
{
    if (y < 0 || y >= c->h) return;
    if (x0 < 0) x0 = 0;
    if (x1 > c->w - 1) x1 = c->w - 1;
    if (x0 > x1) return;
    uint8_t *row = c->px + y * c->stride, pw = (uint8_t)~shade_mask(shade, y);
    for (int b = x0 >> 3; b <= x1 >> 3; b++) {
        uint8_t m = 0xff;
        if (b == x0 >> 3) m &= (uint8_t)(0xff >> (x0 & 7));
        if (b == x1 >> 3) m &= (uint8_t)(0xff << (7 - (x1 & 7)));
        row[b] = (uint8_t)((row[b] & ~m) | (pw & m));
    }
}

void cv_span_s(canvas_t *c, int y, int x0, int x1, int colour, int shade)
{
    if (y < 0 || y >= c->h) return;
    if (x0 < 0) x0 = 0;
    if (x1 > c->w - 1) x1 = c->w - 1;
    if (x0 > x1) return;
    span_m(c, y, x0, x1, colour, colour ? 0xff : shade_mask(shade, y));
}

void cv_px_s(canvas_t *c, int x, int y, int colour, int shade)
{
    if (x < 0 || y < 0 || x >= c->w || y >= c->h) return;
    if (!colour && !cv_shade_black(shade, x, y)) return;
    uint8_t m = (uint8_t)(0x80 >> (x & 7));
    uint8_t *b = c->px + y * c->stride + (x >> 3);
    *b = colour ? (uint8_t)(*b | m) : (uint8_t)(*b & ~m);
}

/* floor(sqrt(n)) */
static int isqrt(int n)
{
    int r = 0;
    while ((r + 1) * (r + 1) <= n) r++;
    return r;
}

void cv_disc(canvas_t *c, int cx, int cy, int d, int colour, box_t *dirty) { cv_disc_s(c, cx, cy, d, colour, 0, dirty); }

void cv_disc_s(canvas_t *c, int cx, int cy, int d, int colour, int shade, box_t *dirty)
{
    if (d < 1) d = 1;
    int r = d / 2; /* d = 1: one pixel; d = 2r + 1 odd discs, d even a little rounder */
    int r2 = d * d / 4 + (d > 2 ? d / 4 : 0);
    for (int dy = -r; dy <= r; dy++) {
        int y = cy + dy, hw = isqrt(r2 - dy * dy);
        if (y < 0 || y >= c->h) continue;
        int x0 = cx - hw, x1 = cx + hw;
        if (x0 < 0) x0 = 0;
        if (x1 > c->w - 1) x1 = c->w - 1;
        if (x0 > x1) continue;
        span_m(c, y, x0, x1, colour, colour ? 0xff : shade_mask(shade, y));
        if (dirty) box_add(dirty, x0, y), box_add(dirty, x1, y);
    }
}

void cv_seg(canvas_t *c, int x0, int y0, int d0, int x1, int y1, int d1, int colour, box_t *dirty)
{
    int dx = x1 - x0, dy = y1 - y0;
    int adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
    int n = adx > ady ? adx : ady;
    if (n == 0) return cv_disc(c, x0, y0, d1, colour, dirty);
    for (int i = 0; i <= n; i++) {
        int x = x0 + (dx * i + (dx < 0 ? -n / 2 : n / 2)) / n;
        int y = y0 + (dy * i + (dy < 0 ? -n / 2 : n / 2)) / n;
        int d = d0 + ((d1 - d0) * i + ((d1 - d0) < 0 ? -n / 2 : n / 2)) / n;
        cv_disc(c, x, y, d, colour, dirty);
    }
}

void cv_copy_box(canvas_t *dst, const canvas_t *src, const box_t *b0)
{
    box_t b = *b0;
    if (box_empty(&b)) return;
    box_clip(&b, dst->w, dst->h);
    if (box_empty(&b)) return;
    int c0 = b.x0 >> 3, c1 = b.x1 >> 3;
    for (int y = b.y0; y <= b.y1; y++) memcpy(dst->px + y * dst->stride + c0, src->px + y * src->stride + c0, (size_t)(c1 - c0 + 1));
}

void cv_flip(canvas_t *c, int x, int y)
{
    if (x < 0 || y < 0 || x >= c->w || y >= c->h) return;
    c->px[y * c->stride + (x >> 3)] ^= (uint8_t)(0x80 >> (x & 7));
}

void cv_set(canvas_t *c, int x, int y, int colour)
{
    if (x < 0 || y < 0 || x >= c->w || y >= c->h) return;
    uint8_t *p = &c->px[y * c->stride + (x >> 3)], m = (uint8_t)(0x80 >> (x & 7));
    if (colour) *p |= m;
    else *p &= (uint8_t)~m;
}
