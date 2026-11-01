/* The pen size: the toolbar slider and the setting. SPDX-License-Identifier: MIT */
#include "sketch.h"
#include "sk_cfg.h"
#include "skui.h"

static void size_store(sketch_t *s, int size)
{
    s->sty.size = (uint8_t)size;
    if (s->sty.brush == BR_ERASER) s->era_size = (uint8_t)size, sk_cfg_num(s, "esize", size);
    else s->ink_size = (uint8_t)size, sk_cfg_num(s, "size", size);
}

void sk_set_size(sketch_t *s, int size, uint32_t now)
{
    if (size < SK_SIZE_MIN || size > SK_SIZE_MAX) return;
    size_store(s, size);
    char t[12] = "Size: ";
    int n = 6;
    if (size >= 10) t[n++] = (char)('0' + size / 10);
    t[n++] = (char)('0' + size % 10), t[n] = 0;
    sk_picked(s, t, now);
}

/* The knob follows the pen along the track: the strip is repainted in the framebuffer, only the slider's box is pushed */
void sk_slider_drag(sketch_t *s, int ux)
{
    int size = tb_slider_size(&s->tb, ux);
    if (size == s->sty.size && s->tb_hot == TI_SIZE) return;
    s->sty.size = (uint8_t)size;
    s->tb_hot = TI_SIZE;
    sk_tb_paint(s);
    int x0, y0, x1, y1;
    box_t b;
    gfx_t g;
    gfx_init(&g, s->fb.px, s->fb.w, s->fb.h, s->fb.stride, s->orient);
    tb_slider_box(&s->tb, &x0, &y0, &x1, &y1);
    ui_fb_box(&g, x0, y0, x1, y1, &b);
    box_clip(&b, s->fb.w, s->fb.h);
    rs_add(&s->pend, &b, s->fb.w, s->fb.h);
}
