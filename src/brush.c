/* SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 */
#include "brush.h"
#include "fmath.h"
#include "mirror.h"
#include <string.h>
#include "penmap.h"
#include "sk_cfg.h"

static const char *const names[BR_N] = { "Pen", "Brush", "Pencil", "Marker", "Spray", "Eraser", "Fill" };

const char *brush_name(int b) { return names[b < 0 || b >= BR_N ? 0 : b]; }
const char *shade_name(int sh) { return sh == SH_BLACK ? "Black" : sh == SH_WHITE ? "White" : "Grey"; }
int brush_next(int b) { return (b + 1) % BR_N; }
int brush_is_eraser(int b) { return b == BR_ERASER; }
int brush_is_fill(int b) { return b == BR_FILL; }
int brush_tail(int b) { return b == BR_PEN; }

/* Small, Medium, Big per brush (pen size = the diameter in px; the eraser's is its side). Fill has no size: it shares the pen's row so the buttons keep their pick */
static const uint8_t sizes[BR_N][BR_SIZES] = { { 5, 9, 17 }, { 12, 22, 40 }, { 5, 9, 17 }, { 6, 10, 18 }, { 8, 14, 26 }, { 14, 27, 44 }, { 5, 9, 17 } };
int brush_size(int b, int cls) { return sizes[b < 0 || b >= BR_N ? 0 : b][cls < 0 ? 0 : cls >= BR_SIZES ? BR_SIZES - 1 : cls]; }
int brush_size_cls(int b, int size)
{
    for (int c = 0; c < BR_SIZES; c++)
        if (brush_size(b, c) == size) return c;
    return -1;
}

static int sz(int size) { return size < SK_SIZE_MIN ? SK_SIZE_MIN : size > SK_SIZE_MAX ? SK_SIZE_MAX : size; }
#define LZ(size, a, b, c, cap) pen_lerp(size, a, b, c, 1, cap) /* the brush's measure at a pen size (src/penmap.h) */

/* ---- the byte a point carries ---- */


int bt_code(int tx, int ty)
{
    int a = fm_round((float)tx / BT_STEP), b = fm_round((float)ty / BT_STEP);
    a = a < -8 ? -8 : a > 7 ? 7 : a, b = b < -8 ? -8 : b > 7 ? 7 : b;
    return (a & 15) << 4 | (b & 15);
}
void bt_decode(int code, int *tx, int *ty)
{
    int a = code >> 4 & 15, b = code & 15;
    *tx = (a > 7 ? a - 16 : a) * BT_STEP, *ty = (b > 7 ? b - 16 : b) * BT_STEP;
}

static int p8(unsigned pressure) { return pressure > 65535 ? 255 : (int)(pressure >> 8); }

int brush_param(int b, int size, unsigned pressure)
{
    size = sz(size);
    if (pressure > 65535) pressure = 65535;
    switch (b) {
    case BR_PEN: { /* swells with the pressure, linear: a light touch is a hairline */
        unsigned long long q = pressure * 5ull / 4;
        if (q > 65535) q = 65535;
        return 1 + (int)((unsigned long long)(LZ(size, 8, 15, 26, 60) - 1) * q / 65535);
    }
    case BR_BRUSH: case BR_PENCIL: case BR_SPRAY: return p8(pressure);
    case BR_MARKER: return LZ(size, 8, 14, 24, 90);
    case BR_ERASER: return pen_erase_d(size);
    default: return 1; /* the bucket draws no stroke */
    }
}

/* ---- stamps ---- */

typedef struct {
    canvas_t *c;
    int colour, shade, size;
    box_t *dirty;
    int mode, img; /* the mirror's symmetry and which image this pass draws (0: the pen's own, no map) */
    int tx, ty;    /* the pen's tilt at the stamp, degrees (bt_decode), in the page's frame */
    int batch;     /* the pen's own image goes into the batch's mask, every image's box into dirty (brush_batch) */
} bctx_t;

/* ---- the batch: a frame's mirrored stamps drawn once a pixel ----
   Stamps overlap (the Brush every sixth of its size, each point starts the next segment) and every image of every
   stamp was mapped and written again: 16 images of the kaleidoscope cost 184 ms a 30 ms frame on the X (2026-11-03).
   A write is the same whatever writes it first (cv_px_s: the pixel, the colour and the shade's page-fixed pattern),
   so the pen's own image of a frame's stamps is collected in a mask and each image writes each pixel of it once: the
   same page as stamping image by image (test_mirror: live against replay). */
