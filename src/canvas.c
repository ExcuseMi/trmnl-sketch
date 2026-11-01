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

static void span(canvas_t *c, int y, int x0, int x1, int colour)
{
    uint8_t *row = c->px + y * c->stride;
    while (x0 <= x1 && (x0 & 7)) {
        uint8_t m = (uint8_t)(0x80 >> (x0 & 7));
        row[x0 >> 3] = colour ? (uint8_t)(row[x0 >> 3] | m) : (uint8_t)(row[x0 >> 3] & ~m);
        x0++;
    }
    while (x0 + 7 <= x1) {
        row[x0 >> 3] = colour ? 0xff : 0;
        x0 += 8;
    }
    while (x0 <= x1) {
        uint8_t m = (uint8_t)(0x80 >> (x0 & 7));
        row[x0 >> 3] = colour ? (uint8_t)(row[x0 >> 3] | m) : (uint8_t)(row[x0 >> 3] & ~m);
        x0++;
    }
}

/* floor(sqrt(n)) */
static int isqrt(int n)
{
    int r = 0;
    while ((r + 1) * (r + 1) <= n) r++;
    return r;
}

void cv_disc(canvas_t *c, int cx, int cy, int d, int colour, box_t *dirty)
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
        span(c, y, x0, x1, colour);
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
