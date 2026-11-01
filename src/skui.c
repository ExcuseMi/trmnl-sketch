/* SPDX-License-Identifier: MIT */
#include "skui.h"
#include <string.h>
#include "font.h"
#include "sk_cfg.h"

enum { BLK = GFX_BLACK, WHT = GFX_WHITE };

void ui_fb_box(const gfx_t *g, int x0, int y0, int x1, int y1, box_t *out)
{
    int ax, ay, bx, by;
    app_orient_to_fb(g->orient, g->fb_w, g->fb_h, x0, y0, &ax, &ay);
    app_orient_to_fb(g->orient, g->fb_w, g->fb_h, x1, y1, &bx, &by);
    box_clear(out);
    box_add(out, ax, ay);
    box_add(out, bx, by);
}

static const font_face_t *face(int bold, int size) { return font_get(bold ? FONT_BOLD : FONT_MEDIUM, size); }
static int text_w(const font_face_t *f, const char *s) { return (int)((font_width64(f, s, 0) + 32) >> 6); }
static void text(gfx_t *g, const font_face_t *f, int x, int y, const char *s, int c) { gfx_text(g, f, (int32_t)x * 64, y, s, 0, c); }

static void panel(gfx_t *g, int y, int w, int h, box_t *out)
{
    gfx_rrect(g, 24, y, 24 + w, y + h, 10, 4, WHT, BLK);
    ui_fb_box(g, 24, y, 24 + w, y + h, out);
}

void ui_toast(gfx_t *g, const char *s, int y, box_t *out)
{
    const font_face_t *f = face(1, 34);
    int w = text_w(f, s) + 40;
    panel(g, y, w, 64, out);
    text(g, f, 44, y + 44, s, BLK);
}

void ui_legend(gfx_t *g, box_t *out)
{
    static const char *const lines[] = { "Tablet buttons: 1 undo, 2 redo, 3 eraser, 4 toolbar,", "5 clear, 6 new page, 7 previous, 8 next.",
        "Pen button: eraser while held. Toolbar: brushes, shades, sizes.", "Touch bar: left undo, right redo, middle menu;", "swipe for pages, hold the middle to clear." };
    const font_face_t *f = face(0, 34);
    int w = 0;
    for (int i = 0; i < 5; i++)
        if (text_w(f, lines[i]) > w) w = text_w(f, lines[i]);
    w += 40;
    panel(g, 24, w, 20 + 5 * 44, out);
    for (int i = 0; i < 5; i++) text(g, f, 44, 24 + 46 + i * 44, lines[i], BLK);
}

void ui_cursor(canvas_t *fb, int cx, int cy, int brush, int size, box_t *out)
{
    int r = brush_cursor_r(brush, size);
    box_clear(out);
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++)
            if (brush_cursor_px(brush, size, dx, dy)) cv_flip(fb, cx + dx, cy + dy);
    box_add(out, cx - r, cy - r);
    box_add(out, cx + r, cy + r);
    box_clip(out, fb->w, fb->h);
}

static void put(char *dst, size_t cap, const char *s)
{
    size_t n = strlen(dst);
    while (*s && n + 1 < cap) dst[n++] = *s++;
    dst[n] = 0;
}

void share_view(share_view_t *v, int rc, const app_webshare_status_t *st)
{
    memset(v, 0, sizeof(*v));
    if (rc != 0) {
        put(v->note, sizeof(v->note),
            rc == APP_ENOMEM ? "Not enough memory for the phone page." : rc == APP_ENONET ? "No network could be started." : "The Wi-Fi page could not start.");
        return;
    }
    if (!st->running) {
        put(v->note, sizeof(v->note), "The page switched itself off. Touch the bar to leave.");
        return;
    }
    if (st->mode == APP_WEBSHARE_AP) {
        put(v->steps[0], sizeof(v->steps[0]), "1. Join the network \"");
        put(v->steps[0], sizeof(v->steps[0]), st->network);
        put(v->steps[0], sizeof(v->steps[0]), "\" with your phone.");
        put(v->steps[1], sizeof(v->steps[1]), "2. Open this address, then type the code:");
    } else {
        put(v->steps[0], sizeof(v->steps[0]), "1. With your phone on the same network open:");
        put(v->steps[1], sizeof(v->steps[1]), "2. Then type the code:");
    }
    put(v->address, sizeof(v->address), st->address);
    put(v->code, sizeof(v->code), st->code);
    v->up = st->locked == 0;
    put(v->note, sizeof(v->note),
        st->locked == 1 ? "Too many wrong codes: wait a minute." : st->locked == 2 ? "Too many wrong codes: the page is off." : "Touch the bar to stop. Your sketches are the .png files.");
}

int share_view_same(const share_view_t *a, const share_view_t *b) { return !memcmp(a, b, sizeof(*a)); }

void ui_share(gfx_t *g, const app_touchbar_t *bar, const share_view_t *v)
{
    (void)bar;
    gfx_fill(g, WHT);
    const font_face_t *f = face(0, 34), *big = face(1, 64), *dig = face(1, 110);
    text(g, big, 60, 130, v->settings ? "Sketch settings" : "Download sketches", BLK);
    int y = 220;
    if (v->steps[0][0]) text(g, f, 60, y, v->steps[0], BLK), y += 50;
    if (v->steps[1][0]) text(g, f, 60, y, v->steps[1], BLK), y += 90;
    if (v->address[0]) text(g, big, 60, y, v->address, BLK), y += 150;
    if (v->code[0] && v->up) text(g, dig, 60, y, v->code, BLK), y += 100;
    text(g, f, 60, y, v->note, BLK);
}