#define BT_W 512 /* the mask's side in px; a stamp outside it writes what is there first and moves it */
static struct {
    canvas_t *on; /* the canvas being batched (the page), 0: none */
    int used;
    int last[6], have_last; /* the last stamp (x, y, p, tilt, brush, size): a segment starts where the one before ended */
    canvas_t *c;
    int colour, shade, mode;
    int ox, oy; /* the mask's corner in page pixels (ox a multiple of 8) */
    box_t bb;   /* the set pixels, in the mask */
    uint8_t m[BT_W / 8 * BT_W];
} bt;

static void bt_write(void);
static void bt_set(int x, int y)
{
    int mx = x - bt.ox, my = y - bt.oy;
    if (!bt.used || mx < 0 || my < 0 || mx >= BT_W || my >= BT_W) {
        bt_write();
        bt.ox = (x - BT_W / 2) & ~7, bt.oy = y - BT_W / 2, bt.used = 1;
        mx = x - bt.ox, my = y - bt.oy;
    }
    bt.m[my * (BT_W / 8) + (mx >> 3)] |= (uint8_t)(0x80 >> (mx & 7)); /* the stamp's box goes into bt.bb (add_box) */
}

/* Where the stamp's pixel (x, y) lands: the mirror image of it (every pixel is mapped, so a stamp's own shape and its
   random grain come out mirrored exactly) */
static void mp(const bctx_t *k, int x, int y, int *ox, int *oy)
{
    if (!k->img) *ox = x, *oy = y;
    else mr_map(k->mode, k->img, k->c->w, k->c->h, x, y, ox, oy);
}
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
    if (k->batch && bt.used) { /* the stamp's pixels in the mask, clipped to it */
        int a = x0 - bt.ox, b = y0 - bt.oy, c = x1 - bt.ox, d = y1 - bt.oy;
        box_add(&bt.bb, a < 0 ? 0 : a > BT_W - 1 ? BT_W - 1 : a, b < 0 ? 0 : b > BT_W - 1 ? BT_W - 1 : b);
        box_add(&bt.bb, c < 0 ? 0 : c > BT_W - 1 ? BT_W - 1 : c, d < 0 ? 0 : d > BT_W - 1 ? BT_W - 1 : d);
    }
    if (!k->dirty) return;
    int cx[4] = { x0, x1, x0, x1 }, cy[4] = { y0, y0, y1, y1 };
    bctx_t m = *k;
    int last = k->batch ? mr_images(k->mode) - 1 : k->img; /* batched: every image's box now */
    for (m.img = k->batch ? 0 : k->img; m.img <= last; m.img++)
        for (int i = 0; i < 4; i++) {
            int X, Y;
            mp(&m, cx[i], cy[i], &X, &Y);
            box_add(k->dirty, X, Y);
        }
}

static void px(const bctx_t *k, int x, int y)
{
    if (k->batch) return bt_set(x, y);
    int X, Y;
    mp(k, x, y, &X, &Y);
    cv_px_s(k->c, X, Y, k->colour, k->shade);
}

/* squares keep their own axis-aligned shape: only their centre is mapped */
static void stamp_square(const bctx_t *k, int x, int y, int p) /* marker, eraser: a square of side p */
{
    int w = p < 1 ? 1 : p, X, Y;
    mp(k, x, y, &X, &Y);
    int x0 = X - w / 2, y0 = Y - w / 2;
    for (int r = 0; r < w; r++) cv_span_s(k->c, y0 + r, x0, x0 + w - 1, k->colour, k->shade);
    if (k->dirty) box_add(k->dirty, x0, y0), box_add(k->dirty, x0 + w - 1, y0 + w - 1);
}

/* The tilt's size m in degrees (the pen's lean from upright, 0 .. about 68) */
static int tilt_m(const bctx_t *k) { return fm_round(fm_sqrt((float)(k->tx * k->tx + k->ty * k->ty))); }

static void stamp_pencil(const bctx_t *k, int x, int y, int p) /* grain: a page-fixed noise under a pressure threshold */
{
    int d = LZ(k->size, 3, 5, 8, 40), r = d / 2, thr = 60 + p * 195 / 255, m = tilt_m(k);
    if (m == 0) { /* upright: the round mark, as it always was */
        for (int dy = -r; dy <= r; dy++)
            for (int dx = -r; dx <= r; dx++)
                if (dx * dx + dy * dy <= r * r + r && (int)(hash3(x + dx, y + dy, 0) & 255) < thr) px(k, x + dx, y + dy);
        add_box(k, x - r, y - r, x + r, y + r);
        return;
    }
    /* tilted, like shading with the side of a real pencil: the mark stretches along the lean (never across it) and is lighter */
    float ux = (float)k->tx / (float)m, uy = (float)k->ty / (float)m, stretch = 1.0f + (float)m / 40.0f;
    float across2 = (float)(r * r + r), along2 = across2 * stretch * stretch;
    thr = thr * (100 - m * 3 / 4) / 100;
    int R = (int)(fm_sqrt(along2)) + 2;
    for (int dy = -R; dy <= R; dy++)
        for (int dx = -R; dx <= R; dx++) {
            float u = (float)dx * ux + (float)dy * uy, v = -(float)dx * uy + (float)dy * ux; /* along and across the lean */
            if (u * u / along2 + v * v / across2 <= 1.0f && (int)(hash3(x + dx, y + dy, 0) & 255) < thr) px(k, x + dx, y + dy);
        }
    add_box(k, x - R, y - R, x + R, y + R);
}

