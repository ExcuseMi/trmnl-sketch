/* The toolbar: a strip along one edge of the screen (the top, or the bottom when the touch bar is on top) with the
   brushes, the three shades, the three sizes, undo, redo and clear. Layout and hit test are pure (user frame, tested
   in every position); the drawing goes through gfx.h. SPDX-License-Identifier: MIT */
#ifndef TOOLBAR_H
#define TOOLBAR_H
#include "brush.h"
#include "gfx.h"

enum { TI_BRUSH = 0, TI_SHADE = BR_N, TI_SIZE = BR_N + SH_N, TI_UNDO = BR_N + SH_N + 3, TI_REDO, TI_CLEAR, TI_N };

typedef struct {
    int x0, y0, x1, y1; /* the strip, user frame, inclusive */
    int cell;           /* an item is cell x cell */
    int cx[TI_N], cy;   /* item left edges, common top edge */
    int top;            /* the strip is along the top edge (else the bottom) */
} tb_layout_t;

void tb_layout(tb_layout_t *L, int orient, int sw, int sh);
int tb_in(const tb_layout_t *L, int x, int y);  /* the point is on the strip */
int tb_hit(const tb_layout_t *L, int x, int y); /* the item under the point, or -1 */
/* Draws the strip; the current brush, shade and size are shown inverted */
void tb_draw(gfx_t *g, const tb_layout_t *L, const bstyle_t *cur);
#endif
