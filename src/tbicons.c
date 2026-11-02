/* SPDX-License-Identifier: MIT */
#include "tbicons.h"
#include "brush.h"
#include "fmath.h"
#include "mirror.h"

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

static void tbi_fineliner(const tbi_tool_t *t)
{
    const float tip[] = { 0, 0, 12, -4, 12, 4 }, body[] = { 12, -6, 54, -6, 54, 6, 12, 6 }, grip[] = { 12, -6, 24, -6, 24, 6, 12, 6 };
    tbi_shape(t, body, 4, t->bg, 1);
    tbi_shape(t, grip, 4, t->ink, 0);
    tbi_shape(t, tip, 3, t->ink, 0);
    tbi_across(t, 48, -6, 6, 3, t->ink);
}

static void tbi_brushpen(const tbi_tool_t *t)
{
    const float bristle[] = { 0, 0, 5, -5, 12, -8, 20, -7, 20, 7, 12, 8, 5, 5 }, ferrule[] = { 20, -7, 32, -7, 32, 7, 20, 7 },
        handle[] = { 32, -4, 60, -7, 60, 7, 32, 4 };
    tbi_shape(t, handle, 4, t->bg, 1);
    tbi_shape(t, ferrule, 4, t->bg, 1);
    tbi_across(t, 26, -7, 7, 3, t->ink);
    tbi_shape(t, bristle, 7, t->ink, 0);
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

static void tbi_nib(const tbi_tool_t *t)
{
    const float blade[] = { 0, 0, 8, -6, 18, -13, 30, -12, 30, 12, 18, 13, 8, 6 }, collar[] = { 30, -11, 38, -11, 38, 11, 30, 11 };
    tbi_shape(t, blade, 7, t->ink, 0);
    tbi_bar(t, 0, 22, 0, 2, t->bg); /* the slit */
    float hx, hy;
    tbi_tp(t, 25, 0, &hx, &hy);
    gfx_ellipse(t->g, hx - 3, hy - 3, hx + 3, hy + 3, 0, t->bg, t->bg); /* the breather hole */
    tbi_shape(t, collar, 4, t->bg, 1);
    tbi_bar(t, 38, 58, 0, 7, t->ink);
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
    case BR_FINELINER: tbi_fineliner(&t); break;
    case BR_BRUSHPEN: tbi_brushpen(&t); break;
    case BR_PENCIL: tbi_pencil(&t); break;
    case BR_MARKER: tbi_marker(&t); break;
    case BR_CALLIGRAPHY: tbi_nib(&t); break;
    case BR_SPRAY: tbi_spray(&t, x, y); break;
    case BR_FILL: tbi_bucket(g, x, y, ink, bg); break;
    default: tbi_eraser(&t); break;
    }
}

/* A curved arrow: an arc over the top with the head at its end; mx 1 undo (head on the left), -1 redo (mirrored) */
static void tbi_curved(gfx_t *g, int x, int y, int mx, int ink)
{
    const float cx = (float)x + 32, cy = (float)y + 38, r = 19, m = (float)mx;
    const float a0 = 35, a1 = 205;
    float px = 0, py = 0;
    for (int i = 0; i <= 12; i++) {
        float th = (a0 + (a1 - a0) * (float)i / 12) * FM_PI / 180;
        float qx = cx + m * r * fm_cos(th), qy = cy - r * fm_sin(th);
        if (i) gfx_line(g, px, py, qx, qy, 7, ink);
        px = qx, py = qy;
    }
    float th = a1 * FM_PI / 180, dx = m * -fm_sin(th), dy = -fm_cos(th); /* direction of travel at the end */
    float p[6] = { px + dx * 14, py + dy * 14, px - dy * 13 - dx * 3, py + dx * 13 - dy * 3, px + dy * 13 - dx * 3, py - dx * 13 - dy * 3 };
    gfx_poly(g, p, 3, ink);
}

void tbi_undo(gfx_t *g, int x, int y, int ink, int bg) { (void)bg, tbi_curved(g, x, y, 1, ink); }
void tbi_redo(gfx_t *g, int x, int y, int ink, int bg) { (void)bg, tbi_curved(g, x, y, -1, ink); }

void tbi_clear(gfx_t *g, int x, int y, int ink, int bg)
{
    float X = (float)x, Y = (float)y;
    gfx_fill_rect(g, X + 11, Y + 14, X + 53, Y + 19, ink); /* the lid */
    gfx_rrect(g, X + 24, Y + 6, X + 40, Y + 16, 2, 3, bg, ink); /* its handle */
    float body[8] = { X + 16, Y + 22, X + 48, Y + 22, X + 44, Y + 60, X + 20, Y + 60 };
    gfx_poly(g, body, 4, bg);
    gfx_poly_outline(g, body, 4, 4, ink);
    for (int i = 0; i < 3; i++) gfx_line(g, X + 25 + (float)i * 7, Y + 30, X + 25 + (float)i * 7, Y + 52, 3, ink);
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

void tbi_tool(gfx_t *g, int tool, int x, int y, int ink, int bg)
{
    (void)bg;
    float X = (float)x, Y = (float)y;
    if (tool == TL_LINE) {
        gfx_line(g, X + 10, Y + 54, X + 54, Y + 10, 5, ink);
        gfx_ellipse(g, X + 4, Y + 48, X + 16, Y + 60, 0, ink, ink);
        gfx_ellipse(g, X + 48, Y + 4, X + 60, Y + 16, 0, ink, ink);
    } else if (tool == TL_RECT) {
        gfx_rrect(g, X + 8, Y + 14, X + 56, Y + 50, 0, 5, bg, ink);
    } else if (tool == TL_OVAL) {
        gfx_ellipse(g, X + 6, Y + 12, X + 58, Y + 52, 5, bg, ink);
    } else {
        gfx_fill_rect(g, X + 10, Y + 12, X + 46, Y + 18, ink);
        gfx_fill_rect(g, X + 25, Y + 12, X + 31, Y + 54, ink);
        gfx_fill_rect(g, X + 52, Y + 26, X + 54, Y + 56, ink); /* the caret */
    }
}
