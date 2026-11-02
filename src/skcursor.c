/* The cursors of the shape and text tools, as pure shapes (the drawing with its white outline is ui_cursor): a crosshair with
   the shape's tiny glyph in the lower right corner and a ring of the line width about the hot spot; an I-beam as tall as the
   text size. SPDX-License-Identifier: MIT */
#include "brush.h"
#include "textdraw.h"

#define CH_ARM 20   /* the crosshair's arms: 41 px across */
#define CH_R 30     /* half side of the crosshair's box: holds the glyph corner and rings up to 48 px */
#define IB_SERIF 7  /* the I-beam's serifs reach this far each side */

static int iabs(int v) { return v < 0 ? -v : v; }
static int d2(int x, int y) { return x * x + y * y; }

int tc_r(int tc, int size)
{
    if (tc == BR_N + TL_TEXT) {
        int h = tx_line_h(tx_class(size)) / 2;
        return h < 16 ? 16 : h + 1;
    }
    return CH_R;
}

/* the glyph of the shape in the box (8, 8) .. (24, 24) of the lower right */
static int glyph(int tool, int x, int y)
{
    int gx = x - 16, gy = y - 16; /* centred on (16, 16) */
    if (iabs(gx) > 9 || iabs(gy) > 9) return 0;
    if (tool == TL_LINE) return iabs(gx + gy) <= 1 || d2(gx + 7, gy - 7) <= 6 || d2(gx - 7, gy + 7) <= 6; /* "/" with its two end dots */
    if (tool == TL_RECT) return iabs(gx) <= 8 && iabs(gy) <= 6 && (iabs(gx) >= 7 || iabs(gy) >= 5); /* a frame */
    return d2(gx, gy) <= 64 && d2(gx, gy) >= 36; /* a ring */
}

int tc_px(int tc, int size, int dx, int dy)
{
    int ax = iabs(dx), ay = iabs(dy);
    if (tc == BR_N + TL_TEXT) {
        int h = tx_line_h(tx_class(size)) / 2;
        if (h < 15) h = 15;
        return (ax <= 1 && ay <= h) || (ay >= h - 2 && ay <= h && ax <= IB_SERIF);
    }
    if ((ax <= 1 && ay <= CH_ARM) || (ay <= 1 && ax <= CH_ARM)) return 1; /* the arms */
    int rr = size / 2 + 1, e = d2(dx, dy); /* the ring of the line width: 2 px thick, touching the stroke's edge */
    if (rr >= 4 && e <= (rr + 1) * (rr + 1) && e >= (rr - 1) * (rr - 1)) return 1;
    return dx > 0 && dy > 0 && glyph(tc - BR_N, dx, dy);
}
