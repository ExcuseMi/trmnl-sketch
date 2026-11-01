/* The gallery of saved pages: the grid in the user's frame (four positions of the X), the paging, the thumbnails
   (box filtered to 16 grays, cached per page and dropped when the page is saved) and the gray screen they are drawn
   into. Pure: no file access except through the page loader the caller hands in. SPDX-License-Identifier: MIT */
#ifndef GALLERY_H
#define GALLERY_H
#include <stdint.h>
#include <app_api.h>
#include "canvas.h"
#include "pages.h"

#define GAL_TW 312 /* a thumbnail, in the panel's frame (the page is 1872 x 1404: 6 px per thumbnail pixel) */
#define GAL_TH 234
#define GAL_CACHE 24
#define GAL_ROW (GAL_TW / 8)
#define GAL_BYTES (GAL_ROW * GAL_TH)
#define GAL_LABEL 56 /* the caption under a tile */

typedef struct {
    int cols, rows, per;     /* tiles per row, rows, tiles per screen */
    int tw, th;              /* a tile in the user's frame (turned in the portrait positions) */
    int ox, oy, gx, rowh;    /* top left of the grid, gap between tiles, row pitch */
    int ax0, ay0, ax1, ay1;  /* the free area, the touch bar's strip left out */
} gal_layout_t;

/* The grid for a screen sw x sh (user's frame) with the bar at `orient`. Every tile lies inside the area. */
void gal_layout(gal_layout_t *L, int orient, int sw, int sh);
/* The user-frame top left of tile `slot` (0 .. per - 1) */
void gal_tile_xy(const gal_layout_t *L, int slot, int *x, int *y);
/* The slot under a user-frame point, or -1 */
int gal_hit(const gal_layout_t *L, int x, int y);
/* A framebuffer point of the panel (fw x fh) in the user's frame of `orient` */
void gal_from_fb(int orient, int fw, int fh, int fx, int fy, int *x, int *y);

typedef struct {
    int num;            /* page number, 0: unused */
    uint32_t used;
    uint8_t *px;        /* GAL_TW x GAL_TH, 1 bit, MSB leftmost, 1 white (error-diffused from the area average) */
} gal_thumb_t;

typedef struct {
    gal_thumb_t t[GAL_CACHE];
    uint32_t tick;
    int made;           /* thumbnails generated so far (a test reads it) */
} gal_cache_t;

void gal_cache_free(gal_cache_t *c);
/* Drops page num's thumbnail: the page changed */
void gal_invalidate(gal_cache_t *c, int num);
/* The cached thumbnail of page num or NULL */
const uint8_t *gal_find(gal_cache_t *c, int num);
/* Makes page num's thumbnail from src: the area average of each cell, Floyd-Steinberg dithered to 1 bit (the gallery
   runs in the panel's 1-bit mode so the pen's cursor can be shown). The least recently used one goes when the cache is
   full. NULL when memory is short. */
const uint8_t *gal_make(gal_cache_t *c, int num, const canvas_t *src);
/* One thumbnail pixel: 1 white */
static inline int gal_bit(const uint8_t *t, int x, int y) { return (t[y * GAL_ROW + (x >> 3)] >> (7 - (x & 7))) & 1; }

/* A thumbnail into the 1-bit screen `dst` at user-frame (x, y), turned for `orient`; the rest is untouched */
void gal_blit(canvas_t *dst, int orient, const uint8_t *thumb, int x, int y);
#endif
