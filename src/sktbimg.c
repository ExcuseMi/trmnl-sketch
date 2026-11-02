/* The toolbar strip's image: tb_draw runs only when what it shows changed (the key); everything else puts the strip's
   pixels back from the image. A pen move over the open strip cost two whole-strip paints before (the arrow's old box
   restored through fb_put, then the strip again: 0.87 ms a move on a PC, tens of ms on the X at ~100 moves a second):
   the lag of 2026-11-02. SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include <string.h>
#include "sketch.h"
#include "toolbar.h"

static void key_of(const sketch_t *s, tb_key_t *k)
{
    memset(k, 0, sizeof(*k)); /* the padding too: the keys are compared with memcmp */
    k->sty = s->sty;
    for (int i = 0; i < s->rc_n && i < SK_RECENT; i++) k->rc[i] = s->rc[i];
    k->hot = s->tb_hot, k->rc_n = s->rc_n, k->smooth = s->smooth, k->orient = s->orient, k->box = s->tb_box;
}

static int img_row(const sketch_t *s) { return ((s->tb_box.x1 >> 3) - (s->tb_box.x0 >> 3)) + 1; }

/* Bits x0..x1 of a row from src to dst, both indexed from pixel 0 */
static void row_bits(uint8_t *dst, const uint8_t *src, int x0, int x1)
{
    int c0 = x0 >> 3, c1 = x1 >> 3;
    uint8_t m0 = (uint8_t)(0xff >> (x0 & 7)), m1 = (uint8_t)(0xff << (7 - (x1 & 7)));
    if (c0 == c1) {
        uint8_t m = m0 & m1;
        dst[c0] = (uint8_t)((dst[c0] & ~m) | (src[c0] & m));
        return;
    }
    dst[c0] = (uint8_t)((dst[c0] & ~m0) | (src[c0] & m0));
    if (c1 > c0 + 1) memcpy(dst + c0 + 1, src + c0 + 1, (size_t)(c1 - c0 - 1));
    dst[c1] = (uint8_t)((dst[c1] & ~m1) | (src[c1] & m1));
}

/* The image's pixels inside b (clipped to the strip's exact pixels) onto the framebuffer */
static void img_put(sketch_t *s, const box_t *b)
{
    box_t c = *b;
    c.x0 &= ~7, c.x1 |= 7; /* the page was copied over whole bytes (cv_copy_box) */
    const box_t *t = &s->tb_px;
    if (c.x0 < t->x0) c.x0 = t->x0;
    if (c.y0 < t->y0) c.y0 = t->y0;
    if (c.x1 > t->x1) c.x1 = t->x1;
    if (c.y1 > t->y1) c.y1 = t->y1;
    if (c.x0 > c.x1 || c.y0 > c.y1) return;
    int rb = img_row(s), bx = s->tb_box.x0 >> 3; /* the image's first byte is the framebuffer's byte bx */
    for (int y = c.y0; y <= c.y1; y++)
        row_bits(s->fb.px + y * s->fb.stride, s->tb_img + (size_t)(y - s->tb_box.y0) * (size_t)rb - bx, c.x0, c.x1);
}

static void img_save(sketch_t *s)
{
    int rb = img_row(s), rows = s->tb_box.y1 - s->tb_box.y0 + 1;
    size_t need = (size_t)rb * (size_t)rows;
    if (need > s->tb_img_cap) {
        uint8_t *p = realloc(s->tb_img, need);
        if (!p) { /* no image: every restore paints the strip again (as before the image existed) */
            s->tb_img_ok = 0;
            return;
        }
        s->tb_img = p, s->tb_img_cap = need;
    }
    for (int y = 0; y < rows; y++) memcpy(s->tb_img + (size_t)y * (size_t)rb, s->fb.px + (s->tb_box.y0 + y) * s->fb.stride + (s->tb_box.x0 >> 3), (size_t)rb);
    key_of(s, &s->tb_key);
    s->tb_img_ok = 1;
}

static int img_fresh(const sketch_t *s)
{
    tb_key_t k;
    key_of(s, &k);
    return s->tb_img_ok && !memcmp(&k, &s->tb_key, sizeof(k));
}

void sk_tb_paint(sketch_t *s)
{
    if (img_fresh(s)) return img_put(s, &s->tb_px);
    gfx_t g;
    gfx_init(&g, s->fb.px, s->fb.w, s->fb.h, s->fb.stride, s->orient);
    tb_draw(&g, &s->tb, &s->sty, s->tb_hot, s->rc, s->rc_n, s->smooth);
    s->tb_paints++;
    img_save(s);
}

void sk_tb_restore(sketch_t *s, const box_t *b)
{
    if (!img_fresh(s)) return sk_tb_paint(s);
    img_put(s, b);
}

void sk_tb_img_free(sketch_t *s)
{
    free(s->tb_img);
    s->tb_img = 0, s->tb_img_cap = 0, s->tb_img_ok = 0;
}
