/* The toolbar: a strip along one edge of the screen (the top, or the bottom when the touch bar is on top) with the
   brushes, the three shades, the size slider, undo, redo and clear, each a glyph with a label. Layout and hit test are pure (user frame, tested
   in every position); the drawing goes through gfx.h. SPDX-License-Identifier: MIT */
#ifndef TOOLBAR_H
#define TOOLBAR_H
#include "brush.h"
#include "gfx.h"

/* TI_SMOOTH cycles the stabiliser (sk_set_smooth, 0 3 6 10); TI_MIRROR cycles the symmetry (mirror.h); it is shown picked while one is on. TI_TONE and TI_SIZE are sliders: each takes the width of three cells (tb_hit gives TI_TONE / TI_SIZE for each of them) */
enum { TI_BRUSH = 0, TI_LINE = BR_N, TI_RECT, TI_OVAL, TI_TEXT, TI_SMOOTH, TI_TONE, TI_SIZE = TI_TONE + 3, TI_MIRROR = TI_SIZE + 3, TI_UNDO, TI_REDO, TI_CLEAR, TI_RECENT, TI_N = TI_RECENT + 5 };
/* Two rows: brushes and the shape and text tools, then the shades, size slider, mirror and the actions (a row of 22 cells would not fit a portrait panel) */
#define TB_ROWS 2
#define TB_ROW_OF(i) ((i) < TI_TONE ? 0 : 1)

typedef struct {
    int x0, y0, x1, y1; /* the strip, user frame, inclusive */
    int cell, ch;       /* an item is cell wide and ch high: a 64 px glyph with its short label under it */
    int cx[TI_N], cyi[TI_N], cy; /* item left and top edges (cy: the first row's top) */
    int gap_x[4], gap_row[4]; /* centres of the dividers before Line, the size slider, Mirror and the Recent slots, and their row */
    int tn_x0, tn_x1, tn_y; /* the tone slider: the knob runs from tn_x0 (lightest) to tn_x1 (black) at tn_y */
    int sl_x0, sl_x1, sl_y; /* the size slider: the knob's centre runs along the track from sl_x0 to sl_x1 at sl_y, user frame */
    int top;            /* the strip is along the top edge (else the bottom) */
} tb_layout_t;

void tb_layout(tb_layout_t *L, int orient, int sw, int sh);
int tb_in(const tb_layout_t *L, int x, int y);  /* the point is on the strip */
int tb_hit(const tb_layout_t *L, int x, int y); /* the item under the point, or -1 */
/* Draws the strip (smooth: the stabiliser level the Smooth cell shows); the current brush, shade and size are shown inverted, item `hot` (-1: none) has a frame (the pen is over it) */
/* The slider: size 1..48 (SK_SIZE_MIN..MAX) is the knob's diameter in px; the track is a curve (finer at the thin end).
   x is clamped to the track; the size of the x it gives is monotonic, and tb_slider_x(size) is the first x of that size */
int tb_slider_size(const tb_layout_t *L, int x);
int tb_slider_x(const tb_layout_t *L, int size);
/* The slider's cells, user frame (what a drag redraws) */
void tb_slider_box(const tb_layout_t *L, int *x0, int *y0, int *x1, int *y1);
/* The tone slider: 9 steps (canvas.h SH_N), light at the left to black at the right; tb_tone_of gives the shade (0 black .. 8 lightest) for an x, tb_tone_x its knob's x */
int tb_tone_of(const tb_layout_t *L, int x);
int tb_tone_x(const tb_layout_t *L, int shade);
void tb_tone_box(const tb_layout_t *L, int *x0, int *y0, int *x1, int *y1);
/* ... `hot` == TI_SIZE: the current brush's footprint is drawn over the knob (clipped to the slider) */
/* The Recent slots (TI_RECENT .. +4): the last combinations drawn with, `rc` (rc_n of them, most recent first): a swatch of the tone with a dot of
   the size and the brush's glyph tiny in a corner; the one equal to the current brush, tone and size is shown picked */
void tb_draw(gfx_t *g, const tb_layout_t *L, const bstyle_t *cur, int hot, const bstyle_t *rc, int rc_n, int smooth);
#endif
