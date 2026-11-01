/* The pen: pressure to line width, and the tablet's area onto the page. SPDX-License-Identifier: MIT */
#ifndef PENMAP_H
#define PENMAP_H
#include <stdint.h>

/* A value that grows with the pen size (1..48 px) through three anchors, at the sizes 5, 9 and 17 (the old thin, medium
   and thick), straight on beyond them, kept within lo..hi */
int pen_lerp(int size, int a, int b, int c, int lo, int hi);
/* Pen size = the diameter in px at full pressure; pen_min_d at no pressure */
int pen_min_d(int size);
int pen_max_d(int size);
/* Diameter for pressure 0..65535 (a concave curve: a light hand already reaches most of the width; tablets rarely
   report their full range) */
int pen_diam(int size, unsigned pressure);
/* The eraser's diameter for a pen size (fixed: it ignores pressure) */
int pen_erase_d(int size);

/* The tablet's area onto the page. The tablet keeps the user's frame (its top is the screen's top in every position of
   the X). fill (the default, setting pen_area): every tablet edge is a screen edge, the aspect is not kept. keep: no
   stretching, the page is covered: the tablet's longer side loses its edges, so every page pixel can be reached; a pen
   outside the page (on the cut-off edge) is `outside`.
   turn (setting tablet_turn, auto): in portrait (bar left or right) the tablet is used turned a quarter, as the person turns
   it, so its long side is the page's long side and nothing is stretched twice over: bar left (the X turned clockwise)
   is a tablet turned clockwise, bar right a tablet turned counter-clockwise; up on the tablet is up on the page. */
typedef struct {
    int orient, fb_w, fb_h, sw, sh; /* sw, sh: the screen in the user's frame */
    float wt, ht;                   /* the tablet's area scaled to the screen, px (fill: the span from the first to the last pixel) */
    int turn;                       /* 0 landscape, 1 the tablet turned clockwise, 2 counter-clockwise */
    float ox, oy;                   /* where the screen starts inside it (0 for fill) */
} penmap_t;

/* width_um, height_um: pen_info (0: unknown, a 16:10 tablet) */
void pm_init(penmap_t *m, int orient, int fb_w, int fb_h, uint32_t width_um, uint32_t height_um, int fill, int turn);
/* x, y: 0..65535 over the tablet. Gives the page (framebuffer) pixel, clamped to the page; returns 1 when the pen is
   on the page, 0 when it is on the cut-off part of the tablet */
int pm_map(const penmap_t *m, unsigned x, unsigned y, int *fx, int *fy);
#endif
