/* The toolbar: one row along one edge of the screen (the top, or the bottom when the touch bar is on top): the seven tools,
   the three colours, the three sizes and Mirror, each a glyph with a short label. Undo, Redo and Clear are the tablet's
   buttons only. Layout and hit test are pure (user frame, tested in every position); the drawing goes through gfx.h.
   SPDX-License-Identifier: MIT */
#ifndef TOOLBAR_H
#define TOOLBAR_H
#include "brush.h"
#include "gfx.h"

/* The tools are the brush ids (TI_BRUSH + BR_*); then the colours (Black, Grey, White), the sizes (Small, Medium, Big) and Mirror, which cycles the symmetry (mirror.h) and is shown picked while one is on */
enum { TI_BRUSH = 0, TI_BLACK = BR_N, TI_GREY, TI_WHITE, TI_SMALL, TI_MEDIUM, TI_BIG, TI_MIRROR, TI_N };
#define TB_GAPS 3 /* dividers: before the colours, the sizes and Mirror */

typedef struct {
    int x0, y0, x1, y1; /* the strip, user frame, inclusive */
    int cell, ch;       /* an item is cell wide and ch high: a 64 px glyph with its short label under it */
    int cx[TI_N], cyi[TI_N], cy; /* item left and top edges (cy: the row's top) */
    int gap_x[TB_GAPS]; /* centres of the dividers */
    int top;            /* the strip is along the top edge (else the bottom) */
} tb_layout_t;

void tb_layout(tb_layout_t *L, int orient, int sw, int sh);
int tb_in(const tb_layout_t *L, int x, int y);  /* the point is on the strip */
int tb_hit(const tb_layout_t *L, int x, int y); /* the item under the point, or -1 */
/* The shade a colour item stands for (Black, Grey, White), or -1 for another item; and the colour item of a shade, or -1 */
int tb_shade_of(int item);
int tb_item_of_shade(int shade);
const char *tb_label(int item); /* the name under a toolbar button */
/* Draws the strip; the current brush, colour and size are shown inverted, item `hot` (-1: none) has a frame (the pen is over it) */
void tb_draw(gfx_t *g, const tb_layout_t *L, const bstyle_t *cur, int hot);
#endif
