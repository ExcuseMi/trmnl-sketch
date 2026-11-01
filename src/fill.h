/* The bucket fill: a scanline flood (no recursion, a bounded seed stack) of the connected region that looks like the
   tapped spot (white, or one of the three shades' own dither: "same appearance"), with small gaps in outlines closed
   (lines are dilated by the gap radius for the flood; the band next to the lines is added back so no halo stays).
   The region is kept as a bit mask, then as runs in the undo log (hist.h): one hpt_t per run, x = first, y = last
   pixel, d = rows down from the previous run (a filler x = -1 for a jump over 255 rows). SPDX-License-Identifier: MIT */
#ifndef FILL_H
#define FILL_H
#include <stddef.h>
#include <stdint.h>
#include "hist.h"

enum { BF_OK, BF_NOTHING, BF_MEM, BF_COMPLEX };
#define BF_GAP_MAX 6
#define BF_STACK 8192 /* seeds (4 bytes each); a fuller stack is repaired by rescans, never overflows */

typedef struct {
    uint8_t *mask; /* 1 = filled; page sized; freed by bf_free */
    int w, h, stride;
    box_t bb;
    uint32_t px;
} bf_t;

/* The region of a tap at (x, y) to be painted with `shade`. BF_NOTHING: the spot already looks like that shade.
   stack_cap 0: BF_STACK (tests pass small ones) */
int bf_plan(bf_t *r, const canvas_t *page, int x, int y, int shade, int gap, size_t stack_cap);
void bf_free(bf_t *r);
uint32_t bf_runs(const bf_t *r);        /* slots bf_emit writes */
void bf_emit(const bf_t *r, hpt_t *out); /* the runs of the region */
/* Paints the runs on a page with `shade` (history replay) */
void bf_apply(canvas_t *c, const hpt_t *runs, uint32_t n, int shade);
#endif
