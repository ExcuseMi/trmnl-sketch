/* A fixed pen stroke per brush through the whole app (sk_pen), hashed: the pixels today's pressure handling gives.
   The expected values were taken from the build before the pressure curve and tilt existed. SPDX-License-Identifier: MIT */
#ifndef GOLDEN_H
#define GOLDEN_H
#include <math.h>
#include "util.h"

static inline unsigned golden_hash(int brush, int size, int tone)
{
    static rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    r.s.sty.brush = (uint8_t)brush, r.s.sty.size = (uint8_t)size, r.s.sty.shade = (uint8_t)tone;
    r.s.ink_size = (uint8_t)size;
    for (int i = 0; i < 90; i++) {
        double u = i / 89.0;
        int x = 300 + (int)(900 * u), y = 500 + (int)(120 * sin(u * 9.0));
        unsigned p = (unsigned)(2000 + 60000 * (0.5 + 0.5 * sin(u * 5.0 + 1.0)));
        rig_pen(&r, x, y, APP_PEN_TIP | APP_PEN_IN_RANGE, p);
    }
    rig_pen(&r, 1200, 500, APP_PEN_IN_RANGE, 0);
    unsigned h = 2166136261u;
    for (int i = 0; i < TS * TH; i++) h = (h ^ r.s.page.px[i]) * 16777619u;
    rig_end(&r);
    return h;
}
#endif
