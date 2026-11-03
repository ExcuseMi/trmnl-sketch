/* The rectangles waiting to be pushed to the panel: new ink, the cursor and overlays add boxes; they merge when
   that costs little, never more than SK_MAX_RECTS wait, and each leaves aligned to whole bytes (the panel pushes whole
   bytes of a row). SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 */
#ifndef RECTS_H
#define RECTS_H
#include <stdint.h>
#include "canvas.h"
#include "sk_cfg.h"

typedef struct {
    int16_t x, y, w, h;
    int16_t flash; /* 1: refresh this rectangle clean (it flashes), not partially */
} rect_t;

typedef struct {
    box_t b[SK_MAX_RECTS];
    int n;
} rset_t;

void rs_init(rset_t *s);
/* Adds the pixels of b (clipped to w x h, widened to whole bytes) */
void rs_add(rset_t *s, const box_t *b, int w, int h);
/* Takes everything pending as rectangles; returns how many */
int rs_take(rset_t *s, rect_t *out);
#endif
