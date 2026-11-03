/* SPDX-License-Identifier: MIT */
#include "tbicons.h"
#include "brush.h"
#include "fmath.h"
#include "mirror.h"
#include "sk_cfg.h"
#include <string.h>

/* A tool lies on the 45 degree diagonal, tip at the lower left: a point is (u along the tool from the tip, v across) */
typedef struct {
    gfx_t *g;
    float tx, ty;
    int ink, bg;
} tbi_tool_t;

static void tbi_tp(const tbi_tool_t *t, float u, float v, float *x, float *y)
{
    const float k = 0.70711f;
    *x = t->tx + (u + v) * k;
    *y = t->ty + (v - u) * k;
}

/* A closed shape from (u, v) pairs: filled with `fill` and outlined 3 px with ink */
static void tbi_shape(const tbi_tool_t *t, const float *uv, int n, int fill, int outline)
{
    float p[24];
    for (int i = 0; i < n; i++) tbi_tp(t, uv[2 * i], uv[2 * i + 1], &p[2 * i], &p[2 * i + 1]);
    gfx_poly(t->g, p, n, fill);
    if (outline) gfx_poly_outline(t->g, p, n, 3, t->ink);
}

static void tbi_bar(const tbi_tool_t *t, float u0, float u1, float v, int w, int c)
{
    float ax, ay, bx, by;
    tbi_tp(t, u0, v, &ax, &ay);
    tbi_tp(t, u1, v, &bx, &by);
    gfx_line(t->g, ax, ay, bx, by, w, c);
}

static void tbi_across(const tbi_tool_t *t, float u, float v0, float v1, int w, int c)
{
    float ax, ay, bx, by;
    tbi_tp(t, u, v0, &ax, &ay);
    tbi_tp(t, u, v1, &bx, &by);
    gfx_line(t->g, ax, ay, bx, by, w, c);
}

static void tbi_pen(const tbi_tool_t *t)
{
    const float bristle[] = { 0, 0, 5, -5, 12, -8, 20, -7, 20, 7, 12, 8, 5, 5 }, ferrule[] = { 20, -7, 32, -7, 32, 7, 20, 7 },
        handle[] = { 32, -4, 60, -7, 60, 7, 32, 4 };
    tbi_shape(t, handle, 4, t->bg, 1);
    tbi_shape(t, ferrule, 4, t->bg, 1);
    tbi_across(t, 26, -7, 7, 3, t->ink);
    tbi_shape(t, bristle, 7, t->ink, 0);
}

/* The Pen: a nib on a barrel (the fountain pen's), apart from the Brush's tuft */
static void tbi_nib(const tbi_tool_t *t)
{
    const float nib[] = { 0, 0, 14, -8, 24, -8, 24, 8, 14, 8 }, grip[] = { 24, -6, 36, -6, 36, 6, 24, 6 }, barrel[] = { 36, -9, 58, -9, 58, 9, 36, 9 };
    tbi_shape(t, barrel, 4, t->bg, 1);
    tbi_bar(t, 42, 52, 0, 3, t->ink);
    tbi_shape(t, grip, 4, t->ink, 0);
    tbi_shape(t, nib, 5, t->ink, 0);
    tbi_bar(t, 1, 14, 0, 2, t->bg); /* the slit */
    tbi_bar(t, 14, 14, 0, 6, t->bg); /* the breather hole */
}

static void tbi_pencil(const tbi_tool_t *t)
{
    const float wood[] = { 0, 0, 14, -7, 14, 7 }, lead[] = { 0, 0, 5, -2.6f, 5, 2.6f }, body[] = { 14, -7, 50, -7, 50, 7, 14, 7 },
        rub[] = { 50, -7, 60, -7, 60, 7, 50, 7 };
    tbi_shape(t, body, 4, t->bg, 1);
    tbi_shape(t, wood, 3, t->bg, 1);
    tbi_shape(t, lead, 3, t->ink, 0);
    tbi_bar(t, 14, 50, 0, 3, t->ink);
    tbi_shape(t, rub, 4, t->ink, 0);
}

static void tbi_marker(const tbi_tool_t *t)
{
    const float nib[] = { 0, -3, 0, 5, 13, 9, 13, -9 }, body[] = { 13, -9, 52, -9, 52, 9, 13, 9 }, label[] = { 24, -9, 38, -9, 38, 9, 24, 9 };
    tbi_shape(t, body, 4, t->bg, 1);
    tbi_shape(t, label, 4, t->ink, 0);
    tbi_shape(t, nib, 4, t->ink, 0);
    tbi_across(t, 56, -6, 6, 3, t->ink);
}

