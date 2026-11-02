/* The Recent slots: the last SK_RECENT combinations of tone, brush and size a stroke was finished with, most recent first,
   no duplicates, never the eraser or the bucket. Kept in the settings as rc0 .. rc4 ("tone.brush.size"). SPDX-License-Identifier: MIT */
#include "sketch.h"

static int same(const bstyle_t *a, const bstyle_t *b) { return a->brush == b->brush && a->shade == b->shade && a->size == b->size; }

static void save(sketch_t *s)
{
    for (int i = 0; i < SK_RECENT; i++) {
        char key[4] = { 'r', 'c', (char)('0' + i), 0 }, v[12];
        int n = 0;
        if (i < s->rc_n) {
            const int f[3] = { s->rc[i].shade, s->rc[i].brush, s->rc[i].size };
            for (int k = 0; k < 3; k++) {
                if (f[k] >= 10) v[n++] = (char)('0' + f[k] / 10);
                v[n++] = (char)('0' + f[k] % 10);
                if (k < 2) v[n++] = '.';
            }
        }
        if (s->env.cfg_set) s->env.cfg_set(s->env.user, key, v, n);
    }
}

void sk_recent_load(sketch_t *s)
{
    s->rc_n = 0;
    for (int i = 0; i < SK_RECENT && s->env.cfg_get; i++) {
        char key[4] = { 'r', 'c', (char)('0' + i), 0 }, v[12];
        int n = s->env.cfg_get(s->env.user, key, v, 11), f[3] = { 0, 0, 0 }, k = 0, ok = n > 0;
        for (int j = 0; j < n && ok; j++) {
            if (v[j] >= '0' && v[j] <= '9' && f[k] < 1000) f[k] = f[k] * 10 + v[j] - '0';
            else if (v[j] == '.' && k < 2) k++;
            else ok = 0;
        }
        ok = ok && k == 2 && f[0] <= SH_WHITE && f[1] < BR_N && f[1] != BR_ERASER && f[1] != BR_FILL && f[2] >= SK_SIZE_MIN && f[2] <= SK_SIZE_MAX;
        if (!ok) continue; /* a damaged value: that slot stays empty */
        bstyle_t e = { (uint8_t)f[1], (uint8_t)f[0], (uint8_t)f[2], 0, 0 };
        s->rc[s->rc_n++] = e;
    }
}

void sk_recent_add(sketch_t *s, const bstyle_t *st)
{
    if (brush_is_eraser(st->brush) || brush_is_fill(st->brush) || st->shade > SH_WHITE) return;
    bstyle_t e = { st->brush, st->shade, st->size, 0, 0 };
    int at = 0;
    while (at < s->rc_n && !same(&s->rc[at], &e)) at++;
    if (at == 0 && s->rc_n > 0) return; /* already the newest: nothing to save */
    if (at == s->rc_n) { /* new: the oldest falls off the end */
        if (s->rc_n < SK_RECENT) s->rc_n++;
        at = s->rc_n - 1;
    }
    for (int i = at; i > 0; i--) s->rc[i] = s->rc[i - 1];
    s->rc[0] = e;
    save(s);
    if (s->tb_on) { /* the strip in the framebuffer, and only the Recent slots pushed */
        sk_tb_paint(s);
        gfx_t g;
        box_t b;
        gfx_init(&g, s->fb.px, s->fb.w, s->fb.h, s->fb.stride, s->orient);
        ui_fb_box(&g, s->tb.cx[TI_RECENT], s->tb.cyi[TI_RECENT], s->tb.cx[TI_RECENT + SK_RECENT - 1] + s->tb.cell - 1, s->tb.cyi[TI_RECENT] + s->tb.ch - 1, &b);
        box_clip(&b, s->fb.w, s->fb.h);
        sk_mark(s, &b);
    }
}

void sk_recent_pick(sketch_t *s, int slot, uint32_t now)
{
    if (slot < 0 || slot >= s->rc_n) return sk_toast(s, "Empty: draw something to fill it", now);
    bstyle_t e = s->rc[slot];
    int tool = s->sty.tool;
    sk_set_brush(s, e.brush, now); /* (ends a text, leaves the eraser) */
    s->sty.tool = (uint8_t)tool;
    s->sty.shade = e.shade;
    s->sty.size = s->ink_size = e.size;
    sk_cfg_num(s, "tone", e.shade);
    sk_cfg_num(s, "size", e.size);
    sk_picked(s, brush_name(e.brush), now);
}
