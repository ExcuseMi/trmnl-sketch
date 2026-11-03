/* An undo step as its changed pixels: the XOR of the page after and before it over the step's box, run-length coded
   per row (skips of unchanged bytes, repeats, literals), so undo and redo are the same cheap XOR whatever the step
   cost to draw (owner 2026-11-03: undo after a big fill took minutes, every press stamped all strokes again).
   SPDX-License-Identifier: MIT */
#ifndef HDELTA_H
#define HDELTA_H
#include <stdint.h>
#include "canvas.h"

/* The bytes of a ^ b over box bb (clipped to the page; whole bytes), written to out; out NULL: only counted */
uint32_t hd_encode(const canvas_t *a, const canvas_t *b, const box_t *bb, uint8_t *out);
/* c ^= the delta (n bytes from hd_encode with the same box) */
void hd_xor(canvas_t *c, const box_t *bb, const uint8_t *d, uint32_t n);
#endif
