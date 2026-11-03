/* Pages in the controller: saving the page on screen, opening, leaving and making pages. SPDX-License-Identifier: MIT */
#include "sketch.h"
#include <string.h>

#define RETRY_MS 5000

/* ---- saving ---- */

int sk_do_save(sketch_t *s, uint32_t now)
{
    if (s->bad_pending) { /* the unreadable file stays, under another name */
        if (pg_keep_bad(&s->pg, s->cur) < 0 && pg_index(&s->pg, s->cur) >= 0) return s->save_failed = 1, s->save_at = now + RETRY_MS, PG_IO;
        s->bad_pending = 0;
    }
    int rc = pg_save(&s->pg, s->cur, &s->page);
    if (rc) return s->save_failed = 1, s->save_at = now + RETRY_MS, rc;
    s->dirty = s->save_failed = 0;
    sk_paper_store(s);
    gal_invalidate(&s->cache, s->cur);
    return 0;
}

void sk_save_now(sketch_t *s)
{
    if (s->dirty) sk_do_save(s, s->last_push);
}

/* ---- pages ---- */

int sk_is_blank(const canvas_t *c)
{
    int rb = (c->w + 7) / 8;
    for (int y = 0; y < c->h; y++)
        for (int x = 0; x < rb; x++)
            if (c->px[y * c->stride + x] != 0xff) return 0;
    return 1;
}

void sk_open_page(sketch_t *s, int num, uint32_t now) { sk_opened(s, num, pg_load(&s->pg, num, &s->page), now); }

void sk_opened(sketch_t *s, int num, int rc, uint32_t now)
{
    s->cur = num;
    s->page_rev++;
    s->bad_pending = 0;
    if (rc == PG_MISSING) cv_fill(&s->page, 1);
    if (rc == PG_BAD) s->bad_pending = 1;
    hist_reset(&s->hist);
    sk_paper_open(s, num);
    s->cursor = s->note_on = s->legend = s->stroke = 0;
    sk_show_page(s);
    s->want_full = 1;
    s->dirty = 0;
    sk_cfg_num(s, "cur", num);
    (void)now;
}

static const char *page_text(sketch_t *s, char *buf)
{
    int pos = 0;
    for (int i = 0; i < s->pg.n; i++)
        if (s->pg.nums[i] <= s->cur) pos = i + 1;
    int v[2] = { pos, s->pg.n }, n = 0;
    const char *pre = "Page ";
    while (*pre) buf[n++] = *pre++;
    for (int k = 0; k < 2; k++) {
        char d[8];
        int m = 0, x = v[k];
        do d[m++] = (char)('0' + x % 10), x /= 10; while (x);
        while (m) buf[n++] = d[--m];
        if (k == 0) buf[n++] = ' ', buf[n++] = 'o', buf[n++] = 'f', buf[n++] = ' ';
    }
    buf[n] = 0;
    return buf;
}

void sk_make_page(sketch_t *s, uint32_t now);

int sk_save_page(sketch_t *s, uint32_t now) /* writes the page on screen if it changed: the menus work on its file; 0 when it could not */
{
    hist_end(&s->hist);
    s->stroke = 0;
    return !s->dirty || sk_is_blank(&s->page) || !sk_do_save(s, now);
}

int sk_leave_page(sketch_t *s, uint32_t now) /* saves the page on screen first; 0 when it could not */
{
    hist_end(&s->hist);
    s->stroke = 0;
    if (sk_is_blank(&s->page)) { /* a blank page is not kept: at most one exists, the one on screen (owner 2026-11-01) */
        if (s->bad_pending) return 1; /* an unreadable file is not blank: it stays */
        pg_remove(&s->pg, s->cur);
        s->dirty = 0;
        return 1;
    }
    if (s->dirty && sk_do_save(s, now)) return 0;
    return 1;
}

void sk_goto_page(sketch_t *s, int index, uint32_t now)
{
    if (!sk_leave_page(s, now)) return sk_toast(s, "Could not save this page", now);
    pg_scan(&s->pg);
    if (s->pg.n == 0) return sk_make_page(s, now);
    if (index < 0 || index >= s->pg.n) return sk_toast(s, index < 0 ? "First page" : "Last page", now);
    sk_open_page(s, s->pg.nums[index], now);
    char b[24];
    sk_toast(s, page_text(s, b), now);
}

/* A new page only from a page with ink: no run of empty pages (owner 2026-11-01) */
void sk_new_page(sketch_t *s, uint32_t now)
{
    if (sk_is_blank(&s->page)) return sk_toast(s, "This page is still empty", now);
    sk_make_page(s, now);
}

void sk_make_page(sketch_t *s, uint32_t now)
{
    if (!sk_leave_page(s, now)) return sk_toast(s, "Could not save this page", now);
    pg_scan(&s->pg);
    int num = pg_next_num(&s->pg);
    if (!num) return sk_toast(s, "No more pages", now);
    s->cur = num;
    cv_fill(&s->page, 1);
    s->page_rev++;
    s->bad_pending = 0;
    hist_reset(&s->hist);
    sk_paper_new(s);
    s->cursor = s->note_on = s->legend = 0;
    sk_show_page(s);
    s->want_full = 1;
    sk_cfg_num(s, "cur", num);
    char b[24];
    sk_toast(s, page_text(s, b), now);
}

