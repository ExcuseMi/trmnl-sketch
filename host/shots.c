/* make shots: the app's screens to PNG (shots/): strokes of three sizes with pressure, the legend, the eraser and its
   cursor, a portrait position, the menu and the Wi-Fi screen. SPDX-License-Identifier: MIT */
#include <math.h>
#include <stdio.h>
#include "util.h"

/* the old calls name three pens by index: thin 5 px, medium 9, thick 17 (the slider now takes any size 1..48) */
#define SZ(i) ((i) == 0 ? 5 : (i) == 1 ? 9 : 17)
#define sk_set_size(s, n, t) (sk_set_size)((s), SZ(n), (t))

int png_write(const char *path, const gfx_t *g);
void fill_shots(const char *dir, int *bad);

static int bad;
#define TAP(z) sk_touch(&r.s, APP_TOUCH_##z, APP_GESTURE_TAP, r.now += 50)
static void brush_shots(const char *dir);
static void paging_shots(const char *dir);

static void save(rig_t *r, const char *dir, const char *name)
{
    gfx_t g;
    gfx_init(&g, r->fb, TW, TH, TS, r->s.orient);
    char path[256];
    snprintf(path, sizeof(path), "%s/%s.png", dir, name);
    if (png_write(path, &g)) fprintf(stderr, "cannot write %s\n", path), bad = 1;
    printf("%s\n", path);
}

/* A wave of n points along x with the pressure rising and falling, as a hand writes it */
static void wave(rig_t *r, int x0, int x1, int y, int amp, unsigned flags)
{
    int n = (x1 - x0) / 7;
    for (int i = 0; i <= n; i++) {
        float t = (float)i / (float)n;
        int x = x0 + (int)(t * (float)(x1 - x0)), yy = y + (int)(sinf(t * 12.0f) * (float)amp);
        unsigned p = (unsigned)(4000.0f + 58000.0f * sinf(t * 3.14159f));
        rig_pen(r, x, yy, APP_PEN_TIP | APP_PEN_IN_RANGE | flags, p);
    }
    rig_pen(r, x1, y, APP_PEN_IN_RANGE | flags, 0);
}

static void ring(rig_t *r, int cx, int cy, int rad, unsigned flags)
{
    for (int i = 0; i <= 90; i++) {
        float a = (float)i / 90.0f * 6.2832f;
        unsigned p = (unsigned)(10000.0f + 40000.0f * (0.5f + 0.5f * cosf(a)));
        rig_pen(r, cx + (int)(cosf(a) * (float)rad), cy + (int)(sinf(a) * (float)rad), APP_PEN_TIP | APP_PEN_IN_RANGE | flags, p);
    }
    rig_pen(r, cx, cy, APP_PEN_IN_RANGE | flags, 0);
}

static void draw_page(rig_t *r)
{
    for (int s = 0; s < 3; s++) {
        wave(r, 200, 1650, 300 + s * 280, 70, 0);
        sk_set_size(&r->s, (s + 1) % 3, r->now += 50);
    }
    ring(r, 450, 1150, 150, 0);
    ring(r, 900, 1150, 120, 0);
    wave(r, 1150, 1700, 1150, 90, 0);
}

static void gshot(rig_t *r, const char *dir, const char *name)
{
    rect_t out[SK_MAX_RECTS];
    rig_frame(r, out);
    save(r, dir, name);
}