static void tbi_spray(const tbi_tool_t *t, int x, int y)
{
    gfx_t *g = t->g;
    gfx_rrect(g, (float)(x + 18), (float)(y + 24), (float)(x + 46), (float)(y + 62), 4, 3, t->bg, t->ink);
    gfx_fill_rect(g, (float)(x + 18), (float)(y + 40), (float)(x + 46), (float)(y + 52), t->ink);
    gfx_rrect(g, (float)(x + 24), (float)(y + 15), (float)(x + 40), (float)(y + 24), 0, 3, t->bg, t->ink);
    gfx_fill_rect(g, (float)(x + 28), (float)(y + 6), (float)(x + 38), (float)(y + 15), t->ink);
    static const int dot[6][3] = { { 20, 8, 3 }, { 10, 5, 3 }, { 15, 16, 2 }, { 5, 14, 2 }, { 8, 24, 2 }, { 2, 6, 2 } };
    for (int i = 0; i < 6; i++)
        gfx_ellipse(g, (float)(x + dot[i][0] - dot[i][2]), (float)(y + dot[i][1] - dot[i][2]), (float)(x + dot[i][0] + dot[i][2]),
            (float)(y + dot[i][1] + dot[i][2]), 0, t->ink, t->ink);
}

static void tbi_eraser(const tbi_tool_t *t)
{
    const float whole[] = { 0, -13, 56, -13, 56, 13, 0, 13 }, worn[] = { 0, -13, 24, -13, 24, 13, 0, 13 };
    tbi_shape(t, whole, 4, t->bg, 1);
    tbi_shape(t, worn, 4, t->ink, 0);
}

/* A paint bucket: tapered pail with paint in it, a handle, a drop falling beside it */
static void tbi_bucket(gfx_t *g, int ix, int iy, int ink, int bg)
{
    float X = (float)ix, Y = (float)iy;
    float body[8] = { X + 10, Y + 22, X + 46, Y + 22, X + 40, Y + 58, X + 16, Y + 58 };
    gfx_poly(g, body, 4, bg);
    float paint[8] = { X + 12, Y + 32, X + 44, Y + 32, X + 41.5f, Y + 58, X + 14.5f, Y + 58 };
    gfx_poly(g, paint, 4, ink);
    gfx_poly_outline(g, body, 4, 4, ink);
    gfx_fill_rect(g, X + 7, Y + 20, X + 49, Y + 25, ink); /* the rim */
    float px = 0, py = 0;
    for (int i = 0; i <= 10; i++) { /* the handle, an arc over the rim */
        float th = (180.0f - 180.0f * (float)i / 10) * FM_PI / 180, qx = X + 28 + 19 * fm_cos(th), qy = Y + 22 - 15 * fm_sin(th);
        if (i) gfx_line(g, px, py, qx, qy, 3, ink);
        px = qx, py = qy;
    }
    float drop[6] = { X + 56, Y + 30, X + 51, Y + 40, X + 61, Y + 40 };
    gfx_poly(g, drop, 3, ink);
    gfx_ellipse(g, X + 50, Y + 36, X + 62, Y + 48, 0, ink, ink);
}

void tbi_brush(gfx_t *g, int brush, int x, int y, int ink, int bg)
{
    tbi_tool_t t = { g, (float)x + 8, (float)y + 56, ink, bg };
    switch (brush) {
    case BR_PEN: tbi_nib(&t); break;
    case BR_BRUSH: tbi_pen(&t); break;
    case BR_PENCIL: tbi_pencil(&t); break;
    case BR_MARKER: tbi_marker(&t); break;
    case BR_SPRAY: tbi_spray(&t, x, y); break;
    case BR_FILL: tbi_bucket(g, x, y, ink, bg); break;
    default: tbi_eraser(&t); break;
    }
}

/* Mirror: a small wing on each side of a dashed axis (off: one wing and no axis), the 4-way with both axes, the
   kaleidoscope with a star of axes round a dot */
