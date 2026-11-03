/* SPDX-License-Identifier: MIT */
#include "hist.h"
#include <string.h>
#include "fill.h"
#include "hdelta.h"
#include "sk_cfg.h"

void hist_init(hist_t *h, canvas_t *page, uint8_t *base_px, hpt_t *pts, uint32_t pt_cap, hstroke_t *st, int st_cap)
{
    h->page = page;
    h->base = *page;
    h->base.px = base_px;
    h->pts = pts;
    h->pt_cap = pt_cap;
    h->st = st;
    h->st_cap = st_cap;
    h->shadow.px = 0;
    h->pool = 0;
    h->pool_cap = 0;
    h->lazy = 0;
    hist_reset(h);
    h->baking = 0;
}

void hist_reset(hist_t *h)
{
    h->baking = 0;
    memcpy(h->base.px, h->page->px, (size_t)h->page->stride * (size_t)h->page->h);
    if (h->shadow.px) memcpy(h->shadow.px, h->page->px, (size_t)h->page->stride * (size_t)h->page->h);
    h->stale = 0;
    h->pt_n = 0;
    h->st_n = h->st_cur = 0;
    h->open = 0;
}

/* The stroke's points from..to (segments from the point before `from`) onto c; a fill or clear is whole or nothing */
static void apply_range(canvas_t *c, const hstroke_t *s, const hpt_t *p, uint32_t from, uint32_t to)
{
    if (s->kind == HK_CLEAR) return cv_fill(c, 1);
    if (s->kind == HK_FILL) return bf_apply(c, p, s->n, s->style.shade);
    int colour = s->kind == HK_ERASE;
    for (uint32_t i = from; i < to; i++) {
        const hpt_t *a = i ? &p[i - 1] : &p[i];
        brush_segt(&s->style, colour, c, a->x, a->y, a->d, a->t, p[i].x, p[i].y, p[i].d, p[i].t, 0);
    }
}

static void apply(canvas_t *c, const hstroke_t *s, const hpt_t *p) { apply_range(c, s, p, 0, s->n); }

void hist_set_deltas(hist_t *h, uint8_t *shadow_px, uint8_t *pool, uint32_t cap)
{
    h->shadow = *h->page;
    h->shadow.px = shadow_px;
    h->pool = pool;
    h->pool_cap = shadow_px && pool ? cap : 0;
    if (shadow_px) memcpy(shadow_px, h->page->px, (size_t)h->page->stride * (size_t)h->page->h);
    for (int i = 0; i < h->st_n; i++) h->st[i].dlen = 0;
}

/* stroke i drawn onto c: its delta XORed when it has one (c is the page as it was before i), else stamped again */
static void redraw(hist_t *h, canvas_t *c, int i)
{
    const hstroke_t *s = &h->st[i];
    if (s->dlen) hd_xor(c, &s->bb, h->pool + s->doff, s->dlen);
    else apply(c, s, h->pts + s->first);
}

/* The oldest and the newest stroke with a delta, below `below` (the deltas lie in the ring in stroke order) */
static int live(const hist_t *h, int below, int *oldest, int *newest)
{
    *oldest = *newest = -1;
    for (int i = 0; i < below; i++)
        if (h->st[i].dlen) {
            if (*oldest < 0) *oldest = i;
            *newest = i;
        }
    return *oldest >= 0;
}

static void bake(hist_t *h, int k);
static void bake_finish(hist_t *h);
static int pinned(const hstroke_t *s) { return s->kind == HK_FILL && s->n == 0; } /* a fill kept only as its delta */

/* Room for n bytes after the deltas of strokes below `below`: the offset, or -1 (the oldest deltas are given up first;
   a fill that is only its delta cannot give it up: it and the strokes before it go into the base instead) */