/* The paintbrush: a soft round mark. Pressure sets how dark it lays (a page-fixed noise under a threshold, the pencil's
   way: the same pixel is ink or not whatever stroke reaches it, so overlapping passes only add) and widens it a little;
   the threshold falls off from a solid core to nothing at the rim, so the edge thins to sparse dots. Full pressure is
   solid in the core. The shade does the rest: Grey caps at its own dots, White lays white (cv_px_s) */
/* The soft mark: a dither by a page-fixed hash under a threshold that falls from the core to the rim. The hash is
   per page pixel, so stamps that overlap put down the same pixels: the segment stamps it every STEP_BRUSH pixels
   (seg_img), not every pixel (2026-11-02: 73 us an event on a PC, about 3 ms on the X, the app behind the pen).
   No division per pixel: the rim's slope as a fixed-point reciprocal */
static void stamp_brush(const bctx_t *k, int x, int y, int p)
{
    int rb = k->size / 2, r = rb * (80 + p * 40 / 255) / 100, thr = 30 + p * 240 / 255;
    int R2 = r * r + r, C2 = R2 * 16 / 100; /* the core: 40 % of the radius is solid at full pressure */
    int slope = R2 > C2 ? (256 << 12) / (R2 - C2) : 0; /* f = (R2 - d2) * slope >> 12, 256 at the core's edge */
    for (int dy = -r; dy <= r; dy++) {
        int dy2 = dy * dy, Y = y + dy;
        for (int dx = -r; dx <= r; dx++) {
            int d2 = dx * dx + dy2;
            if (d2 > R2) continue;
            int f = d2 <= C2 ? 256 : ((R2 - d2) * slope) >> 12;
            if ((int)(hash3(x + dx, Y, 0) & 255) < (thr * f) >> 8) px(k, x + dx, Y);
        }
    }
    add_box(k, x - r, y - r, x + r, y + r);
}

static void stamp_pen(const bctx_t *k, int x, int y, int p) /* the swelling disc; a tilted pen lays a little more down */
{
    int X, Y, d = p + p * tilt_m(k) / 160;
    mp(k, x, y, &X, &Y);
    cv_disc_s(k->c, X, Y, d < 1 ? 1 : d, k->colour, k->shade, k->dirty);
}

static void stamp_spray(const bctx_t *k, int x, int y, int p) /* dots about the point, as many as the pressure says */
{
    int r = LZ(k->size, 10, 18, 30, 46), n = 2 + p * 14 / 255;
    for (int i = 0; i < n; i++) {
        uint32_t h = hash3(x, y, i + 1);
        int ox = (int)(h & 0xffff) % (2 * r + 1) - r, oy = (int)(h >> 16) % (2 * r + 1) - r;
        if (ox * ox + oy * oy <= r * r) px(k, x + ox, y + oy);
    }
    add_box(k, x - r, y - r, x + r, y + r);
}

static void stamp_none(const bctx_t *k, int x, int y, int p) { (void)k, (void)x, (void)y, (void)p; } /* the bucket draws no stroke */

static const stamp_fn stamps[BR_N] = { stamp_pen, stamp_brush, stamp_pencil, stamp_square, stamp_spray, stamp_square, stamp_none };

static void seg_img(const bstyle_t *st, int img, int colour, canvas_t *c, int x0, int y0, int p0, int t0, int x1, int y1, int p1, int t1, box_t *dirty);

/* Pixels between stamps along the longer axis: 1 for the shapes whose stamps must touch (the pen's disc, the pencil's
   grain, the spray); the soft Brush's overlapping stamps draw the same page pixels (stamp_brush), so a stamp every
   sixth of its size is the same mark; the Eraser's and Marker's squares cover a path without gaps while the step is
   under their side */
