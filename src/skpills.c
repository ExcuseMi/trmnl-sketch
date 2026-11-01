/* SPDX-License-Identifier: MIT */
#include "skpills.h"
#include <app_api.h>
#include "font.h"

#define PL_W 150
#define PL_H 72
#define PL_GAP 14
#define PL_EDGE 16

void pl_layout(pills_t *p, int orient, int sw, int sh)
{
    int top_bar = orient == APP_ORIENT_BAR_TOP; /* the toolbar is at the bottom then */
    p->y0 = top_bar ? PL_EDGE : sh - PL_EDGE - PL_H;
    p->y1 = p->y0 + PL_H - 1;
    p->x1[1] = sw - PL_EDGE - 1;
    p->x0[1] = p->x1[1] - PL_W + 1;
    p->x1[0] = p->x0[1] - PL_GAP - 1;
    p->x0[0] = p->x1[0] - PL_W + 1;
}

int pl_hit(const pills_t *p, int x, int y)
{
    if (y < p->y0 - 12 || y > p->y1 + 12) return PL_NONE;
    for (int i = 0; i < 2; i++)
        if (x >= p->x0[i] - 8 && x <= p->x1[i] + 8) return i + 1;
    return PL_NONE;
}

void pl_draw(gfx_t *g, const pills_t *p)
{
    static const char *const label[2] = { "Tools", "Menu" };
    const font_face_t *f = font_get(FONT_BOLD, 34);
    for (int i = 0; i < 2; i++) {
        gfx_rrect(g, p->x0[i], p->y0, p->x1[i], p->y1, PL_H / 2, 3, GFX_WHITE, GFX_BLACK);
        int w = (int)((font_width64(f, label[i], 0) + 32) >> 6);
        gfx_text(g, f, (int32_t)(p->x0[i] + (PL_W - w) / 2) * 64, p->y0 + (PL_H + f->digit_h) / 2, label[i], 0, GFX_BLACK);
    }
}
