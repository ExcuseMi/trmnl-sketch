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
        sk_flash(s, &all); /* then a clean whole-screen refresh: the guide leaves no trace */
    }
    sk_picked(s, t, now);
}