int brush_stamp_step(int b, int size)
{
    switch (b) {
    case BR_BRUSH: return size / 6 > 1 ? size / 6 : 1;
    case BR_ERASER: case BR_MARKER: return size / 4 > 1 ? size / 4 : 1;
    default: return 1;
    }
}

/* A stamp every brush_stamp_step pixels along the longer axis, the byte interpolated: no gaps whatever the speed */
static void bt_write(void)
{
    if (!bt.used || box_empty(&bt.bb)) return box_clear(&bt.bb);
    canvas_t *c = bt.c;
    int n = mr_images(bt.mode);
    static int16_t js[BT_W], px_[BT_W], py_[BT_W];
    for (int my = bt.bb.y0; my <= bt.bb.y1; my++) {
        const uint8_t *row = bt.m + my * (BT_W / 8);
        int b0 = bt.bb.x0 >> 3, b1 = bt.bb.x1 >> 3, y = bt.oy + my, k = 0;
        while (b0 <= b1 && !row[b0]) b0++;
        while (b1 >= b0 && !row[b1]) b1--;
        if (b0 > b1) continue;
        int x0 = bt.ox + b0 * 8;
        for (int b = b0; b <= b1; b++) /* the row's set pixels, once for every image */
            for (int i = 0, v = row[b]; v; i++, v = (v << 1) & 0xff)
                if (v & 0x80) js[k++] = (int16_t)((b - b0) * 8 + i);
        for (int img = 0; img < n; img++) {
            int fx, fy;
            if (mr_flips(bt.mode, img, &fx, &fy) && !(c->w & 7)) { /* a row stays a row: its bytes (reversed) */
                int Y = fy ? c->h - 1 - y : y, xb = fx ? (c->w - 1 - x0) >> 3 : x0 >> 3;
                cv_bits_row(c, Y, xb, row + b0, b1 - b0 + 1, fx, bt.colour, bt.shade);
                continue;
            }
            mr_map_pts(bt.mode, img, c->w, c->h, x0, y, js, k, px_, py_);
            cv_px_list(c, px_, py_, k, bt.colour, bt.shade);
        }
    }
    for (int my = bt.bb.y0; my <= bt.bb.y1; my++) memset(bt.m + my * (BT_W / 8) + (bt.bb.x0 >> 3), 0, (size_t)((bt.bb.x1 >> 3) - (bt.bb.x0 >> 3) + 1));
    box_clear(&bt.bb);
}

void brush_flush(void)
{
    bt_write();
    bt.used = 0, bt.have_last = 0;
}

void brush_batch(canvas_t *c)
{
    brush_flush();
    bt.on = c;
}

/* The brushes whose stamps write mapped pixels one by one (the pen's disc and the squares map their centre only) */
static int by_pixel(int b) { return b == BR_BRUSH || b == BR_PENCIL || b == BR_SPRAY; }

void brush_segt(const bstyle_t *st, int colour, canvas_t *c, int x0, int y0, int p0, int t0, int x1, int y1, int p1, int t1, box_t *dirty)
{
    int imgs = mr_images(st->mirror);
    if (bt.on == c && c && imgs > 1 && by_pixel(st->brush < BR_N ? st->brush : 0)) {
        int col = st->shade == SH_WHITE ? 1 : colour, sh = st->shade < SH_N ? st->shade : 0;
        if (bt.used && (bt.c != c || bt.colour != col || bt.shade != sh || bt.mode != st->mirror)) brush_flush();
        if (!bt.used) box_clear(&bt.bb);
        bt.c = c, bt.colour = col, bt.shade = sh, bt.mode = st->mirror;
        return seg_img(st, -1, colour, c, x0, y0, p0, t0, x1, y1, p1, t1, dirty); /* img -1: into the batch */
    }
    for (int img = 0; img < imgs; img++) seg_img(st, img, colour, c, x0, y0, p0, t0, x1, y1, p1, t1, dirty);
}
void brush_seg(const bstyle_t *st, int colour, canvas_t *c, int x0, int y0, int p0, int x1, int y1, int p1, box_t *dirty)
{
    brush_segt(st, colour, c, x0, y0, p0, 0, x1, y1, p1, 0, dirty);
}

