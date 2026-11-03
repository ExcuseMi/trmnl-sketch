/* The one flash path (an inverted flash, then the clean refresh) and the clean-up rules that ask for it. SPDX-License-Identifier: MIT */
#include <string.h>
#include "sketch.h"

static int fl_whole(const sketch_t *s, const box_t *b) { return b->x0 <= 0 && b->y0 <= 0 && b->x1 >= s->fb.w - 1 && b->y1 >= s->fb.h - 1; }

static void fl_invert(sketch_t *s, const box_t *b)
{
    for (int y = b->y0; y <= b->y1; y++)
        for (int x = b->x0 / 8; x <= b->x1 / 8; x++) s->fb.px[y * s->fb.stride + x] ^= 0xff;
}

void sk_flash(sketch_t *s, const box_t *area)
{
    box_t b = *area;
    if (box_empty(&b)) return;
    b.x0 &= ~7, b.x1 |= 7;
    box_clip(&b, s->fb.w, s->fb.h);
    box_union(&s->flash, &b);
}

int sk_flash_busy(const sketch_t *s) { return s->fl_phase != 0; }

static void fl_rect(const box_t *b, int flash, rect_t *r) { *r = (rect_t){ (int16_t)b->x0, (int16_t)b->y0, (int16_t)(b->x1 - b->x0 + 1), (int16_t)(b->y1 - b->y0 + 1), (int16_t)flash }; }

/* May the screen be cleaned now (the pen is not drawing)? */
static int fl_calm(const sketch_t *s) { return !s->tip && !s->stroke && !s->fl_phase && !s->want_full && box_empty(&s->flash) && !s->print_req; }

/* The drawing page never flashes by itself (owner 2026-11-02: a flash costs 1.1 s and blocks the app): the toolbar, the
   legend, the arrow and the cursors are ink only. Only a menu or gallery screen with traces gets the idle flash. */
static int fl_dirty(const sketch_t *s) { return s->ghost && s->mode != SK_DRAW; }
static int fl_idle_due(const sketch_t *s, uint32_t now) { return s->cleanup_s > 0 && fl_dirty(s) && !s->pend.n && fl_calm(s) && (int32_t)(now - s->last_act - (uint32_t)s->cleanup_s * 1000u) >= 0; }

uint32_t sk_cleanup_due(const sketch_t *s, uint32_t now)
{
    if (s->cleanup_s > 0 && fl_dirty(s) && !s->pend.n && fl_calm(s)) {
        int32_t d = (int32_t)(s->last_act + (uint32_t)s->cleanup_s * 1000u - now);
        return d < 0 ? 0 : (uint32_t)d;
    }
    return 1000000;
}

int sk_flash_step(sketch_t *s, uint32_t now, rect_t *out)
{
    if (s->fl_phase) { /* the clean half: what the framebuffer held before the inverted half */
        fl_invert(s, &s->fl_box);
        s->fl_phase = 0;
        s->last_push = now;
        if (fl_whole(s, &s->fl_box)) {
            s->ghost = s->traces = 0;
            s->pushed_px += (uint32_t)(s->fb.w * s->fb.h);
            s->pushed_rects++;
            return SK_FULL;
        }
        fl_rect(&s->fl_box, 1, &out[0]);
        s->pushed_px += (uint32_t)(out[0].w * out[0].h);
        s->pushed_rects++;
        return 1;
    }
    if (fl_idle_due(s, now)) s->want_full = 1;
    if (s->want_full) {
        s->want_full = 0;
        if (s->mode >= SK_GALLERY && s->ui.px) { /* the screen as it is, then the pointer on it */
            memcpy(s->fb.px, s->ui.px, (size_t)s->fb.stride * (size_t)s->fb.h);
            s->cursor = 0;
            sk_menu_cursor(s);
        }
        box_t all = { 0, 0, s->fb.w - 1, s->fb.h - 1 };
        box_union(&s->flash, &all);
    }
    if (box_empty(&s->flash)) return SK_NOFLASH;
    s->fl_box = s->flash;
    box_clear(&s->flash);
    if (fl_whole(s, &s->fl_box)) rs_init(&s->pend);
    fl_invert(s, &s->fl_box);
    s->fl_phase = 1;
    fl_rect(&s->fl_box, 0, &out[0]);
    s->pushed_px += (uint32_t)(out[0].w * out[0].h);
    s->pushed_rects++;
    s->last_push = now;
    return 1;
}
