/* Stroke-based undo and redo over a page bitmap. Every stroke (ink, eraser, clear) is a log of points; the page is the
   base image plus the first st_cur strokes drawn again, so undo and redo give back the exact pixels. The log is
   bounded: when it is full the oldest stroke is drawn into the base image and can no longer be undone.
   SPDX-License-Identifier: MIT */
#ifndef HIST_H
#define HIST_H
#include <stdint.h>
#include "brush.h"
#include "canvas.h"

enum { HK_INK, HK_ERASE, HK_CLEAR, HK_FILL }; /* HK_FILL: the points are runs of a bucket fill (fill.h) */

typedef struct {
    int16_t x, y;
    uint8_t d; /* the brush's byte for this point (brush.h): a diameter, a side or a pressure */
    uint8_t t; /* the pen's tilt at this point (brush.h bt_code); 0 upright. Fill runs leave it 0 */
} hpt_t;

typedef struct {
    uint32_t first, n; /* points */
    uint8_t kind;
    bstyle_t style; /* the brush, shade and size it was drawn with */
    box_t bb; /* where the stroke changed the page */
} hstroke_t;

typedef struct {
    canvas_t *page;
    canvas_t base; /* the page before st[0] */
    hpt_t *pts;
    uint32_t pt_cap, pt_n;
    hstroke_t *st;
    int st_cap, st_n, st_cur; /* st_cur strokes are on the page; the rest can be redone */
    int open;                 /* st[st_cur - 1] is being drawn */
    uint32_t baking;          /* points of st[0] already in the base: a bake under way (hist_bake_step) */
} hist_t;

/* Baking ahead, a slice at a time (owner's X 2026-11-02: once the log held HIST strokes every new stroke started
   with the oldest one re-stamped into the base, 0.6 s for a big Brush stroke, the pen frozen): when the log is
   within HIST_BAKE_AHEAD strokes of full, up to max_pts points of the oldest stroke go into the base per call; the
   stroke leaves the log once whole. Returns 1 while there was something to do. Undo, redo, clear and a full log
   finish a bake under way first. */
#define HIST_BAKE_AHEAD 8
int hist_bake_step(hist_t *h, uint32_t max_pts);

/* base_px: a page-sized buffer. The base starts as a copy of the page */
void hist_init(hist_t *h, canvas_t *page, uint8_t *base_px, hpt_t *pts, uint32_t pt_cap, hstroke_t *st, int st_cap);
/* The page was replaced (loaded): no undo into the old one */
void hist_reset(hist_t *h);
void hist_begin(hist_t *h, int kind); /* the pen in black (HK_ERASE: a round eraser) */
void hist_begin_style(hist_t *h, int kind, const bstyle_t *style);
/* A point of the open stroke: draws it on the page and adds the touched box to *dirty */
void hist_add(hist_t *h, int x, int y, int d, box_t *dirty);
void hist_add_t(hist_t *h, int x, int y, int d, int tilt, box_t *dirty); /* with the pen's tilt code */
void hist_end(hist_t *h);
/* 1 and the box of changed pixels, or 0 when there is nothing to undo / redo */
int hist_undo(hist_t *h, box_t *changed);
int hist_redo(hist_t *h, box_t *changed);
/* Whitens the page as a stroke of its own (undoable) */
void hist_clear(hist_t *h, box_t *changed);
/* A bucket fill as a stroke of its own: room for n runs (older strokes are baked into the base if the log is short;
   NULL when n alone does not fit), the caller writes them (bf_emit); hist_fill_end paints them with the style's shade */
hpt_t *hist_fill_begin(hist_t *h, uint32_t n, const bstyle_t *style);
void hist_fill_end(hist_t *h, uint32_t n, const box_t *bb);
int hist_can_undo(const hist_t *h);
int hist_can_redo(const hist_t *h);
#endif
