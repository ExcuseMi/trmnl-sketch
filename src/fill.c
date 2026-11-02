/* SPDX-License-Identifier: MIT */
#include "fill.h"
#include <stdlib.h>
#include <string.h>

/* Classes of look: 0 white, then a shade + 1 (1 black, 2 .. SH_N the dithers, darkest first) */
typedef struct {
    const canvas_t *pg;
    uint8_t *dil, *vis;
    uint8_t pw[4]; /* the class's white-bit byte of row y & 3 (the patterns repeat every 4 px, so one byte fits all) */
    int w, h, stride;
} bfc_t;

static int bf_pat_black(int cls, int x, int y) { return cls == 0 ? 0 : cv_shade_black(cls - 1, x, y); }

static void bf_setup(bfc_t *k, int cls)
{
    for (int y = 0; y < 4; y++) {
        uint8_t w = 0;
        for (int x = 0; x < 8; x++) w = (uint8_t)(w << 1 | !bf_pat_black(cls, x, y));
        k->pw[y] = w;
    }
}

/* bits of the pixels of byte xb in row y that look like the class */
static inline uint8_t bf_match(const bfc_t *k, int xb, int y) { return (uint8_t)~(k->pg->px[y * k->stride + xb] ^ k->pw[y & 3]); }
static inline uint8_t bf_ok(const bfc_t *k, int xb, int y)
{
    uint8_t m = bf_match(k, xb, y) & (uint8_t)~k->vis[y * k->stride + xb];
    if (k->dil) m &= (uint8_t)~k->dil[y * k->stride + xb];
    if (xb == k->stride - 1 && (k->w & 7)) m &= (uint8_t)(0xff << (8 - (k->w & 7)));
    return m;
}
static inline int bf_okp(const bfc_t *k, int x, int y) { return bf_ok(k, x >> 3, y) >> (7 - (x & 7)) & 1; }

static void bf_mark(bfc_t *k, int y, int x0, int x1)
{
    uint8_t *row = k->vis + y * k->stride;
    for (int b = x0 >> 3; b <= x1 >> 3; b++) {
        uint8_t m = 0xff;
        if (b == x0 >> 3) m &= (uint8_t)(0xff >> (x0 & 7));
        if (b == x1 >> 3) m &= (uint8_t)(0xff << (7 - (x1 & 7)));
        row[b] |= m;
    }
}

typedef struct {
    int16_t *xy;
    size_t n, cap;
    int lost, lo, hi; /* a seed did not fit: the rows to rescan */
} bfs_t;

static void bf_push(bfs_t *s, int x, int y)
{
    if (s->n == s->cap) {
        if (!s->lost || y < s->lo) s->lo = y;
        if (!s->lost || y > s->hi) s->hi = y;
        s->lost = 1;
        return;
    }
    s->xy[2 * s->n] = (int16_t)x, s->xy[2 * s->n + 1] = (int16_t)y, s->n++;
}

/* The pixels of row ny (from xl to xr) that can be filled: a seed for the start of each run */
static void bf_seeds(const bfc_t *k, bfs_t *s, int ny, int xl, int xr)
{
    int in = 0;
    for (int x = xl; x <= xr; x++) {
        if (!(x & 7) && x + 7 <= xr && !bf_ok(k, x >> 3, ny)) { /* a whole byte nothing of which can be filled */
            in = 0, x += 7;
            continue;
        }
        int o = bf_okp(k, x, ny);
        if (o && !in) bf_push(s, x, ny);
        in = o;
    }
}

static void bf_drain(bfc_t *k, bfs_t *s)
{
    while (s->n) {
        s->n--;
        int x = s->xy[2 * s->n], y = s->xy[2 * s->n + 1];
        if (!bf_okp(k, x, y)) continue;
        int xl = x, xr = x;
        while (xl > 0) {
            if (!(xl & 7) && xl >= 8 && bf_ok(k, (xl >> 3) - 1, y) == 0xff) { xl -= 8; continue; }
            if (!bf_okp(k, xl - 1, y)) break;
            xl--;
        }
        while (xr < k->w - 1) {
            if (!((xr + 1) & 7) && xr + 8 < k->w && bf_ok(k, (xr + 1) >> 3, y) == 0xff) { xr += 8; continue; }
            if (!bf_okp(k, xr + 1, y)) break;
            xr++;
        }
        bf_mark(k, y, xl, xr);
        if (y > 0) bf_seeds(k, s, y - 1, xl, xr);
        if (y < k->h - 1) bf_seeds(k, s, y + 1, xl, xr);
    }
}

/* The flood from (x, y); a stack that overflowed is repaired by rescanning the rows around the lost seeds: filled
   pixels with a fillable neighbour above or below become seeds again. Gives up after BF_PASSES rescans. */
