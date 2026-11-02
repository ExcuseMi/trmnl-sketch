/* The mirror setting: the toolbar item and the guide on the screen. SPDX-License-Identifier: MIT */
#include "sketch.h"

void sk_set_mirror(sketch_t *s, int mode, uint32_t now)
{
    if (mode < 0 || mode >= MR_N) return;
    hist_end(&s->hist);
    s->stroke = 0;
    s->sty.mirror = (uint8_t)mode;
    char t[24] = "Mirror: ";
    sk_cpy(t + 8, sizeof(t) - 8, mr_name(mode));
    sk_cursor_hide(s);
    if (s->mode == SK_DRAW) {
        sk_show_page(s); /* the guide on or off in the framebuffer, the page itself untouched */
        box_t all = { 0, 0, s->fb.w - 1, s->fb.h - 1 };
        rs_add(&s->pend, &all, s->fb.w, s->fb.h); /* fast ink: the drawing page never flashes */
    }
    sk_picked(s, t, now);
}

/* ---- the mirror picker (mrpick.h): opened by a tap on the toolbar's Mirror item, closed by a pick, a tap elsewhere, the
   pen going out of range or any tablet button. Fast ink only: opening and closing never ask for a flash. ---- */

static void mp_place(sketch_t *s)
{
    gfx_t g;
    box_t b;
    gfx_init(&g, s->fb.px, s->fb.w, s->fb.h, s->fb.stride, s->orient);
    mp_layout(&s->mp, &s->tb, g.SW);
    ui_fb_box(&g, s->mp.x0, s->mp.y0, s->mp.x1, s->mp.y1, &b);
    box_clip(&b, s->fb.w, s->fb.h);
    b.x0 &= ~7;
    b.x1 |= 7;
    box_clip(&b, s->fb.w, s->fb.h);
    s->mp_box = b;
}

void sk_mp_paint(sketch_t *s)
{
    if (!s->mp_on) return;
    gfx_t g;
    gfx_init(&g, s->fb.px, s->fb.w, s->fb.h, s->fb.stride, s->orient);
    mp_draw(&g, &s->mp, s->sty.mirror);
}

void sk_mp_open(sketch_t *s)
{
    if (s->mode != SK_DRAW || !s->tb_on || s->mp_on) return;
    sk_cursor_hide(s);
    sk_note_hide(s); /* a toast sits where the picker goes */
    hist_end(&s->hist);
    s->stroke = 0;
    mp_place(s);
    s->mp_on = 1;
    s->mp_hold = 1; /* the press that opened it is still down */
    sk_mp_paint(s);
    sk_mark(s, &s->mp_box);
}

void sk_mp_close(sketch_t *s)
{
    if (!s->mp_on) return;
    s->mp_on = 0;
    sk_cursor_hide(s); /* (an arrow over the picker goes with it) */
    sk_fb_restore(s, &s->mp_box); /* the page, paper, guide and strip under it, pushed */
    s->traces++; /* its ghost waits for the idle clean-up like a toast's */
}

/* The pen while the picker is open. A press on a mode picks it (the same mode: nothing changes); a press anywhere else
   closes it and does nothing more until the pen lifts; out of range closes it; hovering over it shows the arrow */
int sk_mp_pen(sketch_t *s, int tip, int range, int inside, int ux, int uy, uint32_t now)
{
    if (!tip) s->mp_hold = 0;
    if (!s->mp_on) return tip; /* closed by the press that is still down: no ink until the pen lifts */
    if (!tip && !range) {
        sk_mp_close(s);
        return 0;
    }
    if (tip) {
        if (s->mp_hold) return 1;
        s->mp_hold = 1;
        int m = inside ? mp_hit(&s->mp, ux, uy) : -1;
        sk_mp_close(s);
        if (m >= 0 && m != s->sty.mirror) sk_set_mirror(s, m, now);
        return 1;
    }
    if (!(inside && mp_in(&s->mp, ux, uy))) return 0;
    int fx, fy;
    app_orient_to_fb(s->orient, s->fb.w, s->fb.h, ux, uy, &fx, &fy);
    s->hover_valid = 0;
    if (s->tb_ptr && (fx - s->cx) * (fx - s->cx) + (fy - s->cy) * (fy - s->cy) < 9) return 1;
    if (s->tb_hot >= 0) { /* the strip's item frame goes */
        s->tb_hot = -1;
        sk_tb_paint(s);
        sk_mark(s, &s->tb_box);
    }
    sk_cursor_hide(s);
    box_t b;
    ui_arrow(&s->fb, s->orient, ux, uy, &b);
    box_clip(&b, s->fb.w, s->fb.h);
    s->tb_ptr = 1, s->tb_ptr_box = b, s->cx = fx, s->cy = fy;
    sk_mark(s, &b);
    return 1;
}
