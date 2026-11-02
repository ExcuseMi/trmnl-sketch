/* When the Tools and Menu pills show (owner 2026-11-02: "tools & menu always visible, that should hide"): while the pen
   hovers near them (PL_REVEAL px round the pair); hidden while it draws or hovers elsewhere; a pen out of range changes
   nothing (no refresh each time it is lifted away). Shown at the start. A press that began on a pill keeps them. The page comes back under them pixel for pixel.
   SPDX-License-Identifier: MIT */
#include "sketch.h"

#define PL_REVEAL 160 /* px round the pills (user frame) where a hovering pen shows them */

void sk_pills_show(sketch_t *s, int on)
{
    on = on != 0;
    if (on == s->pl_on) return;
    s->pl_on = on;
    if (on) sk_pl_paint(s), sk_mark(s, &s->pl_box);
    else sk_fb_restore(s, &s->pl_box); /* the page, paper, guide and strip under them, pushed */
}

void sk_pills_follow(sketch_t *s, int tip, int range, int inside, int ux, int uy)
{
    if (s->pl_hold) return;
    const pills_t *p = &s->pl;
    int near = inside && ux >= p->x0[0] - PL_REVEAL && ux <= p->x1[1] + PL_REVEAL && uy >= p->y0 - PL_REVEAL && uy <= p->y1 + PL_REVEAL;
    if (tip) sk_pills_show(s, s->pl_on && !s->stroke && !s->tb_hold && pl_hit(p, ux, uy) != PL_NONE); /* a press on a shown pill */
    else if (range) sk_pills_show(s, near);
}
