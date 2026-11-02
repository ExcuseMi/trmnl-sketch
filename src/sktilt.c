/* The pen's tilt in the page's frame, as the byte a stroke point carries (brush.h bt_code). SPDX-License-Identifier: MIT */
#include "sketch.h"

/* The tablet's lean (degrees, its own x right and y down) turned the way pm_map turns the tablet onto the page: the turned
   tablet of portrait, then the X's position */
int sk_tilt_code(const penmap_t *m, int tx, int ty)
{
    if (!tx && !ty) return 0;
    int vx = tx, vy = ty;
    if (m->turn == 1) vx = -ty, vy = tx;
    else if (m->turn == 2) vx = ty, vy = -tx;
    int ox, oy, ex, ey, fx, fy;
    app_orient_to_fb(m->orient, m->fb_w, m->fb_h, 0, 0, &ox, &oy);
    app_orient_to_fb(m->orient, m->fb_w, m->fb_h, 1, 0, &ex, &ey);
    app_orient_to_fb(m->orient, m->fb_w, m->fb_h, 0, 1, &fx, &fy);
    return bt_code(vx * (ex - ox) + vy * (fx - ox), vx * (ey - oy) + vy * (fy - oy));
}

void sk_pen_tilt(sketch_t *s, int tilt_x, int tilt_y)
{
    s->tilt = sk_tilt_code(&s->map, tilt_x, tilt_y);
}