static int64_t pool_alloc(hist_t *h, int below, uint32_t n)
{
    if (n == 0 || n > h->pool_cap) return -1;
    int o, w;
    while (live(h, below, &o, &w)) {
        uint32_t head = h->st[o].doff, tail = h->st[w].doff + h->st[w].dlen;
        if (h->st[w].doff >= head) { /* not wrapped: free after the tail and before the head */
            if (h->pool_cap - tail >= n) return tail;
            if (head >= n) return 0;
        } else if (head - tail >= n) {
            return tail;
        }
        if (pinned(&h->st[o])) { /* it and the strokes before it into the base */
            int k = o + 1;
            if (h->baking) bake_finish(h), below--, k--; /* (the oldest stroke left the log) */
            if (k > 0) bake(h, k), below -= k;
            continue;
        }
        h->st[o].dlen = 0; /* undone by drawing the page again from now on */
    }
    return 0;
}

/* The closed stroke i's delta (the page against the shadow over its box), and the shadow brought up to the page */
static void delta_take(hist_t *h, int i)
{
    hstroke_t *s = &h->st[i];
    s->dlen = 0;
    if (!h->shadow.px) return;
    uint32_t n = hd_encode(h->page, &h->shadow, &s->bb, 0);
    int64_t at = n ? pool_alloc(h, i, n) : -1;
    s = &h->st[h->st_cur - 1]; /* (i is the newest stroke; pool_alloc may have baked older ones out of the log) */
    if (at >= 0) {
        hd_encode(h->page, &h->shadow, &s->bb, h->pool + at);
        s->doff = (uint32_t)at, s->dlen = n;
    }
    cv_copy_box(&h->shadow, h->page, &s->bb);
}

uint32_t hist_delta_bytes(const hist_t *h)
{
    uint32_t n = 0;
    for (int i = 0; i < h->st_n; i++) n += h->st[i].dlen;
    return n;
}

int hist_settle(hist_t *h, box_t *changed)
{
    if (!h->stale) return 0;
    h->stale = 0;
    memcpy(h->page->px, h->base.px, (size_t)h->page->stride * (size_t)h->page->h);
    for (int i = 0; i < h->st_cur; i++) redraw(h, h->page, i);
    if (h->shadow.px) memcpy(h->shadow.px, h->page->px, (size_t)h->page->stride * (size_t)h->page->h);
    *changed = h->stale_bb;
    return 1;
}

static void settle(hist_t *h)
{
    box_t b;
    hist_settle(h, &b);
}

/* The first k strokes out of the log (their pixels are in the base by now) */
static void cut(hist_t *h, int k)
{
    uint32_t c = h->st[k - 1].first + h->st[k - 1].n;
    memmove(h->pts, h->pts + c, (size_t)(h->pt_n - c) * sizeof(hpt_t));
    h->pt_n -= c;
    memmove(h->st, h->st + k, (size_t)(h->st_n - k) * sizeof(hstroke_t));
    for (int i = 0; i < h->st_n - k; i++) h->st[i].first -= c;
    h->st_n -= k;
    h->st_cur -= k;
    if (h->st_cur < 0) h->st_cur = 0;
}

/* A bake under way (hist_bake_step) finished now: the oldest stroke's rest into the base, and out of the log */
static void bake_finish(hist_t *h)
{
    if (!h->baking) return;
    uint32_t from = h->baking;
    h->baking = 0;
    apply_range(&h->base, &h->st[0], h->pts + h->st[0].first, from, h->st[0].n);
    cut(h, 1);
}

/* The first k strokes into the base image, and out of the log */
static void bake(hist_t *h, int k)
{
    bake_finish(h);
    if (k > h->st_n) k = h->st_n;
    if (k <= 0) return;
    for (int i = 0; i < k; i++) redraw(h, &h->base, i);
    cut(h, k);
}

