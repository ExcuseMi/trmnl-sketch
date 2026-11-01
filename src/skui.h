/* What Sketch draws besides ink: the hover cursor, the corner notes (legend, toast), the menu and the Wi-Fi screen.
   Text is drawn in the user's frame of the X's position (gfx.h). SPDX-License-Identifier: MIT */
#ifndef SKUI_H
#define SKUI_H
#include <app_api.h>
#include "brush.h"
#include "canvas.h"
#include "gfx.h"

/* The framebuffer box of a user-frame rectangle (inclusive corners) */
void ui_fb_box(const gfx_t *g, int x0, int y0, int x1, int y1, box_t *out);

/* A one-line note in the corner of the screen; *out: the framebuffer box it covers */
void ui_toast(gfx_t *g, const char *text, int y, box_t *out); /* the panel's top edge at y (24: the corner) */
/* The button legend in the corner */
void ui_legend(gfx_t *g, box_t *out);
/* The hover cursor of a brush at a size (its shape: brush.h) black with a white outline; *out: its box. Undone by copying the
   page back */
void ui_cursor(canvas_t *fb, int cx, int cy, int brush, int size, box_t *out);

#define ARROW_W 28
#define ARROW_H 48
/* The plain arrow pointer of the gallery and the menus (tip at user-frame (ux, uy) of `orient`), black with a white outline;
   *out: the framebuffer box it covers (clipped). Undone by copying the clean screen back */
void ui_arrow(canvas_t *fb, int orient, int ux, int uy, box_t *out);

/* The Wi-Fi screen's text from the page's status (pure, so it is tested) */
typedef struct {
    char steps[2][96];
    char address[48];
    char code[8];
    char note[96];
    int up;
    int settings; /* the settings form, not the files */
} share_view_t;
void share_view(share_view_t *v, int start_rc, const app_webshare_status_t *st);
int share_view_same(const share_view_t *a, const share_view_t *b);
void ui_share(gfx_t *g, const app_touchbar_t *bar, const share_view_t *v);
#endif
