/* SPDX-License-Identifier: MIT */
#include "hist.h"
#include <string.h>
#include "fill.h"

void hist_init(hist_t *h, canvas_t *page, uint8_t *base_px, hpt_t *pts, uint32_t pt_cap, hstroke_t *st, int st_cap)
{
    h->page = page;
    h->base = *page;
    h->base.px = base_px;
    h->pts = pts;
    h->pt_cap = pt_cap;
    h->st = st;
    h->st_cap = st_cap;
    hist_reset(h);
}

void hist_reset(hist_t *h)
{
    memcpy(h->base.px, h->page->px, (size_t)h->page->stride * (size_t)h->page->h);
    h->pt_n = 0;
    h->st_n = h->st_cur = 0;
    h->open = 0;
}

static void apply(canvas_t *c, const hstroke_t *s, const hpt_t *p)
{
    if (s->kind == HK_CLEAR) return cv_fill(c, 1);
    if (s->kind == HK_FILL) return bf_apply(c, p, s->n, s->style.shade);
    int colour = s->kind == HK_ERASE;
    for (uint32_t i = 0; i < s->n; i++) {
        const hpt_t *a = i ? &p[i - 1] : &p[i];
        brush_seg(&s->style, colour, c, a->x, a->y, a->d, p[i].x, p[i].y, p[i].d, 0);
    }
}

/* The first k strokes into the base image, and out of the log */
static void bake(hist_t *h, int k)
{
    if (k > h->st_n) k = h->st_n;
    if (k <= 0) return;
    for (int i = 0; i < k; i++) apply(&h->base, &h->st[i], h->pts + h->st[i].first);
    uint32_t cut = h->st[k - 1].first + h->st[k - 1].n;
    memmove(h->pts, h->pts + cut, (size_t)(h->pt_n - cut) * sizeof(hpt_t));
    h->pt_n -= cut;
    memmove(h->st, h->st + k, (size_t)(h->st_n - k) * sizeof(hstroke_t));
    for (int i = 0; i < h->st_n - k; i++) h->st[i].first -= cut;
    h->st_n -= k;
    h->st_cur -= k;
    if (h->st_cur < 0) h->st_cur = 0;
}

static void replay(hist_t *h)
{
    memcpy(h->page->px, h->base.px, (size_t)h->page->stride * (size_t)h->page->h);
    for (int i = 0; i < h->st_cur; i++) apply(h->page, &h->st[i], h->pts + h->st[i].first);
}

void hist_begin(hist_t *h, int kind)
{
    static const bstyle_t plain = { BR_FINELINER, SH_BLACK, 1, 0, 0 };
    hist_begin_style(h, kind, &plain);
}

void hist_begin_style(hist_t *h, int kind, const bstyle_t *style)
{
    if (h->open) hist_end(h);
    h->st_n = h->st_cur; /* a new stroke ends the redo list */
    h->pt_n = h->st_cur ? h->st[h->st_cur - 1].first + h->st[h->st_cur - 1].n : 0;
    if (h->st_cur == h->st_cap) bake(h, 1);
    hstroke_t *s = &h->st[h->st_cur++];
    h->st_n = h->st_cur;
    s->first = h->pt_n;
    s->n = 0;
    s->kind = (uint8_t)kind;
    s->style = *style;
    box_clear(&s->bb);
    h->open = 1;
}

void hist_add(hist_t *h, int x, int y, int d, box_t *dirty)
{
    if (!h->open) return;
    hstroke_t *s = &h->st[h->st_cur - 1];
    const hpt_t *last = s->n ? &h->pts[s->first + s->n - 1] : 0;
    if (last && last->x == x && last->y == y && last->d == d) return;
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
    if (!last) brush_seg(&s->style, colour, h->page, x, y, d, x, y, d, &b);
    else brush_seg(&s->style, colour, h->page, last->x, last->y, last->d, x, y, d, &b);
    h->pts[h->pt_n++] = (hpt_t){ (int16_t)x, (int16_t)y, (uint8_t)d };
    s->n++;
    box_union(&s->bb, &b);
    if (dirty) box_union(dirty, &b);
}

void hist_end(hist_t *h)
{
    if (!h->open) return;
    h->open = 0;
    if (h->st[h->st_cur - 1].n == 0 && h->st[h->st_cur - 1].kind != HK_CLEAR) h->st_cur--, h->st_n = h->st_cur;
}

int hist_can_undo(const hist_t *h) { return h->st_cur > 0; }
int hist_can_redo(const hist_t *h) { return h->st_cur < h->st_n && !h->open; }

int hist_undo(hist_t *h, box_t *changed)
{
    hist_end(h);
    if (h->st_cur == 0) return 0;
    h->st_cur--;
    *changed = h->st[h->st_cur].bb;
    replay(h);
    return 1;
}

int hist_redo(hist_t *h, box_t *changed)
{
    hist_end(h);
    if (h->st_cur >= h->st_n) return 0;
    apply(h->page, &h->st[h->st_cur], h->pts + h->st[h->st_cur].first);
    *changed = h->st[h->st_cur].bb;
    h->st_cur++;
    return 1;
}

void hist_clear(hist_t *h, box_t *changed)
{
    hist_begin(h, HK_CLEAR);
    cv_fill(h->page, 1);
    h->st[h->st_cur - 1].bb = (box_t){ 0, 0, h->page->w - 1, h->page->h - 1 };
    *changed = h->st[h->st_cur - 1].bb;
    hist_end(h);
}

hpt_t *hist_fill_begin(hist_t *h, uint32_t n, const bstyle_t *style)
{
    if (n == 0 || n > h->pt_cap) return 0;
    hist_begin_style(h, HK_FILL, style);
    while (h->pt_cap - h->pt_n < n && h->st_cur > 1) bake(h, 1);
    return h->pts + h->pt_n;
}

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
