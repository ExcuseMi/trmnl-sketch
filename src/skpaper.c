/* The page's paper in the controller: loading it with the page, saving it beside the page (the sidecar and the printed
   copy with the paper under the ink), the screen overlay, the page menu's Paper item. SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include <string.h>
#include "sketch.h"

void sk_paper_new(sketch_t *s) { s->paper = s->paper_def, s->paper_sp = s->paper_sp_def; }

void sk_paper_open(sketch_t *s, int num)
{
    int k = 0, sp = 0;
    if (pg_index(&s->pg, num) < 0) return sk_paper_new(s); /* a page with no file yet */
    pg_paper_load(&s->pg, num, &k, &sp);
    s->paper = k, s->paper_sp = k ? sp : s->paper_sp_def;
}

void sk_paper_store(sketch_t *s)
{
    pg_paper_save(&s->pg, s->cur, s->paper, s->paper_sp);
    if (!s->paper) return;
    canvas_t c = s->page;
    size_t bytes = (size_t)c.stride * (size_t)c.h;
    c.px = malloc(bytes);
    if (!c.px) return; /* printing then uses the plain page */
    memcpy(c.px, s->page.px, bytes);
    box_t all = { 0, 0, c.w - 1, c.h - 1 };
    pp_overlay(&c, &all, s->paper, s->paper_sp);
    pg_save_as(&s->pg, s->cur, "-paper.png", &c);
    free(c.px);
}

void sk_paper_blit(sketch_t *s, const box_t *b) { pp_overlay(&s->fb, b, s->paper, s->paper_sp); }

void sk_set_paper(sketch_t *s, int kind, uint32_t now)
{
    if (kind < 0 || kind >= PP_N) return;
    s->paper = kind;
    s->paper_sp = s->paper_sp ? s->paper_sp : s->paper_sp_def;
    gal_invalidate(&s->cache, s->cur);
    if (!sk_is_blank(&s->page)) s->dirty = 1, s->save_at = now + SK_AUTOSAVE_MS;
}
