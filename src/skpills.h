/* The two pills on the drawing page ("Tools", "Menu"): what the pen taps to reach the toolbar and the page menu without the
   tablet's buttons or the touch bar. Layout and hit test are pure (user frame); drawn over the page like the toolbar.
   SPDX-License-Identifier: MIT */
#ifndef SKPILLS_H
#define SKPILLS_H
#include "gfx.h"

enum { PL_NONE = 0, PL_TOOLS, PL_MENU };
typedef struct {
    int x0[2], y0, x1[2], y1; /* the two pills (Tools, Menu), user frame, inclusive */
} pills_t;

/* In the corner away from the toolbar (the toolbar is on top unless the touch bar is) */
void pl_layout(pills_t *p, int orient, int sw, int sh);
int pl_hit(const pills_t *p, int x, int y); /* PL_*, a little generous */
void pl_draw(gfx_t *g, const pills_t *p);
#endif
