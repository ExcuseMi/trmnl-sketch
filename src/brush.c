/* SPDX-License-Identifier: MIT */
#include "brush.h"
#include "fmath.h"
#include "penmap.h"
#include "sk_cfg.h"

static const char *const names[BR_N] = { "Fineliner", "Brush pen", "Pencil", "Marker", "Calligraphy", "Spray", "Eraser" };
static const char *const shades[SH_N] = { "Black", "Dark gray", "Light gray" };

const char *brush_name(int b) { return names[b < 0 || b >= BR_N ? 0 : b]; }
const char *shade_name(int sh) { return shades[sh < 0 || sh >= SH_N ? 0 : sh]; }
int brush_next(int b) { return (b + 1) % BR_N; }
int brush_is_eraser(int b) { return b == BR_ERASER; }
int brush_tail(int b) { return b == BR_BRUSHPEN; }

static int sz(int size) { return size < SK_SIZE_MIN ? SK_SIZE_MIN : size > SK_SIZE_MAX ? SK_SIZE_MAX : size; }
#define LZ(size, a, b, c, cap) pen_lerp(size, a, b, c, 1, cap) /* the brush's measure at a pen size (src/penmap.h) */

/* ---- the byte a point carries ---- */


static int p8(unsigned pressure) { return pressure > 65535 ? 255 : (int)(pressure >> 8); }

int brush_param(int b, int size, unsigned pressure)
{
    size = sz(size);
    if (pressure > 65535) pressure = 65535;
    switch (b) {
    case BR_BRUSHPEN: { /* swells with the pressure, linear: a light touch is a hairline */
        unsigned long long q = pressure * 5ull / 4;
        if (q > 65535) q = 65535;
        return 1 + (int)((unsigned long long)(LZ(size, 8, 15, 26, 60) - 1) * q / 65535);
    }
    case BR_PENCIL: case BR_SPRAY: return p8(pressure);
    case BR_MARKER: return LZ(size, 8, 14, 24, 90);
    case BR_CALLIGRAPHY: return LZ(size, 12, 20, 32, 124) * 3 / 4 + (int)((unsigned long long)(LZ(size, 12, 20, 32, 124) / 4) * pressure / 65535);
    case BR_ERASER: return pen_erase_d(size);
    default: return pen_diam(size, pressure);
    }
}

/* ---- stamps ---- */

typedef struct {
    canvas_t *c;
    int colour, shade, size;
    box_t *dirty;
} bctx_t;
typedef void (*stamp_fn)(const bctx_t *k, int x, int y, int p);

static uint32_t hash3(int x, int y, int k)
{
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + (uint32_t)k * 2246822519u;
    h ^= h >> 13;
    h *= 1274126177u;
    return h ^ (h >> 16);
}

static void add_box(const bctx_t *k, int x0, int y0, int x1, int y1)
{
    if (!k->dirty) return;
    box_add(k->dirty, x0, y0);
    box_add(k->dirty, x1, y1);
}

static void stamp_disc(const bctx_t *k, int x, int y, int p) { cv_disc_s(k->c, x, y, p < 1 ? 1 : p, k->colour, k->shade, k->dirty); }

static void stamp_square(const bctx_t *k, int x, int y, int p) /* marker, eraser: a square of side p */
{
    int w = p < 1 ? 1 : p, x0 = x - w / 2, y0 = y - w / 2;
    for (int r = 0; r < w; r++) cv_span_s(k->c, y0 + r, x0, x0 + w - 1, k->colour, k->shade);
    add_box(k, x0, y0, x0 + w - 1, y0 + w - 1);
}

static void stamp_pencil(const bctx_t *k, int x, int y, int p) /* grain: a page-fixed noise under a pressure threshold */
{
    int d = LZ(k->size, 3, 5, 8, 40), r = d / 2, thr = 60 + p * 195 / 255;
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++)
            if (dx * dx + dy * dy <= r * r + r && (int)(hash3(x + dx, y + dy, 0) & 255) < thr) cv_px_s(k->c, x + dx, y + dy, k->colour, k->shade);
    add_box(k, x - r, y - r, x + r, y + r);
}

static void stamp_nib(const bctx_t *k, int x, int y, int p) /* a flat nib slanted 45 degrees: a thin line along "/" */
{
    int e = p * 181 / 512;
    for (int t = -e; t <= e; t++) {
        cv_px_s(k->c, x + t, y - t, k->colour, k->shade);
        cv_px_s(k->c, x + t + 1, y - t, k->colour, k->shade);
    }
    add_box(k, x - e, y - e, x + e + 1, y + e);
}

static void stamp_spray(const bctx_t *k, int x, int y, int p) /* dots about the point, as many as the pressure says */
{
    int r = LZ(k->size, 10, 18, 30, 46), n = 2 + p * 14 / 255;
    for (int i = 0; i < n; i++) {
        uint32_t h = hash3(x, y, i + 1);
        int ox = (int)(h & 0xffff) % (2 * r + 1) - r, oy = (int)(h >> 16) % (2 * r + 1) - r;
        if (ox * ox + oy * oy <= r * r) cv_px_s(k->c, x + ox, y + oy, k->colour, k->shade);
    }
    add_box(k, x - r, y - r, x + r, y + r);
}

static const stamp_fn stamps[BR_N] = { stamp_disc, stamp_disc, stamp_pencil, stamp_square, stamp_nib, stamp_spray, stamp_square };

