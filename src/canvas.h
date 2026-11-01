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

void cv_fill(canvas_t *c, int colour); /* 1 white, 0 black */
int cv_get(const canvas_t *c, int x, int y);
/* A disc of diameter d (>= 1) centred on (cx, cy); the touched box is added to *dirty (may be NULL) */
void cv_disc(canvas_t *c, int cx, int cy, int d, int colour, box_t *dirty);
/* A stroke segment: discs every pixel along the longer axis, diameter interpolated d0 -> d1, so there are no gaps
   whatever the speed */
void cv_seg(canvas_t *c, int x0, int y0, int d0, int x1, int y1, int d1, int colour, box_t *dirty);
/* Copies the rows and whole bytes covering box b from src to dst (same size and stride) */
void cv_copy_box(canvas_t *dst, const canvas_t *src, const box_t *b);
/* Inverts one pixel (the hover cursor: drawn and undone by copying the page back) */
void cv_flip(canvas_t *c, int x, int y);
#endif
