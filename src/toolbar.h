/* The toolbar: a strip along one edge of the screen (the top, or the bottom when the touch bar is on top) with the
   brushes, the three shades, the size slider, undo, redo and clear, each a glyph with a label. Layout and hit test are pure (user frame, tested
   in every position); the drawing goes through gfx.h. SPDX-License-Identifier: MIT */
#ifndef TOOLBAR_H
#define TOOLBAR_H
#include "brush.h"
#include "gfx.h"

/* TI_MIRROR cycles the symmetry (mirror.h); it is shown picked while one is on. TI_SIZE is the slider: it takes the width of three cells (tb_hit gives TI_SIZE for each of them) */
enum { TI_BRUSH = 0, TI_SHADE = BR_N, TI_SIZE = BR_N + SH_N, TI_MIRROR = BR_N + SH_N + 3, TI_UNDO, TI_REDO, TI_CLEAR, TI_N };

typedef struct {
    int x0, y0, x1, y1; /* the strip, user frame, inclusive */
    int cell, ch;       /* an item is cell wide and ch high: a 64 px glyph with its short label under it */
    int cx[TI_N], cy;   /* item left edges, common top edge */
    int gap_x[3];       /* centres of the dividers between the groups (brushes | shades | sizes | actions) */
    int sl_x0, sl_x1, sl_y; /* the size slider: the knob's centre runs along the track from sl_x0 to sl_x1 at sl_y, user frame */
    int top;            /* the strip is along the top edge (else the bottom) */
} tb_layout_t;

void tb_layout(tb_layout_t *L, int orient, int sw, int sh);
int tb_in(const tb_layout_t *L, int x, int y);  /* the point is on the strip */
int tb_hit(const tb_layout_t *L, int x, int y); /* the item under the point, or -1 */
/* Draws the strip; the current brush, shade and size are shown inverted, item `hot` (-1: none) has a frame (the pen is over it) */
/* The slider: size 1..48 (SK_SIZE_MIN..MAX) is the knob's diameter in px; the track is a curve (finer at the thin end).
   x is clamped to the track; the size of the x it gives is monotonic, and tb_slider_x(size) is the first x of that size */
int tb_slider_size(const tb_layout_t *L, int x);
int tb_slider_x(const tb_layout_t *L, int size);
/* The slider's cells, user frame (what a drag redraws) */
void tb_slider_box(const tb_layout_t *L, int *x0, int *y0, int *x1, int *y1);
/* ... `hot` == TI_SIZE: the current brush's footprint is drawn over the knob (clipped to the slider) */
void tb_draw(gfx_t *g, const tb_layout_t *L, const bstyle_t *cur, int hot);
#endif
