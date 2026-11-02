/* Text as ink: the baked font drawn into a page bitmap in three sizes, in a tone. A text is a list of code points
   (10 starts a new line); drawing it is a pure function of its points (so undo and replay give the same pixels).
   SPDX-License-Identifier: MIT */
#ifndef TEXTDRAW_H
#define TEXTDRAW_H
#include <stdint.h>
#include "canvas.h"

#define TX_SIZES 3
/* The text size class (0 small, 1 medium, 2 large) for the pen size 1..48 of the size slider */
int tx_class(int pen_size);
int tx_line_h(int cls); /* the height of one line in px (the caret and the I-beam are this tall) */
/* Draws cps[0..n) with the top left of the first line at (x, y) in `tone` (a shade, canvas.h) onto c (NULL: only measure).
   *bb gets the box covered (empty for none); *ex, *ey the place of the caret after the last character (its line's top). */
void tx_draw(canvas_t *c, int colour, int shade, int x, int y, int cls, const uint16_t *cps, int n, box_t *bb, int *ex, int *ey);
#endif
