/* SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 */
#include "skui.h"
#include <string.h>
#include "font.h"
#include "sk_cfg.h"
#include "skmenu.h"

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
    static const char *const lines[] = { "Tablet buttons: 1 undo, 2 redo, 3 eraser, 4 toolbar,", "5 clear, 6 new page, 7 and 8 page menu.",
        "Pen button: eraser while held. Toolbar: tools, colours, sizes.", "Menu: print, send, new page, gallery, exit." };
    const font_face_t *f = face(0, 34);
    int w = 0;
    for (int i = 0; i < 4; i++)
        if (text_w(f, lines[i]) > w) w = text_w(f, lines[i]);
    w += 40;
    panel(g, 24, w, 20 + 4 * 44, out);
    for (int i = 0; i < 4; i++) text(g, f, 44, 24 + 46 + i * 44, lines[i], BLK);
}

/* Draws a black shape with a white outline BR_CUR_OUT wide: `in(ctx, dx, dy)` for dx, dy within +-r */
#define CUR_MAXR 100 /* the text I-beam of the largest size */
typedef int (*cur_in_fn)(const void *ctx, int dx, int dy);

/* A cursor's shape and outline, worked out once for a shape and size (bit 0: inside, bits 1-2: the colour + 1) and
   kept: a hover only draws them (the bucket's took 2 ms on the host, 40 ms on the X, at each new hover; 2026-11-03).
   The brush cursor and the menu arrow keep one each, so the menus do not throw the brush's away */
typedef struct {
    int a, b, r, valid;
    uint8_t *m;
} cur_cache_t;
static uint8_t brush_grid[(2 * (CUR_MAXR + BR_CUR_OUT) + 1) * (2 * (CUR_MAXR + BR_CUR_OUT) + 1)];
static uint8_t arrow_grid[(2 * (ARROW_H / 2 + 1 + BR_CUR_OUT) + 1) * (2 * (ARROW_H / 2 + 1 + BR_CUR_OUT) + 1)];
static cur_cache_t brush_cc = { 0, 0, 0, 0, brush_grid }, arrow_cc = { 0, 0, 0, 0, arrow_grid };

static void cur_make(cur_cache_t *c, int r, cur_in_fn in, const void *ctx, int ka, int kb)
{
    if (c->valid && c->a == ka && c->b == kb && c->r == r) return;
    int n = 2 * (r + BR_CUR_OUT) + 1, o = r + BR_CUR_OUT;
    uint8_t *m = c->m;
    for (int dy = -o; dy <= o; dy++)
        for (int dx = -o; dx <= o; dx++) m[(dy + o) * n + dx + o] = (dx >= -r && dx <= r && dy >= -r && dy <= r) && in(ctx, dx, dy);
    for (int dy = -o; dy <= o; dy++)
        for (int dx = -o; dx <= o; dx++) {
            int colour = -1;
            if (m[(dy + o) * n + dx + o] & 1) colour = 0;
            else
                for (int y = dy - BR_CUR_OUT; y <= dy + BR_CUR_OUT && colour < 0; y++)
                    for (int x = dx - BR_CUR_OUT; x <= dx + BR_CUR_OUT; x++)
                        if (x >= -o && x <= o && y >= -o && y <= o && (m[(y + o) * n + x + o] & 1)) {
                            colour = 1;
                            break;
                        }
            m[(dy + o) * n + dx + o] |= (uint8_t)((colour + 1) << 1);
        }
    c->a = ka, c->b = kb, c->r = r, c->valid = 1;
}

/* Draws a black shape with a white outline BR_CUR_OUT wide (the cache made by cur_make) */
static void cur_outline(canvas_t *fb, int cx, int cy, const cur_cache_t *c, int orient, box_t *out)
{
    int r = c->r, n = 2 * (r + BR_CUR_OUT) + 1, o = r + BR_CUR_OUT;
    box_clear(out);
    for (int dy = -o; dy <= o; dy++)
        for (int dx = -o; dx <= o; dx++) {
            int colour = (c->m[(dy + o) * n + dx + o] >> 1) - 1;
            int fx, fy;
            if (orient < 0) fx = cx + dx, fy = cy + dy;
            else app_orient_to_fb(orient, fb->w, fb->h, cx + dx, cy + dy, &fx, &fy);
            box_add(out, fx, fy);
            if (colour >= 0) cv_set(fb, fx, fy, colour);
        }
    box_clip(out, fb->w, fb->h);
}

