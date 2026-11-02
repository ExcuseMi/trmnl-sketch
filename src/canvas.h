/* A 1-bit page bitmap (MSB leftmost, 1 = white, same layout as the panel's framebuffer) with the pen's drawing:
   discs and interpolated segments, plus the dirty boxes that follow. SPDX-License-Identifier: MIT */
#ifndef CANVAS_H
#define CANVAS_H
#include <stdint.h>

typedef struct {
    uint8_t *px;
    int w, h, stride;
} canvas_t;

typedef struct {
    int x0, y0, x1, y1; /* inclusive; x0 > x1: empty */
} box_t;

void box_clear(box_t *b);
int box_empty(const box_t *b);
void box_add(box_t *b, int x, int y);
void box_union(box_t *b, const box_t *o);
void box_clip(box_t *b, int w, int h);

/* Pen shades: black, and two grays as 4 x 4 ordered-dither patterns fixed to the page (the same pixel is black or not
   whatever stroke draws it, so erasing and redrawing, undo and redo give the same pattern). Only ink (colour 0) is
   shaded; white (the eraser) is always solid. */
/* The tone: 9 steps, 0 black then lighter to 8, the 4 x 4 Bayer levels 16 (black), 15, 14, 12, 10, 8, 6, 4 and 2 of 16 dots.
   SH_DARK and SH_LIGHT are what the earlier Dark gray (11 of 16) and Light gray (5 of 16) became (settings migrate: skcfg.c) */
enum { SH_BLACK = 0, SH_DARK = 3, SH_LIGHT = 6, SH_N = 9 };
int cv_shade_level(int shade); /* the dots of 16 that are black */
int cv_shade_black(int shade, int x, int y); /* 1: the pattern is black at this page pixel */
void cv_span_s(canvas_t *c, int y, int x0, int x1, int colour, int shade); /* clipped */
void cv_span_pat(canvas_t *c, int y, int x0, int x1, int shade); /* the shade's pattern exactly, white dots too */
void cv_px_s(canvas_t *c, int x, int y, int colour, int shade);            /* clipped */
void cv_disc_s(canvas_t *c, int cx, int cy, int d, int colour, int shade, box_t *dirty);

void cv_fill(canvas_t *c, int colour); /* 1 white, 0 black */
int cv_get(const canvas_t *c, int x, int y);
/* A disc of diameter d (>= 1) centred on (cx, cy); the touched box is added to *dirty (may be NULL) */
void cv_disc(canvas_t *c, int cx, int cy, int d, int colour, box_t *dirty);
/* A stroke segment: discs every pixel along the longer axis, diameter interpolated d0 -> d1, so there are no gaps
   whatever the speed */
void cv_seg(canvas_t *c, int x0, int y0, int d0, int x1, int y1, int d1, int colour, box_t *dirty);
/* Copies the rows and whole bytes covering box b from src to dst (same size and stride) */
void cv_copy_box(canvas_t *dst, const canvas_t *src, const box_t *b);
/* Sets one pixel (colour 0 black, 1 white), clipped (the cursors: undone by copying the page back) */
void cv_set(canvas_t *c, int x, int y, int colour);
/* Inverts one pixel (the hover cursor: drawn and undone by copying the page back) */
void cv_flip(canvas_t *c, int x, int y);
#endif
