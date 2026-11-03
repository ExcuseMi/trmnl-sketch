/* The brushes: one table, one drawing path. A stroke is a log of points (x, y, p); `p` is one byte whose meaning the
   brush gives (a diameter, a side, a pressure), so a stroke is drawn again from the log with the same pixels (undo,
   redo, replay): everything here is a pure function of the style and the points, nothing reads a clock or a random
   source. Also each brush's hover cursor shape. SPDX-License-Identifier: MIT */
#ifndef BRUSH_H
#define BRUSH_H
#include <stdint.h>
#include "canvas.h"

/* The order is the toolbar's order and the cycle order */
enum { BR_BRUSH, BR_PEN, BR_PENCIL, BR_MARKER, BR_SPRAY, BR_ERASER, BR_FILL, BR_N };
/* The three size buttons (Small, Medium, Big): brush_size gives the pen size (1..48) of a button for a brush; brush_size_cls the button a size belongs to, or -1 */
#define BR_SIZES 3
int brush_size(int brush, int cls);
int brush_size_cls(int brush, int size);
/* A brush id of the earlier ten-brush table (fineliner, brush pen, pencil, marker, calligraphy, spray, eraser, fill) as it is now */

typedef struct {
    uint8_t brush, shade, size; /* BR_*, SH_*, the pen size 1..48 */
    uint8_t mirror;             /* MR_* (mirror.h): the stroke is repeated through that symmetry; 0 off */
    uint8_t scale;              /* the Brush's stamp step times this (0 and 1: as drawn); set per stroke (skframe.h) */
} bstyle_t;

const char *brush_name(int brush);
const char *shade_name(int shade);
int brush_next(int brush);       /* the next in the cycle (wraps) */
int brush_is_fill(int brush);    /* the bucket: a tap fills (fill.h), no stroke */
int brush_is_eraser(int brush);  /* draws white, ignores the shade */
int brush_tail(int brush);       /* the end of a stroke tapers out (brush pen) */
/* The pen's tilt on a point: one byte, the tilt of x and of y in 6 degree steps (-48..42 degrees) as two signed nibbles; 0 is upright.
   Only the pencil (a wider, lighter mark along the tilt, like shading) and, lightly, the brush pen (a little wider) read it */
#define BT_STEP 6
int bt_code(int tilt_x, int tilt_y);                  /* degrees to the byte */
void bt_decode(int code, int *tilt_x, int *tilt_y);   /* the byte to degrees */
/* The byte a point carries for a pressure 0..65535 */
int brush_param(int brush, int size, unsigned pressure);
/* The same with the pen's tilt code on each end (interpolated like p); brush_seg is upright */
void brush_segt(const bstyle_t *st, int colour, canvas_t *c, int x0, int y0, int p0, int t0, int x1, int y1, int p1, int t1, box_t *dirty);
/* The stroke from (x0, y0, p0) to (x1, y1, p1); one stamp when both are the same. colour 1: white (an erasing stroke) */
int brush_stamp_step(int b, int size); /* pixels between the stamps of a segment */
void brush_seg(const bstyle_t *st, int colour, canvas_t *c, int x0, int y0, int p0, int x1, int y1, int p1, box_t *dirty);
/* Mirrored strokes of the Brush, Pencil and Spray onto c between brush_batch(c) and brush_batch(0) are written once a
   pixel per image (the same page as one by one, faster): the dirty boxes come at once, the pixels at brush_flush (and
   brush_batch). Other canvases (a replay into the base) draw as always. Whoever reads c in between calls brush_flush */
void brush_batch(canvas_t *c);
void brush_flush(void);

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
