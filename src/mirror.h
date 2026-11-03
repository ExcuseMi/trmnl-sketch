/* Mirror drawing: every stroke is repeated through a symmetry about the page's centre (page = framebuffer frame, so
   the axes follow the page in every position of the X). The images are integer maps (no floats: replay is exact), a
   stroke's log holds only the pen's own points. Also the dashed guide drawn over the screen only. SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 */
#ifndef MIRROR_H
#define MIRROR_H
#include "canvas.h"

enum { MR_OFF, MR_LR, MR_TB, MR_4, MR_K8, MR_N }; /* off, left-right, top-bottom, 4-way, kaleidoscope (4 quarter turns + mirror: one image a 45 degree sector) */

const char *mr_name(int mode);
int mr_images(int mode); /* 1, 2, 2, 4, 8 */
/* Image `img` (0 is the identity) of the page point (x, y) on a w x h page. Exact (flips and quarter turns). The result may lie off the page (a turned page is cropped) */
void mr_map(int mode, int img, int w, int h, int x, int y, int *ox, int *oy);
/* 1 when image img is only flips (x to w - 1 - x when *fx, y to h - 1 - y when *fy), as mr_map makes it */
int mr_flips(int mode, int img, int *fx, int *fy);
/* mr_map of the n pixels x + dx[i] of row y (no 64-bit products a pixel when the page is small): the same points */
void mr_map_pts(int mode, int img, int w, int h, int x, int y, const int16_t *dx, int n, int16_t *ox, int16_t *oy);
/* The guide for `mode` flipped onto fb inside box `clip` widened to whole bytes (cv_copy_box copies those; inverted 2 px dashes: seen over white and black alike). The
   page canvas is never touched */
void mr_guide(canvas_t *fb, int mode, const box_t *clip);
#endif
