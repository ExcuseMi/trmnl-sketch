/* The mirror picker: a tap on the toolbar's Mirror item opens a small row of the five modes (Off, Left-right, Top-bottom,
   4-way, Kaleidoscope), each a glyph with a short label, right under the strip (above it when the strip is at the bottom).
   One tap picks and closes it. Layout and hit test are pure (user frame); the drawing goes through gfx.h.
   SPDX-License-Identifier: MIT */
#ifndef MRPICK_H
#define MRPICK_H
#include "gfx.h"
#include "toolbar.h"

typedef struct {
    int x0, y0, x1, y1; /* the row with its frame, user frame, inclusive */
    int cell, ch;       /* an item is cell wide and ch high */
    int cx[5], cy;      /* item left edges and the top edge */
} mp_layout_t;

/* Under (or above) the strip `tb`, its right end at the Mirror item's right end, kept on the screen */
void mp_layout(mp_layout_t *L, const tb_layout_t *tb, int sw);
int mp_in(const mp_layout_t *L, int x, int y);  /* the point is on the row */
int mp_hit(const mp_layout_t *L, int x, int y); /* the mode (MR_*) under the point, or -1 */
const char *mp_label(int mode);                 /* the short name under a glyph */
void mp_draw(gfx_t *g, const mp_layout_t *L, int cur); /* `cur`: the mode on, shown inverted */
#endif