/* eight drawings, then the gallery, the action list and the print screens, in two positions */
static void gallery_shots(const char *dir)
{
    static rig_t r;
    for (int o = 0; o < 2; o++) { /* the bar at the bottom, then on the left (portrait) */
        rig_new(&r, o);
        for (int i = 0; i < 8; i++) {
            if (i & 1) wave(&r, 150, 1700, 250 + (i * 90) % 800, 40 + i * 12, 0);
            else ring(&r, 400 + (i * 170) % 900, 500 + (i * 110) % 500, 120 + i * 15, 0);
            if (i % 3 == 0) wave(&r, 300, 1500, 1100, 60, 0);
            r.now += 3000;
            sk_tick(&r.s, r.now);
            if (i < 7) sk_pen_key(&r.s, 1u << 5, 1u << 5, r.now += 50), sk_pen_key(&r.s, 0, 1u << 5, r.now);
        }
        static const uint8_t junk[64] = { 1, 2, 3 };
        memfs_put(&r.fs, "sketch/sk0003.png", junk, sizeof(junk)); /* a page file that does not decode */
        rig_pen(&r, 10, 10, 0, 0);
        TAP(MIDDLE); /* the page menu of the page on screen */
        gshot(&r, dir, o ? "m-pagemenu-portrait" : "m-pagemenu-landscape");
        if (!o) {
            TAP(MIDDLE); /* Print */
            gshot(&r, dir, "f-print-confirm");
            TAP(MIDDLE);
            gshot(&r, dir, "g-printing");
            r.pr.status = 200;
            sk_run_print(&r.s);
            gshot(&r, dir, "h-printed");
            TAP(MIDDLE); /* OK */
            r.pr_rc = PJ_NO_PRINTER;
            TAP(MIDDLE);
            gshot(&r, dir, "i-no-printer");
            r.pr_rc = 0;
            TAP(MIDDLE);
        }
        for (int k = 0; k < 4; k++) TAP(RIGHT);
        TAP(MIDDLE); /* Gallery */
        for (int k = 0; k < 5; k++) TAP(RIGHT); /* the damaged tile */
        gshot(&r, dir, o ? "c-gallery-portrait" : "b-gallery-landscape");
        { /* the pen hovers over a tile: its frame is thick and the arrow stands on it */
            int tx, ty;
            gal_tile_xy(&r.s.L, 2, &tx, &ty);
            rig_pen_user(&r, tx + r.s.L.tw / 2 + 40, ty + r.s.L.th / 2 - 20, APP_PEN_IN_RANGE, 0);
            r.now += 200;
            gshot(&r, dir, o ? "cp-gallery-portrait-pointer" : "bp-gallery-landscape-pointer");
        }
        rig_pen(&r, 10, 10, 0, 0);
        TAP(LEFT);
        TAP(RIGHT); /* back on the damaged tile: its list */
        TAP(MIDDLE);
        gshot(&r, dir, o ? "e-damaged-portrait" : "d-damaged-landscape");
        rig_end(&r);
    }
}

/* twenty drawings: the gallery's second screen (paging arrows, page indicator, button hint), landscape and portrait */
static void paging_shots(const char *dir)
{
    static rig_t r;
    for (int o = 0; o < 2; o++) {
        rig_new(&r, o);
        for (int i = 0; i < 20; i++) {
            if (i & 1) wave(&r, 150, 1700, 250 + (i * 90) % 800, 40 + i * 6, 0);
            else ring(&r, 400 + (i * 170) % 900, 500 + (i * 110) % 500, 100 + i * 5, 0);
            r.now += 3000;
            sk_tick(&r.s, r.now);
            if (i < 19) sk_pen_key(&r.s, 1u << 5, 1u << 5, r.now += 50), sk_pen_key(&r.s, 0, 1u << 5, r.now);
        }
        rig_pen(&r, 10, 10, 0, 0);
        sk_touch(&r.s, APP_TOUCH_MIDDLE, APP_GESTURE_TAP, r.now += 50);
        for (int k = 0; k < 4; k++) sk_touch(&r.s, APP_TOUCH_RIGHT, APP_GESTURE_TAP, r.now += 50);
        sk_touch(&r.s, APP_TOUCH_MIDDLE, APP_GESTURE_TAP, r.now += 50); /* Gallery */
        sk_pen_key(&r.s, 1u << 7, 1u << 7, r.now += 50); /* button 8: next screen of tiles */
        sk_pen_key(&r.s, 0, 1u << 7, r.now);
        int x0, y0, x1, y1;
        ui_nav_box(&r.s.L, CTL_PREV, &x0, &y0, &x1, &y1);
        rig_pen_user(&r, (x0 + x1) / 2, (y0 + y1) / 2, APP_PEN_IN_RANGE, 0); /* the pen over "Previous" */
        r.now += 200;
        gshot(&r, dir, o ? "p2-gallery-screen2-portrait" : "p1-gallery-screen2-landscape");
        rig_end(&r);
    }
}