int hist_bake_step(hist_t *h, uint32_t max_pts)
{
    if (h->st_cur < 1 || (h->st_cur < h->st_cap - HIST_BAKE_AHEAD && !h->baking)) return 0;
    if (h->open && h->st_cur == 1) return 0; /* the oldest stroke is the one being drawn */
    const hstroke_t *s = &h->st[0];
    if (h->stale) return 0;
    if (s->kind == HK_CLEAR || s->kind == HK_FILL || (s->dlen && !h->baking) || s->n <= h->baking + max_pts) {
        if (h->baking) bake_finish(h);
        else bake(h, 1);
        return 1;
    }
    apply_range(&h->base, s, h->pts + s->first, h->baking, h->baking + max_pts);
    h->baking += max_pts;
    return 1;
}


void hist_begin(hist_t *h, int kind)
{
    static const bstyle_t plain = { BR_PEN, SH_BLACK, 1, 0, 0 };
    hist_begin_style(h, kind, &plain);
}

void hist_begin_style(hist_t *h, int kind, const bstyle_t *style)
{
    if (h->open) hist_end(h);
    settle(h);
    h->st_n = h->st_cur; /* a new stroke ends the redo list */
    h->pt_n = h->st_cur ? h->st[h->st_cur - 1].first + h->st[h->st_cur - 1].n : 0;
    if (h->st_cur == h->st_cap) bake(h, 1);
    hstroke_t *s = &h->st[h->st_cur++];
    h->st_n = h->st_cur;
    s->first = h->pt_n;
    s->n = 0;
    s->dlen = 0;
    s->kind = (uint8_t)kind;
    s->style = *style;
    box_clear(&s->bb);
    h->open = 1;
}

void hist_add(hist_t *h, int x, int y, int d, box_t *dirty) { hist_add_t(h, x, y, d, 0, dirty); }

void hist_add_t(hist_t *h, int x, int y, int d, int tilt, box_t *dirty)
{
    if (!h->open) return;
    hstroke_t *s = &h->st[h->st_cur - 1];
    const hpt_t *last = s->n ? &h->pts[s->first + s->n - 1] : 0;
    if (last && last->x == x && last->y == y && last->d == d && last->t == tilt) return;
    if (h->pt_n >= h->pt_cap) {
        /* full: older strokes go into the base; if the open stroke alone fills the log, it goes too and continues
           from its last point as a stroke of its own */
        bake(h, h->st_cur - 1);
        s = &h->st[h->st_cur - 1];
        if (h->pt_n >= h->pt_cap) {
            int kind = s->kind;
            bstyle_t style = s->style;
            hpt_t keep = h->pts[s->first + s->n - 1];
            bake(h, h->st_cur);
            if (h->shadow.px) memcpy(h->shadow.px, h->page->px, (size_t)h->page->stride * (size_t)h->page->h);
            h->open = 0;
            hist_begin_style(h, kind, &style);
            s = &h->st[h->st_cur - 1];
            h->pts[h->pt_n++] = keep;
            s->n = 1;
            box_add(&s->bb, keep.x - keep.d, keep.y - keep.d);
            box_add(&s->bb, keep.x + keep.d, keep.y + keep.d);
        }
        last = s->n ? &h->pts[s->first + s->n - 1] : 0;
    }
    int colour = s->kind == HK_ERASE;
    box_t b;
    box_clear(&b);
    if (!last) brush_segt(&s->style, colour, h->page, x, y, d, tilt, x, y, d, tilt, &b);
    else brush_segt(&s->style, colour, h->page, last->x, last->y, last->d, last->t, x, y, d, tilt, &b);
    h->pts[h->pt_n++] = (hpt_t){ (int16_t)x, (int16_t)y, (uint8_t)d, (uint8_t)tilt };
    s->n++;
    box_union(&s->bb, &b);
    if (dirty) box_union(dirty, &b);
}

