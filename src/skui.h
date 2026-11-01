/* What Sketch draws besides ink: the hover cursor, the corner notes (legend, toast), the menu and the Wi-Fi screen.
   Text is drawn in the user's frame of the X's position (gfx.h). SPDX-License-Identifier: MIT */
#ifndef SKUI_H
#define SKUI_H
#include <app_api.h>
#include "canvas.h"
#include "gfx.h"

/* The framebuffer box of a user-frame rectangle (inclusive corners) */
void ui_fb_box(const gfx_t *g, int x0, int y0, int x1, int y1, box_t *out);

/* A one-line note in the corner of the screen; *out: the framebuffer box it covers */
void ui_toast(gfx_t *g, const char *text, box_t *out);
/* The button legend in the corner */
void ui_legend(gfx_t *g, box_t *out);
/* The hover cursor: a ring of radius r (and a dot) by inverting pixels; *out: its box. Undone by copying the page back */
void ui_cursor(canvas_t *fb, int cx, int cy, int r, box_t *out);

/* The menu, on a white screen. page: 1-based, of pages. bar: where the touch bar's zones are */
void ui_menu(gfx_t *g, const app_touchbar_t *bar, int page, int pages, int tablet);

/* The Wi-Fi screen's text from the page's status (pure, so it is tested) */
typedef struct {
    char steps[2][96];
    char address[48];
    char code[8];
    char note[96];
    int up;
} share_view_t;
void share_view(share_view_t *v, int start_rc, const app_webshare_status_t *st);
int share_view_same(const share_view_t *a, const share_view_t *b);
void ui_share(gfx_t *g, const app_touchbar_t *bar, const share_view_t *v);
#endif