/* One stroke per brush, the toolbar's order from the top; the names are panels drawn on the screen only */
static void brush_page(rig_t *r, const char *dir, const char *name, int shade)
{
    sk_set_shade(&r->s, shade, r->now += 50);
    sk_set_size(&r->s, 1, r->now += 50);
    for (int b = 0; b < BR_N; b++) {
        int y = 140 + b * 190;
        if (b == BR_ERASER) { /* the eraser works on dense scribbles of the marker */
            sk_set_brush(&r->s, BR_MARKER, r->now += 50);
            sk_set_size(&r->s, 2, r->now += 50);
            for (int k = -1; k <= 1; k++) wave(r, 330, 1700, y + k * 34, 0, 0);
            sk_set_size(&r->s, 1, r->now += 50);
        }
        sk_set_brush(&r->s, b, r->now += 50);
        wave(r, 330, 1700, y, b == BR_ERASER ? 25 : 55, 0);
        r->now += 3000;
        sk_tick(&r->s, r->now);
    }
    rig_pen(r, 10, 10, 0, 0);
    r->now += 3000;
    sk_tick(&r->s, r->now);
    for (int b = 0; b < BR_N; b++) { /* names over the left edge, on the screen only */
        gfx_t g;
        box_t bx;
        gfx_init(&g, r->fb, TW, TH, TS, r->s.orient);
        ui_toast(&g, brush_name(b), 140 + b * 190 - 38, &bx);
    }
    save(r, dir, name);
}