void hist_end(hist_t *h)
{
    if (!h->open) return;
    h->open = 0;
    hstroke_t *s = &h->st[h->st_cur - 1];
    if (s->n == 0 && s->kind != HK_CLEAR && s->kind != HK_FILL) h->st_cur--, h->st_n = h->st_cur;
    else delta_take(h, h->st_cur - 1);
}

int hist_can_undo(const hist_t *h) { return h->st_cur > 0; }
int hist_can_redo(const hist_t *h) { return h->st_cur < h->st_n && !h->open; }

int hist_undo(hist_t *h, box_t *changed)
{
    hist_end(h);
    bake_finish(h);
    if (h->st_cur == 0) return 0;
    hstroke_t *s = &h->st[--h->st_cur];
    *changed = s->bb;
    if (!h->stale && s->dlen) { /* the page and the shadow are the state before this stroke again */
        hd_xor(h->page, &s->bb, h->pool + s->doff, s->dlen);
        hd_xor(&h->shadow, &s->bb, h->pool + s->doff, s->dlen);
        return 1;
    }
    if (!h->stale) box_clear(&h->stale_bb), h->stale = 1;
    box_union(&h->stale_bb, &s->bb);
    if (!h->lazy) return hist_settle(h, changed);
    box_clear(changed);
    return 1;
}

int hist_redo(hist_t *h, box_t *changed)
{
    hist_end(h);
    bake_finish(h);
    if (h->st_cur >= h->st_n) return 0;
    hstroke_t *s = &h->st[h->st_cur++];
    *changed = s->bb;
    if (h->stale) {
        box_union(&h->stale_bb, &s->bb);
        if (!h->lazy) return hist_settle(h, changed);
        box_clear(changed);
        return 1;
    }
    redraw(h, h->page, h->st_cur - 1);
    if (s->dlen) hd_xor(&h->shadow, &s->bb, h->pool + s->doff, s->dlen);
    else if (h->shadow.px) cv_copy_box(&h->shadow, h->page, &s->bb);
    return 1;
}

void hist_clear(hist_t *h, box_t *changed)
{
    bake_finish(h);
    settle(h);
    hist_begin(h, HK_CLEAR);
    cv_fill(h->page, 1);
    h->st[h->st_cur - 1].bb = (box_t){ 0, 0, h->page->w - 1, h->page->h - 1 };
    *changed = h->st[h->st_cur - 1].bb;
    hist_end(h);
}

static hpt_t *slots_begin(hist_t *h, int kind, uint32_t n, const bstyle_t *style)
{
    if (n == 0 || n > h->pt_cap) return 0;
    hist_begin_style(h, kind, style);
    while (h->pt_cap - h->pt_n < n && h->st_cur > 1) bake(h, 1);
    return h->pts + h->pt_n;
}

hpt_t *hist_fill_begin(hist_t *h, uint32_t n, const bstyle_t *style) { return slots_begin(h, HK_FILL, n, style); }
void hist_fill_end(hist_t *h, uint32_t n, const box_t *bb)
{
    if (!h->open) return;
    hstroke_t *s = &h->st[h->st_cur - 1];
    s->n = n;
    s->bb = *bb;
    h->pt_n += n;
    apply(h->page, s, h->pts + s->first);
    hist_end(h);
}

int hist_fill_mask(hist_t *h, const bf_t *r, const bstyle_t *style)
{
    if (!h->shadow.px || !h->pool_cap) return 0;
    hist_begin_style(h, HK_FILL, style);
    hstroke_t *s = &h->st[h->st_cur - 1];
    s->bb = r->bb;
    bf_paint(h->page, r, style->shade);
    hist_end(h);
    s = &h->st[h->st_cur - 1];
    if (!s->dlen) { /* no room for its delta (a pool halved by short memory): the page as it is becomes the base, no undo */
        memcpy(h->base.px, h->page->px, (size_t)h->page->stride * (size_t)h->page->h);
        h->baking = 0, h->pt_n = 0, h->st_n = h->st_cur = 0;
        return 2;
    }
    return 1;
}
