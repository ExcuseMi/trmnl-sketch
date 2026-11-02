/* The toolbar's glyphs: each brush as the tool drawing apps show for it (fine-tip pen, paintbrush, pencil, chisel
   marker, fountain-pen nib, spray can, block eraser), undo and redo as curved arrows, clear as a trash can. All
   in a 64 x 64 box at (x, y), bold 1-bit line art: `ink` for the strokes, `bg` for what the box is filled with.
   SPDX-License-Identifier: MIT */
#ifndef TBICONS_H
#define TBICONS_H
#include "gfx.h"

#define TBI_SIZE 64
void tbi_brush(gfx_t *g, int brush, int x, int y, int ink, int bg);
/* The symmetry mode (MR_*) drawn: two halves about the axis, or a star of axes */
void tbi_mirror(gfx_t *g, int mode, int x, int y, int ink, int bg);
void tbi_undo(gfx_t *g, int x, int y, int ink, int bg);
void tbi_redo(gfx_t *g, int x, int y, int ink, int bg);
void tbi_clear(gfx_t *g, int x, int y, int ink, int bg);
/* The shape and text tools (TL_LINE .. TL_TEXT): a diagonal line with its end dots, a box, an oval, a letter T with a caret */
void tbi_tool(gfx_t *g, int tool, int x, int y, int ink, int bg);
#endif