#define BF_PASSES 64
static int bf_flood(bfc_t *k, int x, int y, size_t cap)
{
    bfs_t s = { malloc(cap * 4), 0, cap, 0, 0, 0 };
    if (!s.xy) return BF_MEM;
    bf_push(&s, x, y);
    bf_drain(k, &s);
    for (int pass = 0; s.lost && pass < BF_PASSES; pass++) {
        int lo = s.lo > 0 ? s.lo - 1 : 0, hi = s.hi < k->h - 1 ? s.hi + 1 : k->h - 1;
        s.lost = 0;
        for (int yy = lo; yy <= hi; yy++)
            for (int b = 0; b < k->stride; b++) {
                uint8_t v = k->vis[yy * k->stride + b];
                if (!v) continue;
                for (int d = -1; d <= 1; d += 2) {
                    int ny = yy + d;
                    if (ny < 0 || ny >= k->h) continue;
                    uint8_t c = (uint8_t)(v & bf_ok(k, b, ny));
                    for (int i = 0; c && i < 8; i++)
                        if (c >> (7 - i) & 1) bf_push(&s, b * 8 + i, ny);
                }
            }
        bf_drain(k, &s);
    }
    int lost = s.lost;
    free(s.xy);
    return lost ? BF_COMPLEX : BF_OK;
}

/* ---- dilation of a bit mask by a square of radius g, in place ---- */

static void bf_dil(uint8_t *m, int stride, int h, int g, uint8_t *r0, uint8_t *r1)
{
    for (int p = 0; p < g; p++)
        for (int y = 0; y < h; y++) {
            uint8_t *row = m + y * stride, prev = 0;
            for (int i = 0; i < stride; i++) {
                uint8_t o = row[i], nx = i + 1 < stride ? row[i + 1] : 0;
                row[i] = (uint8_t)(o | (o << 1 | nx >> 7) | (o >> 1 | prev << 7));
                prev = o;
            }
        }
    for (int p = 0; p < g; p++) {
        uint8_t *up = r0, *cur = r1;
        memset(up, 0, (size_t)stride);
        for (int y = 0; y < h; y++) {
            uint8_t *row = m + y * stride;
            memcpy(cur, row, (size_t)stride);
            for (int i = 0; i < stride; i++) row[i] |= (uint8_t)(up[i] | (y + 1 < h ? row[stride + i] : 0));
            uint8_t *t = up;
            up = cur, cur = t;
        }
    }
}

/* ---- the class of the tapped spot ---- */

static int bf_class(const canvas_t *pg, int x, int y)
{
    int seed_black = !cv_get(pg, x, y);
    int best = -1, best_score = -1;
    int order[SH_N + 1]; /* white first on a tie, then the lightest dither */
    for (int i = 0; i <= SH_N; i++) order[i] = i ? SH_N + 1 - i : 0;
    int x0 = (x & ~3) - 2, y0 = (y & ~3) - 2;
    for (int oi = 0; oi <= SH_N; oi++) {
        int c = order[oi];
        if (bf_pat_black(c, x, y) != seed_black) continue;
        int cell = 1, score = 0;
        for (int yy = y0; yy < y0 + 8; yy++)
            for (int xx = x0; xx < x0 + 8; xx++) {
                if (xx < 0 || yy < 0 || xx >= pg->w || yy >= pg->h) continue;
                int m = (!cv_get(pg, xx, yy)) == bf_pat_black(c, xx, yy);
                score += m;
                if (!m && xx >= (x & ~3) && xx < (x & ~3) + 4 && yy >= (y & ~3) && yy < (y & ~3) + 4) cell = 0;
            }
        if (cell) return c; /* its own 4 x 4 cell is that look through and through */
        if (score > best_score) best_score = score, best = c;
    }
    return best;
}

/* ---- the region ---- */

void bf_free(bf_t *r)
{
    free(r->mask);
    r->mask = 0;
}

static void bf_box(bf_t *r)
{
    box_clear(&r->bb);
    r->px = 0;
    for (int y = 0; y < r->h; y++)
        for (int b = 0; b < r->stride; b++) {
            uint8_t v = r->mask[y * r->stride + b];
            if (!v) continue;
            r->px += (uint32_t)__builtin_popcount(v);
            for (int i = 0; i < 8; i++)
                if (v >> (7 - i) & 1) box_add(&r->bb, b * 8 + i, y);
        }
}

