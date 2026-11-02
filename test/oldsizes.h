/* The tests below were written for three pen sizes, 0 thin, 1 medium, 2 thick. A pen size is now its diameter in px
   (1..48, the slider); these map the old indices to the same pens (5, 9 and 17 px) at the calls that take a size.
   SPDX-License-Identifier: MIT */
#ifndef OLDSIZES_H
#define OLDSIZES_H
#define SZ(i) ((i) == 0 ? 5 : (i) == 1 ? 9 : 17)
#define brush_param(b, s, p) (brush_param)((b), SZ(s), (p))
#define brush_shape_r(b, s) (brush_shape_r)((b), SZ(s))
#define brush_shape_px(b, s, x, y) (brush_shape_px)((b), SZ(s), (x), (y))
#define brush_cursor_r(b, s) (brush_cursor_r)((b), SZ(s))
#define brush_cursor_px(b, s, x, y) (brush_cursor_px)((b), SZ(s), (x), (y))
#define pen_min_d(s) (pen_min_d)(SZ(s))
#define pen_max_d(s) (pen_max_d)(SZ(s))
#define pen_erase_d(s) (pen_erase_d)(SZ(s))
#define pen_diam(s, p) (pen_diam)(SZ(s), (p))
#endif