void tbi_mirror(gfx_t *g, int mode, int ix, int iy, int ink, int bg)
{
    float X = (float)ix, Y = (float)iy;
    (void)bg;
    if (mode == MR_OFF || mode == MR_LR || mode == MR_4 || mode == MR_TB) {
        int lr = mode != MR_TB && mode != MR_OFF, tb = mode == MR_TB || mode == MR_4;
        float tri[2][6] = { { X + 6, Y + 14, X + 26, Y + 32, X + 6, Y + 50 }, { X + 58, Y + 14, X + 38, Y + 32, X + 58, Y + 50 } };
        if (mode == MR_TB) { /* wings above and below the axis */
            float up[6] = { X + 12, Y + 6, X + 32, Y + 26, X + 52, Y + 6 }, dn[6] = { X + 12, Y + 58, X + 32, Y + 38, X + 52, Y + 58 };
            gfx_poly(g, up, 3, ink), gfx_poly(g, dn, 3, ink);
        } else {
            gfx_poly(g, tri[0], 3, ink);
            if (mode != MR_OFF) gfx_poly(g, tri[1], 3, ink);
        }
        if (lr)
            for (int k = 0; k < 6; k++) gfx_fill_rect(g, X + 31, Y + 2 + (float)k * 11, X + 32, Y + 8 + (float)k * 11, ink);
        if (tb)
            for (int k = 0; k < 6; k++) gfx_fill_rect(g, X + 2 + (float)k * 11, Y + 31, X + 8 + (float)k * 11, Y + 32, ink);
        if (mode == MR_4) { /* four small wings round the crossing */
            float q[4][6] = { { X + 8, Y + 8, X + 26, Y + 14, X + 14, Y + 26 }, { X + 56, Y + 8, X + 38, Y + 14, X + 50, Y + 26 }, { X + 8, Y + 56, X + 26, Y + 50, X + 14, Y + 38 }, { X + 56, Y + 56, X + 38, Y + 50, X + 50, Y + 38 } };
            gfx_fill_rect(g, X + 4, Y + 4, X + 59, Y + 59, bg);
            for (int k = 0; k < 4; k++) gfx_poly(g, q[k], 3, ink);
            for (int k = 0; k < 6; k++) gfx_fill_rect(g, X + 31, Y + 2 + (float)k * 11, X + 32, Y + 8 + (float)k * 11, ink), gfx_fill_rect(g, X + 2 + (float)k * 11, Y + 31, X + 8 + (float)k * 11, Y + 32, ink);
        }
        return;
    }
    for (int k = 0; k < 4; k++) { /* a star: four axes at 45 degrees, a dot in the middle, petals at the ends */
        float a = (float)k * FM_PI / 4, dx = fm_cos(a) * 29, dy = fm_sin(a) * 29;
        gfx_line(g, X + 32 - dx, Y + 32 - dy, X + 32 + dx, Y + 32 + dy, 3, ink);
    }
    gfx_ellipse(g, X + 24, Y + 24, X + 40, Y + 40, 0, ink, ink);
    gfx_ellipse(g, X + 28, Y + 28, X + 36, Y + 36, 0, bg, bg);
}


/* A colour swatch (the toolbar's Black, Grey, White): the shade as it will be inked, in a white-ringed frame so black shows on a picked (black) cell */
void tbi_colour(gfx_t *g, int shade, int x, int y, int ink, int bg)
{
    int q = 40, sx = x + (TBI_SIZE - q) / 2, sy = y + (TBI_SIZE - q) / 2;
    (void)bg;
    gfx_rrect(g, (float)(sx - 8), (float)(sy - 8), (float)(sx + q + 7), (float)(sy + q + 7), 6, 0, GFX_WHITE, GFX_WHITE);
    gfx_rrect(g, (float)(sx - 4), (float)(sy - 4), (float)(sx + q + 3), (float)(sy + q + 3), 4, 4, GFX_WHITE, GFX_BLACK);
    for (int py = 0; py < q; py++)
        for (int px = 0; px < q; px++) gfx_px(g, sx + px, sy + py, shade != SH_WHITE && cv_shade_black(shade, sx + px, sy + py) ? GFX_BLACK : GFX_WHITE);
    (void)ink;
}

/* A size button: a dot of the pen size class, small, medium or big */
void tbi_size(gfx_t *g, int cls, int x, int y, int ink, int bg)
{
    static const int d[BR_SIZES] = { 12, 28, 52 };
    float cx = (float)x + 32, cy = (float)y + 32, r = (float)d[cls < 0 ? 0 : cls >= BR_SIZES ? BR_SIZES - 1 : cls] / 2;
    (void)bg;
    gfx_ellipse(g, cx - r, cy - r, cx + r, cy + r, 0, ink, ink);
}

/* The Menu button: three bars, each with a bullet in front, as a list */
void tbi_menu(gfx_t *g, int x, int y, int ink, int bg)
{
    float X = (float)x, Y = (float)y;
    (void)bg;
    for (int k = 0; k < 3; k++) {
        float cy = Y + 14 + (float)k * 18;
        gfx_ellipse(g, X + 4, cy - 4, X + 12, cy + 4, 0, ink, ink);
        gfx_fill_rect(g, X + 20, cy - 4, X + 60, cy + 3, ink);
    }
}