int bf_plan(bf_t *r, const canvas_t *page, int x, int y, int shade, int gap, size_t stack_cap)
{
    memset(r, 0, sizeof(*r));
    if (x < 0 || y < 0 || x >= page->w || y >= page->h) return BF_NOTHING;
    if (shade < 0 || shade > SH_WHITE) shade = 0;
    int cls = bf_class(page, x, y);
    if (cls == (shade == SH_WHITE ? 0 : shade + 1)) return BF_NOTHING;
    if (gap < 0) gap = 0;
    if (gap > BF_GAP_MAX) gap = BF_GAP_MAX;
    if (cls == 1) gap = 0; /* black: plain connected pixels */
    else if (cls != 0 && gap < 1) gap = 1; /* a dithered look: the dots of a line that match it must not be flooded into */
    if (!stack_cap) stack_cap = BF_STACK;
    size_t bytes = (size_t)page->stride * (size_t)page->h;
    bfc_t k = { page, 0, 0, { 0 }, page->w, page->h, page->stride };
    bf_setup(&k, cls);
    uint8_t *vis = calloc(1, bytes), *dil = gap ? malloc(bytes) : 0, *tmp = malloc(bytes), *rows = malloc(2 * (size_t)page->stride);
    int rc = BF_MEM;
    if (!vis || !tmp || !rows || (gap && !dil)) goto out;
    k.vis = vis;
    if (gap) { /* the lines (what does not look like the class) grown by the gap */
        for (int yy = 0; yy < k.h; yy++)
            for (int b = 0; b < k.stride; b++) dil[yy * k.stride + b] = (uint8_t)~bf_match(&k, b, yy);
        for (int g = gap;; g--) { /* a tap within the gap of a line: a smaller gap */
            memcpy(tmp, dil, bytes);
            bf_dil(tmp, k.stride, k.h, g, rows, rows + k.stride);
            if (g == 0 || !(tmp[y * k.stride + (x >> 3)] >> (7 - (x & 7)) & 1)) {
                gap = g;
                memcpy(dil, tmp, bytes);
                break;
            }
        }
    }
    k.dil = gap ? dil : 0;
    rc = bf_flood(&k, x, y, stack_cap);
    if (rc != BF_OK) goto out;
    if (gap) { /* the band between the flood and the lines: M and D pixels within the gap of the flood */
        memcpy(tmp, vis, bytes);
        bf_dil(tmp, k.stride, k.h, gap, rows, rows + k.stride);
        for (int yy = 0; yy < k.h; yy++)
            for (int b = 0; b < k.stride; b++) vis[yy * k.stride + b] |= (uint8_t)(tmp[yy * k.stride + b] & dil[yy * k.stride + b] & bf_match(&k, b, yy));
    }
    if (cls != 0 && cls != 1) { /* black dots touching a line stay as they are: the outline is never eaten */
        for (int yy = 0; yy < k.h; yy++)
            for (int b = 0; b < k.stride; b++) {
                uint8_t keep = 0;
                for (int d = -1; d <= 1; d++) {
                    int ny = yy + d;
                    if (ny < 0 || ny >= k.h) continue;
                    const uint8_t *pr = page->px + ny * k.stride, *vr = vis + ny * k.stride;
                    uint8_t n = (uint8_t)(~pr[b] & ~vr[b]), nl = b ? (uint8_t)(~pr[b - 1] & ~vr[b - 1]) : 0, nr = b + 1 < k.stride ? (uint8_t)(~pr[b + 1] & ~vr[b + 1]) : 0;
                    if (d) keep |= n;
                    else keep |= (uint8_t)((n << 1 | nr >> 7) | (n >> 1 | nl << 7));
                }
                tmp[yy * k.stride + b] = (uint8_t)(keep & ~page->px[yy * k.stride + b] & vis[yy * k.stride + b]);
            }
        for (size_t i = 0; i < bytes; i++) vis[i] &= (uint8_t)~tmp[i];
    }
    r->mask = vis, r->w = k.w, r->h = k.h, r->stride = k.stride;
    vis = 0;
    bf_box(r);
    if (!r->px) bf_free(r), rc = BF_NOTHING;
    else rc = BF_OK;
out:
    free(vis), free(dil), free(tmp), free(rows);
    return rc;
}

void bf_merge(bf_t *dst, bf_t *src)
{
    if (!src->mask) return;
    if (!dst->mask) *dst = *src, src->mask = 0;
    else {
        size_t bytes = (size_t)dst->stride * (size_t)dst->h;
        for (size_t i = 0; i < bytes; i++) dst->mask[i] |= src->mask[i];
        bf_free(src);
        bf_box(dst);
    }
}

/* ---- runs ---- */

/* One pass for both: with out == NULL it only counts */
static uint32_t bf_walk(const bf_t *r, hpt_t *out)
{
    uint32_t n = 0;
    int prev = 0;
    for (int y = r->bb.y0; y <= r->bb.y1; y++) {
        const uint8_t *row = r->mask + y * r->stride;
        int x = r->bb.x0;
        while (x <= r->bb.x1) {
            if (!(row[x >> 3] >> (7 - (x & 7)) & 1)) { x++; continue; }
            int x1 = x;
            while (x1 + 1 <= r->bb.x1 && (row[(x1 + 1) >> 3] >> (7 - ((x1 + 1) & 7)) & 1)) x1++;
            while (y - prev > 255) {
                if (out) out[n] = (hpt_t){ -1, -1, 255, 0 };
                n++, prev += 255;
            }
            if (out) out[n] = (hpt_t){ (int16_t)x, (int16_t)x1, (uint8_t)(y - prev), 0 };
            n++, prev = y;
            x = x1 + 1;
        }
    }
    return n;
}
uint32_t bf_runs(const bf_t *r) { return bf_walk(r, 0); }
void bf_emit(const bf_t *r, hpt_t *out) { (void)bf_walk(r, out); }

void bf_apply(canvas_t *c, const hpt_t *runs, uint32_t n, int shade)
{
    int y = 0;
    for (uint32_t i = 0; i < n; i++) {
        y += runs[i].d;
        if (runs[i].x >= 0) cv_span_pat(c, y, runs[i].x, runs[i].y, shade);
    }
}
