/* SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 */
#include "rects.h"

void rs_init(rset_t *s) { s->n = 0; }

static long area(const box_t *b) { return (long)(b->x1 - b->x0 + 1) * (long)(b->y1 - b->y0 + 1); }
static box_t uni(const box_t *a, const box_t *b)
{
    box_t u = *a;
    box_union(&u, b);
    return u;
}
static int touches(const box_t *a, const box_t *b) { return a->x0 <= b->x1 + 1 && b->x0 <= a->x1 + 1 && a->y0 <= b->y1 + 1 && b->y0 <= a->y1 + 1; }

void rs_add(rset_t *s, const box_t *in, int w, int h)
{
    box_t b = *in;
    box_clip(&b, w, h);
    if (box_empty(&b)) return;
    b.x0 &= ~7;
    b.x1 |= 7;
    if (b.x1 > ((w + 7) & ~7) - 1) b.x1 = ((w + 7) & ~7) - 1;
    for (;;) { /* merge into a neighbour while that wastes little */
        int hit = -1;
        for (int i = 0; i < s->n; i++) {
            box_t u = uni(&s->b[i], &b);
            if (touches(&s->b[i], &b) && area(&u) <= (area(&s->b[i]) + area(&b)) * 13 / 10 + 2048) {
                hit = i;
                break;
            }
        }
        if (hit < 0) break;
        b = uni(&s->b[hit], &b);
        s->b[hit] = s->b[--s->n];
    }
    if (s->n == SK_MAX_RECTS) { /* full: the pair that wastes the least becomes one */
        int bi = -1, bj = -1;
        long best = 0x7fffffffL;
        box_t all[SK_MAX_RECTS + 1];
        for (int i = 0; i < s->n; i++) all[i] = s->b[i];
        all[s->n] = b;
        for (int i = 0; i <= s->n; i++)
            for (int j = i + 1; j <= s->n; j++) {
                box_t u = uni(&all[i], &all[j]);
                long waste = area(&u) - area(&all[i]) - area(&all[j]);
                if (waste < best) best = waste, bi = i, bj = j;
            }
        all[bi] = uni(&all[bi], &all[bj]);
        all[bj] = all[s->n];
        for (int i = 0; i < s->n; i++) s->b[i] = all[i];
        return;
    }
    s->b[s->n++] = b;
}

int rs_take(rset_t *s, rect_t *out)
{
    int n = s->n;
    for (int i = 0; i < n; i++)
        out[i] = (rect_t){ (int16_t)s->b[i].x0, (int16_t)s->b[i].y0, (int16_t)(s->b[i].x1 - s->b[i].x0 + 1), (int16_t)(s->b[i].y1 - s->b[i].y0 + 1), 0 };
    s->n = 0;
    return n;
}