typedef struct { int brush, size; } cur_brush_t;
static int brush_in(const void *ctx, int dx, int dy)
{
    const cur_brush_t *b = ctx;
    return brush_cursor_px(b->brush, b->size, dx, dy);
}

void ui_cursor_warm(int brush, int size)
{
    cur_brush_t b = { brush, size };
    int r = brush_cursor_r(brush, size);
    cur_make(&brush_cc, r > CUR_MAXR ? CUR_MAXR : r, brush_in, &b, brush, size);
}

void ui_cursor(canvas_t *fb, int cx, int cy, int brush, int size, box_t *out)
{
    ui_cursor_warm(brush, size);
    cur_outline(fb, cx, cy, &brush_cc, -1, out);
}

/* The pointer of the menus: the classic arrow, tip at the point, scaled to 28 x 48 */
static int arrow_poly(int x, int y)
{
    static const int px[7] = { 0, 0, 9, 16, 22, 16, 27 }, py[7] = { 0, 40, 31, 47, 44, 29, 29 };
    int in = 0;
    if (x < 0 || y < 0) return 0;
    for (int i = 0, j = 6; i < 7; j = i++)
        if ((py[i] > y) != (py[j] > y) && x < (px[j] - px[i]) * (y - py[i]) / (py[j] - py[i]) + px[i]) in = !in;
    return in;
}

static int arrow_in(const void *ctx, int dx, int dy)
{
    (void)ctx;
    return arrow_poly(dx + ARROW_W / 2, dy + ARROW_H / 2);
}

void ui_arrow(canvas_t *fb, int orient, int ux, int uy, box_t *out)
{
    /* the shape spans 0..ARROW_W-1 x 0..ARROW_H-1 from the tip: drawn about its centre, the outline included */
    cur_make(&arrow_cc, ARROW_H / 2 + 1, arrow_in, 0, 0, 0);
    cur_outline(fb, ux + ARROW_W / 2, uy + ARROW_H / 2, &arrow_cc, orient, out);
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
            rc == APP_ENOMEM ? "Not enough memory for the phone page."
            : rc == APP_ENONET ? "No network could be started."
            : rc == APP_EBUSY ? "The Wi-Fi is still closing. Try again in a moment."
                              : "The Wi-Fi page could not start.");
        return;
    }
    if (!st->running) {
        put(v->note, sizeof(v->note), "The page switched itself off. Tap Back to leave.");
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
        st->locked == 1 ? "Too many wrong codes: wait a minute." : st->locked == 2 ? "Too many wrong codes: the page is off." : "Tap Back to stop. Your sketches are the .png files.");
}

int share_view_same(const share_view_t *a, const share_view_t *b) { return !memcmp(a, b, sizeof(*a)); }

void ui_share(gfx_t *g, const app_touchbar_t *bar, const share_view_t *v)
{
    (void)bar;
    gfx_fill(g, WHT);
    const font_face_t *f = face(0, 34), *big = face(1, 64), *dig = face(1, 110);
    text(g, big, 60, 130, "Download sketches", BLK);
    int y = 220;
    if (v->steps[0][0]) text(g, f, 60, y, v->steps[0], BLK), y += 50;
    if (v->steps[1][0]) text(g, f, 60, y, v->steps[1], BLK), y += 90;
    if (v->address[0]) text(g, big, 60, y, v->address, BLK), y += 150;
    if (v->code[0] && v->up) text(g, dig, 60, y, v->code, BLK), y += 100;
    text(g, f, 60, y, v->note, BLK);
    ui_back_corner(g);
}