/* A stamp every pixel along the longer axis, the byte interpolated: no gaps whatever the speed */
void brush_seg(const bstyle_t *st, int colour, canvas_t *c, int x0, int y0, int p0, int x1, int y1, int p1, box_t *dirty)
{
    int b = st->brush < BR_N ? st->brush : 0;
    bctx_t k = { c, colour, st->shade < SH_N ? st->shade : 0, sz(st->size), dirty };
    stamp_fn fn = stamps[b];
    int dx = x1 - x0, dy = y1 - y0, adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy, n = adx > ady ? adx : ady;
    if (n == 0) return fn(&k, x0, y0, p1);
    for (int i = 0; i <= n; i++) {
        int x = x0 + (dx * i + (dx < 0 ? -n / 2 : n / 2)) / n;
        int y = y0 + (dy * i + (dy < 0 ? -n / 2 : n / 2)) / n;
        int q = p0 + ((p1 - p0) * i + ((p1 - p0) < 0 ? -n / 2 : n / 2)) / n;
        fn(&k, x, y, q);
    }
}

/* ---- cursors ---- */


int brush_shape_r(int b, int size)
{
    size = sz(size);
    switch (b) {
    case BR_BRUSHPEN: return LZ(size, 3, 5, 8, 15) * 3 + 2;
    case BR_PENCIL: return LZ(size, 14, 20, 28, 58) * 3 / 4 + 3;
    case BR_MARKER: return LZ(size, 8, 14, 24, 90) / 2 + 2;
    case BR_CALLIGRAPHY: return LZ(size, 12, 20, 32, 124) * 181 / 512 + 3;
    case BR_SPRAY: return LZ(size, 10, 18, 30, 46);
    case BR_ERASER: return pen_erase_d(size) / 2 + 1;
    default: return LZ(size, 6, 8, 12, 46);
    }
}

static int in_drop(int rr, int dx, int dy) /* a disc with a tail tapering to the upper right */
{
    if (dx * dx + dy * dy <= rr * rr) return 1;
    float a = (float)(dx - dy) * 0.70711f, bb = fm_abs((float)(dx + dy) * 0.70711f), len = (float)rr * 2.6f;
    return a >= 0 && a <= len && bb <= (float)rr * (1 - a / len);
}
static int in_pencil(int l, int dx, int dy) /* a cone, its tip at the hot spot, the body to the upper right */
{
    float a = (float)(dx - dy) * 0.70711f, bb = fm_abs((float)(dx + dy) * 0.70711f);
    return a >= 0 && a <= (float)l && bb <= a * 0.30f + 0.6f;
}

int brush_shape_px(int b, int size, int dx, int dy)
{
    size = sz(size);
    int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy, m = ax > ay ? ax : ay;
    switch (b) {
    case BR_BRUSHPEN: {
        int rr = LZ(size, 3, 5, 8, 15);
        return in_drop(rr, dx, dy) && !(in_drop(rr, dx - 1, dy) && in_drop(rr, dx + 1, dy) && in_drop(rr, dx, dy - 1) && in_drop(rr, dx, dy + 1));
    }
    case BR_PENCIL: {
        int l = LZ(size, 14, 20, 28, 58);
        if (!in_pencil(l, dx, dy)) return 0;
        if (!(in_pencil(l, dx - 1, dy) && in_pencil(l, dx + 1, dy) && in_pencil(l, dx, dy - 1) && in_pencil(l, dx, dy + 1))) return 1;
        return (hash3(dx, dy, 7) & 7) == 0; /* the grain inside */
    }
    case BR_MARKER: {
        int h = LZ(size, 8, 14, 24, 90) / 2;
        return (m <= h && m >= h - 1) || (dx == 0 && dy == 0);
    }
    case BR_CALLIGRAPHY: {
        int e = LZ(size, 12, 20, 32, 124) * 181 / 512;
        return dx + dy >= 0 && dx + dy <= 1 && ax <= e + 1;
    }
    case BR_SPRAY: { /* a dotted ring */
        int r = LZ(size, 10, 18, 30, 46), d2 = dx * dx + dy * dy;
        if (dx == 0 && dy == 0) return 1;
        return d2 <= r * r && d2 >= (r - 1) * (r - 1) && (((dx + 64) / 3 + (dy + 64) / 3) & 1) == 0;
    }
    case BR_ERASER: {
        int h = pen_erase_d(size) / 2;
        return m == h;
    }
    default: {
        int r = LZ(size, 6, 8, 12, 46), d2 = dx * dx + dy * dy;
        return (d2 <= r * r && d2 >= (r - 1) * (r - 1)) || (dx == 0 && dy == 0);
    }
    }
}

/* The cursor the owner sees: the footprint's shape drawn 3 px thick, and for small ones a cross out to 16 px so the
   cursor is never smaller than 32 px across */
#define CUR_MIN 16
int brush_cursor_r(int b, int size)
{
    int r = brush_shape_r(b, size) + 1;
    return r < CUR_MIN ? CUR_MIN : r;
}
int brush_cursor_px(int b, int size, int dx, int dy)
{
    int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy, s = brush_shape_r(b, size);
    if (s + 1 < CUR_MIN && ((ax <= 1 && ay <= CUR_MIN) || (ay <= 1 && ax <= CUR_MIN))) return 1;
    if (ax > s + 1 || ay > s + 1) return 0;
    for (int y = dy - 1; y <= dy + 1; y++)
        for (int x = dx - 1; x <= dx + 1; x++)
            if (brush_shape_px(b, size, x, y)) return 1;
    return 0;
}
