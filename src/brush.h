/* The brushes: one table, one drawing path. A stroke is a log of points (x, y, p); `p` is one byte whose meaning the
   brush gives (a diameter, a side, a pressure), so a stroke is drawn again from the log with the same pixels (undo,
   redo, replay): everything here is a pure function of the style and the points, nothing reads a clock or a random
   source. Also each brush's hover cursor shape. SPDX-License-Identifier: MIT */
#ifndef BRUSH_H
#define BRUSH_H
#include <stdint.h>
#include "canvas.h"

/* The order is the toolbar's order and the cycle order */
enum { BR_FINELINER, BR_BRUSHPEN, BR_PENCIL, BR_MARKER, BR_CALLIGRAPHY, BR_SPRAY, BR_ERASER, BR_FILL, BR_N };

typedef struct {
    uint8_t brush, shade, size; /* BR_*, SH_*, 0..2 */
} bstyle_t;

const char *brush_name(int brush);
const char *shade_name(int shade);
int brush_next(int brush);       /* the next in the cycle (wraps) */
int brush_is_fill(int brush);    /* the bucket: a tap fills (fill.h), no stroke */
int brush_is_eraser(int brush);  /* draws white, ignores the shade */
int brush_tail(int brush);       /* the end of a stroke tapers out (brush pen) */
/* The byte a point carries for a pressure 0..65535 */
int brush_param(int brush, int size, unsigned pressure);
/* The stroke from (x0, y0, p0) to (x1, y1, p1); one stamp when both are the same. colour 1: white (an erasing stroke) */
void brush_seg(const bstyle_t *st, int colour, canvas_t *c, int x0, int y0, int p0, int x1, int y1, int p1, box_t *dirty);

/* The brush's footprint outline about its hot spot (0, 0) inside a square of half side brush_shape_r */
int brush_shape_r(int brush, int size);
int brush_shape_px(int brush, int size, int dx, int dy);
/* The hover cursor drawn: that outline 3 px thick, with a cross to 16 px for small brushes (so 32 px across at least),
   inside a square of half side brush_cursor_r. brush_cursor_px says whether the pixel at (dx, dy) is black; a white
   outline BR_CUR_OUT px wide goes around it (ui_cursor) so it shows on black ink too */
#define BR_CUR_OUT 2
int brush_cursor_r(int brush, int size);
int brush_cursor_px(int brush, int size, int dx, int dy);
#endif