static void brush_shots(const char *dir)
{
    static rig_t r;
    const char *const names[SH_N] = { "b1-brushes-black", "b2-brushes-dark", "b3-brushes-light" };
    for (int sh = 0; sh < SH_N; sh++) {
        rig_new(&r, APP_ORIENT_BAR_BOTTOM);
        brush_page(&r, dir, names[sh], sh);
        if (sh == 0) { /* the toolbar over this drawing (landscape), then the same page with it hidden again */
            sk_set_brush(&r.s, BR_BRUSHPEN, r.now += 50);
            sk_pen_key(&r.s, 1u << 3, 1u << 3, r.now += 50);
            sk_pen_key(&r.s, 0, 1u << 3, r.now);
            r.now += 3000;
            sk_tick(&r.s, r.now);
            save(&r, dir, "t1-toolbar-landscape");
            rig_pen_user(&r, r.s.tb.cx[TI_SHADE + 1] + r.s.tb.cell / 2, r.s.tb.cy + r.s.tb.cell / 2, APP_PEN_IN_RANGE, 0); /* the pointer over an icon */
            r.now += 200;
            rect_t o[SK_MAX_RECTS];
            rig_frame(&r, o);
            save(&r, dir, "t3-toolbar-pointer");
            rig_pen_user(&r, tb_slider_x(&r.s.tb, 30), r.s.tb.sl_y, APP_PEN_TIP | APP_PEN_IN_RANGE, 30000); /* the knob picked up: size 30 and the footprint */
            r.now += 200;
            rig_frame(&r, o);
            save(&r, dir, "t4-slider-dragging");
            rig_pen_user(&r, tb_slider_x(&r.s.tb, 30), r.s.tb.sl_y, APP_PEN_IN_RANGE, 0);
        }
        rig_end(&r);
    }
    /* the cursor of every brush at the three sizes, over ink and paper */
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    sk_set_brush(&r.s, BR_MARKER, r.now += 50);
    sk_set_size(&r.s, 2, r.now += 50);
    for (int j = 0; j < 3; j++) wave(&r, 100, 1770, 380 + j * 380, 0, 0);
    rig_pen(&r, 10, 10, 0, 0);
    r.now += 3000;
    sk_tick(&r.s, r.now);
    for (int b = 0; b < BR_N; b++)
        for (int j = 0; j < 3; j++) {
            box_t bx; /* each over paper (above the bar) and over the black bar */
            ui_cursor(&r.s.fb, 120 + b * 255, 380 + j * 380 - 110, b, j, &bx);
            ui_cursor(&r.s.fb, 120 + b * 255 + 120, 380 + j * 380, b, j, &bx);
        }
    save(&r, dir, "c1-cursors-each-brush-three-sizes");
    rig_end(&r);
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "shots";
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    save(&r, dir, "1-legend-at-start");
    r.fs.fail_write = 0;
    sk_set_size(&r.s, 0, r.now += 50); /* the loop below starts thin */
    draw_page(&r);
    r.now += 3000;
    sk_tick(&r.s, r.now);
    rig_pen(&r, 10, 10, 0, 0);
    save(&r, dir, "2-three-sizes-with-pressure");
    rig_pen(&r, 700, 800, APP_PEN_IN_RANGE, 0);
    save(&r, dir, "3-hover-cursor");
    sk_set_size(&r.s, 1, r.now += 50);
    save(&r, dir, "4-toast-pen-size");
    r.now += 3000;
    sk_tick(&r.s, r.now);
    /* the eraser: the pen's button held, across the waves and one ring (medium size: the eraser is wider) */
    sk_set_size(&r.s, 2, r.now += 50);
    r.now += 3000;
    sk_tick(&r.s, r.now);
    for (int i = 0; i <= 40; i++) rig_pen(&r, 600 + i * 12, 220 + i * 10 + (i % 2) * 6, APP_PEN_TIP | APP_PEN_IN_RANGE | APP_PEN_BARREL, 30000);
    rig_pen(&r, 1080, 640, APP_PEN_IN_RANGE | APP_PEN_BARREL, 0);
    save(&r, dir, "5-eraser-and-its-cursor");
    sk_pen_key(&r.s, 1u << 0, 1u << 0, r.now += 50); /* undo the erasing */
    sk_pen_key(&r.s, 0, 1u << 0, r.now);
    rig_pen(&r, 10, 10, 0, 0);
    r.now += 3000;
    sk_tick(&r.s, r.now);
    save(&r, dir, "6-after-undo");
    sk_touch(&r.s, APP_TOUCH_MIDDLE, APP_GESTURE_TAP, r.now += 50); /* the page menu, Send to phone */
    sk_touch(&r.s, APP_TOUCH_RIGHT, APP_GESTURE_TAP, r.now += 50);
    sk_touch(&r.s, APP_TOUCH_MIDDLE, APP_GESTURE_TAP, r.now += 50);
    app_webshare_status_t st = { .running = 1, .mode = APP_WEBSHARE_AP };
    strcpy(st.code, "482915"), strcpy(st.network, "Writer"), strcpy(st.address, "http://4.3.2.1"), strcpy(st.ip, "4.3.2.1");
    sk_share_update(&r.s, 0, &st);
    save(&r, dir, "8-wifi-page");
    rig_end(&r);
    gallery_shots(dir);
    paging_shots(dir);

    /* portrait: the touch bar on the left, the legend and a page of strokes turned with it */
    rig_new(&r, APP_ORIENT_BAR_LEFT);
    save(&r, dir, "9-portrait-legend");
    {
        penmap_t *m = &r.s.map;
        for (int s = 0; s < 3; s++) {
            for (int i = 0; i <= 120; i++) {
                float t = (float)i / 120.0f;
                unsigned x = (unsigned)(18000.0f + 30000.0f * t), y = (unsigned)(12000.0f + (float)s * 18000.0f + sinf(t * 10.0f) * 4000.0f);
                r.now += 5;
                sk_pen(&r.s, APP_PEN_TIP | APP_PEN_IN_RANGE, x, y, (unsigned)(4000.0f + 58000.0f * sinf(t * 3.14159f)), r.now);
            }
            sk_pen(&r.s, APP_PEN_IN_RANGE, 48000, 12000, 0, r.now += 5);
            sk_set_size(&r.s, (s + 2) % 3, r.now += 50);
        }
        (void)m;
    }
    r.now += 3000;
    sk_tick(&r.s, r.now);
    rig_pen(&r, 10, 10, 0, 0);
    save(&r, dir, "a-portrait-strokes");
    sk_pen_key(&r.s, 1u << 3, 1u << 3, r.now += 50); /* tablet button 4: the toolbar, in portrait */
    sk_pen_key(&r.s, 0, 1u << 3, r.now);
    sk_set_brush(&r.s, BR_PENCIL, r.now += 50);
    save(&r, dir, "t2-toolbar-portrait");
    rig_pen_user(&r, tb_slider_x(&r.s.tb, 24), r.s.tb.sl_y, APP_PEN_TIP | APP_PEN_IN_RANGE, 30000);
    r.now += 200;
    save(&r, dir, "t5-slider-portrait");
    rig_pen_user(&r, tb_slider_x(&r.s.tb, 24), r.s.tb.sl_y, APP_PEN_IN_RANGE, 0);
    rig_end(&r);
    brush_shots(dir);
    fill_shots(dir, &bad);
    return bad;
}