static void seg_img(const bstyle_t *st, int img, int colour, canvas_t *c, int x0, int y0, int p0, int t0, int x1, int y1, int p1, int t1, box_t *dirty)
{
    int b = st->brush < BR_N ? st->brush : 0;
    if (st->shade == SH_WHITE) colour = 1; /* white ink: the brush's own shape in solid white */
    bctx_t k = { c, colour, st->shade < SH_N ? st->shade : 0, sz(st->size), dirty, st->mirror, img < 0 ? 0 : img, 0, 0, img < 0 };
    stamp_fn fn = stamps[b];
    int dx = x1 - x0, dy = y1 - y0, adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy, n = adx > ady ? adx : ady;
    int ax, ay, bx, by; /* the tilt at each end; the stamps read it from k */
    bt_decode(t0, &ax, &ay);
    bt_decode(t1, &bx, &by);
    if (n == 0) return k.tx = bx, k.ty = by, fn(&k, x0, y0, p1);
    const int step = brush_stamp_step(b, k.size) * (b == BR_BRUSH && st->scale > 1 ? st->scale : 1);
    for (int i = 0; i <= n; i += i + step > n && i < n ? n - i : step) { /* the end point always gets its stamp */
        int x = x0 + (dx * i + (dx < 0 ? -n / 2 : n / 2)) / n;
        int y = y0 + (dy * i + (dy < 0 ? -n / 2 : n / 2)) / n;
        int q = p0 + ((p1 - p0) * i + ((p1 - p0) < 0 ? -n / 2 : n / 2)) / n;
        k.tx = ax + ((bx - ax) * i + ((bx - ax) < 0 ? -n / 2 : n / 2)) / n;
        k.ty = ay + ((by - ay) * i + ((by - ay) < 0 ? -n / 2 : n / 2)) / n;
        if (k.batch) { /* the same stamp again (the point that ended the segment before) adds no pixel: skipped */
            int me[6] = { x, y, q, k.tx * 256 + k.ty, b, k.size };
            if (bt.have_last && !memcmp(me, bt.last, sizeof(me))) continue;
            memcpy(bt.last, me, sizeof(me)), bt.have_last = 1;
        }
        fn(&k, x, y, q);
    }
}

/* ---- cursors ---- */


int brush_shape_r(int b, int size)
{
    size = sz(size);
    switch (b) {
    case BR_PEN: return LZ(size, 3, 5, 8, 15) * 3 + 2;
    case BR_BRUSH: return size * 6 / 10 + 2;
    case BR_PENCIL: return LZ(size, 14, 20, 28, 58) * 3 / 4 + 3;
    case BR_MARKER: return LZ(size, 8, 14, 24, 90) / 2 + 2;
    case BR_SPRAY: return LZ(size, 10, 18, 30, 46);
    case BR_ERASER: return pen_erase_d(size) / 2 + 1;
    case BR_FILL: return 40;
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

/* A bucket upright to the upper right of the hot spot (the pour point: a drop at the lower left), its handle an arch over the rim */
static int in_bucket(int dx, int dy)
{
    int bx = dx - 20, by = dy + 30; /* the body's rim centre */
    if (by >= 0 && by <= 22 && bx >= -(15 - by * 5 / 22) && bx <= 15 - by * 5 / 22) return 1; /* body, wider at the top */
    if (by < 0) { /* the handle: the band between two half ellipses */
        int o = bx * bx * 100 + by * by * 256, i = bx * bx * 64 + by * by * 196;
        return o <= 25600 && i >= 12544;
    }
    return dx * dx + (dy + 1) * (dy + 1) <= 9; /* the drop */
}

int brush_shape_px(int b, int size, int dx, int dy)
{
    size = sz(size);
    int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy, m = ax > ay ? ax : ay;
    switch (b) {
    case BR_PEN: {
        int rr = LZ(size, 3, 5, 8, 15);
        return in_drop(rr, dx, dy) && !(in_drop(rr, dx - 1, dy) && in_drop(rr, dx + 1, dy) && in_drop(rr, dx, dy - 1) && in_drop(rr, dx, dy + 1));
    }
    case BR_BRUSH: { /* a ring of the soft mark's size */
        int r = size / 2, d2 = dx * dx + dy * dy;
        return d2 <= r * r + r && d2 >= (r - 1) * (r - 1);
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
    case BR_SPRAY: { /* a dotted ring */
        int r = LZ(size, 10, 18, 30, 46), d2 = dx * dx + dy * dy;
        if (dx == 0 && dy == 0) return 1;
        return d2 <= r * r && d2 >= (r - 1) * (r - 1) && (((dx + 64) / 3 + (dy + 64) / 3) & 1) == 0;
    }
    case BR_ERASER: {
        int h = pen_erase_d(size) / 2;
        return m == h;
    }
    case BR_FILL: return in_bucket(dx, dy) && !(in_bucket(dx - 1, dy) && in_bucket(dx + 1, dy) && in_bucket(dx, dy - 1) && in_bucket(dx, dy + 1));
    default: return dx == 0 && dy == 0;
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
