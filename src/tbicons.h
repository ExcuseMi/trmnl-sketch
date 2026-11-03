/* The toolbar's glyphs: each brush as the tool drawing apps show for it (pen nib for the Pen, paintbrush for the Brush, pencil, chisel
   marker, spray can, block eraser, paint bucket), the mirror, the colour swatches and the size dots. All
   in a 64 x 64 box at (x, y), bold 1-bit line art: `ink` for the strokes, `bg` for what the box is filled with.
   SPDX-License-Identifier: MIT */
#ifndef TBICONS_H
#define TBICONS_H
#include "brush.h"
#include "gfx.h"

#define TBI_SIZE 64
void tbi_brush(gfx_t *g, int brush, int x, int y, int ink, int bg);
/* The symmetry mode (MR_*) drawn: two halves about the axis, or a star of axes */
void tbi_mirror(gfx_t *g, int mode, int x, int y, int ink, int bg);
/* A colour swatch of the shade (black, grey, white) and a size button's dot (class 0 small .. 2 big) */
void tbi_colour(gfx_t *g, int shade, int x, int y, int ink, int bg);
void tbi_size(gfx_t *g, int cls, int x, int y, int ink, int bg);
/* The Menu button: a list (three bars with their bullets) */
void tbi_menu(gfx_t *g, int x, int y, int ink, int bg);
#endif
