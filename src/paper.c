/* SPDX-License-Identifier: MIT */
#include "paper.h"
#include <string.h>

static const char *const names[PP_N] = { "Blank", "Grid", "Lines", "Dots" };

const char *pp_name(int k) { return names[k < 0 || k >= PP_N ? 0 : k]; }

int pp_from_name(const char *s, int n)
{
    for (int k = 1; k < PP_N; k++) {
        const char *w = names[k];
        int i = 0;
        for (; i < n && w[i]; i++)
            if ((s[i] | 0x20) != (w[i] | 0x20)) break;
        if (i == n && !w[i]) return k;
    }
    return PP_BLANK;
}

int pp_spacing(int sp) { return sp < PP_SP_MIN ? (sp <= 0 ? PP_SP_DEF : PP_SP_MIN) : sp > PP_SP_MAX ? PP_SP_MAX : sp; }

int pp_black(int kind, int sp, int x, int y)
{
    if (sp < PP_SP_MIN) sp = PP_SP_MIN;
    switch (kind) {
    case PP_GRID: return (y % sp == sp - 1 && x % 3 == 0) || (x % sp == sp - 1 && y % 3 == 0); /* dotted lines: one pixel in three */
    case PP_LINES: return y % sp == sp - 1 && x % 3 == 0;
    case PP_DOTS: return x % sp < 2 && y % sp < 2;
    default: return 0;
    }
}

void pp_overlay(canvas_t *c, const box_t *b0, int kind, int sp)
{
    box_t b = *b0;
    if (kind == PP_BLANK || box_empty(&b)) return;
    box_clip(&b, c->w, c->h);
    if (box_empty(&b)) return;
    int c0 = b.x0 >> 3, c1 = b.x1 >> 3;
    for (int y = b.y0; y <= b.y1; y++)
        for (int cx = c0; cx <= c1; cx++) {
            unsigned m = 0;
            for (int k = 0; k < 8; k++)
                if (pp_black(kind, sp, cx * 8 + k, y)) m |= 0x80u >> k;
            if (m) c->px[y * c->stride + cx] &= (uint8_t)~m;
        }
}
