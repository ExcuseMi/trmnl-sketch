/* The Fill tool in the controller: a tap (tip down and up without moving much) fills. SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include "fill.h"
#include "sketch.h"

#define BK_TAP_PX 24     /* the tip may move this far (each axis) and still be a tap */
#define BK_ASK_MS 6000   /* "Fill the whole page?" waits this long for a second tap */

static int far(int a, int b) { return a - b > BK_TAP_PX || b - a > BK_TAP_PX; }

int sk_fill_pen(sketch_t *s, int tip, int inside, int fx, int fy, uint32_t now)
{
    if (s->bk_down && !tip) { /* the tip is up: a tap that stayed put fills where it came down */
        s->bk_down = 0;
        if (!s->bk_moved) sk_fill_at(s, s->bk_x, s->bk_y, now);
        return 0;
    }
    if (!tip) return 0;
    if (s->bk_down) {
        if (far(fx, s->bk_x) || far(fy, s->bk_y)) s->bk_moved = 1;
        return 1;
    }
    if (inside && !s->stroke && brush_is_fill(s->barrel ? (s->sty.brush == BR_ERASER ? s->last_ink : BR_ERASER) : s->sty.brush)) {
        sk_cursor_hide(s);
        sk_note_hide(s);
        s->bk_down = 1, s->bk_moved = 0, s->bk_x = fx, s->bk_y = fy;
        return 1;
    }
    return 0;
}

void sk_fill_at(sketch_t *s, int x, int y, uint32_t now)
{
    bf_t r = { 0 };
    hist_end(&s->hist);
    int rc = BF_NOTHING;
    for (int img = 0, n = mr_images(s->sty.mirror); img < n; img++) { /* the tap and its mirror images, one region in all */
        int ix, iy;
        mr_map(s->sty.mirror, img, s->page.w, s->page.h, x, y, &ix, &iy);
        if (ix < 0 || iy < 0 || ix >= s->page.w || iy >= s->page.h) continue;
        if (r.mask && (r.mask[iy * r.stride + (ix >> 3)] >> (7 - (ix & 7)) & 1)) continue; /* already inside */
        bf_t one;
        int rc1 = bf_plan(&one, &s->page, ix, iy, s->sty.shade, s->bk_gap, 0);
        if (rc1 == BF_OK) bf_merge(&r, &one), rc = BF_OK;
        else if (rc1 != BF_NOTHING) {
            bf_free(&r);
            rc = rc1;
            break;
        }
    }
    if (rc == BF_NOTHING) return sk_toast(s, "Already like that", now);
    if (rc == BF_MEM) return sk_toast(s, "Not enough memory to fill", now);
    if (rc == BF_COMPLEX) return sk_toast(s, "Too intricate to fill", now);
    if ((uint64_t)r.px * 10 > (uint64_t)s->page.w * (uint64_t)s->page.h * 7 && (int32_t)(now - s->bk_ask_until) >= 0) {
        bf_free(&r); /* nearly the whole page: ask first, a second tap within a few seconds says yes */
        s->bk_ask_until = now + BK_ASK_MS;
        sk_toast(s, "Fill the whole page? Tap again", now);
        s->note_until = now + BK_ASK_MS;
        return;
    }
    s->bk_ask_until = now;
    uint32_t n = bf_runs(&r);
    bstyle_t st = { BR_FILL, s->sty.shade, 1, s->sty.mirror };
    hpt_t *slots = hist_fill_begin(&s->hist, n, &st);
    if (!slots) {
        bf_free(&r);
        return sk_toast(s, "Too intricate to fill", now);
    }
    bf_emit(&r, slots);
    box_t bb = r.bb;
    hist_fill_end(&s->hist, n, &bb);
    bf_free(&r);
    sk_page_changed(s, &bb, now); /* one partial push of the filled box */
}
