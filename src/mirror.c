/* SPDX-License-Identifier: MIT */
#include "mirror.h"

const char *mr_name(int mode)
{
    static const char *const n[MR_N] = { "Off", "Left-right", "Top-bottom", "4-way", "Kaleidoscope 8" };
    return n[mode < 0 || mode >= MR_N ? 0 : mode];
}

int mr_images(int mode)
{
    static const uint8_t n[MR_N] = { 1, 2, 2, 4, 16 };
    return n[mode < 0 || mode >= MR_N ? 0 : mode];
}

/* cos and sin of k * 45 degrees in Q16 */
static const int cs[8][2] = { { 65536, 0 }, { 46341, 46341 }, { 0, 65536 }, { -46341, 46341 }, { -65536, 0 }, { -46341, -46341 }, { 0, -65536 }, { 46341, -46341 } };

void mr_map(int mode, int img, int w, int h, int x, int y, int *ox, int *oy)
{
    int u = 2 * x - (w - 1), v = 2 * y - (h - 1); /* about the centre, in half pixels */
    int fx = 0, fy = 0, k = 0;
    switch (mode) {
    case MR_LR: fx = img & 1; break;
    case MR_TB: fy = img & 1; break;
    case MR_4: fx = img & 1, fy = img >> 1 & 1; break;
    case MR_K8: fx = img >> 3 & 1, k = img & 7; break;
    default: break;
    }
    if (fx) u = -u;
    if (fy) v = -v;
    if (k) {
        long long a = (long long)u * cs[k][0] - (long long)v * cs[k][1], b = (long long)u * cs[k][1] + (long long)v * cs[k][0];
        u = (int)((a + 32768) >> 16), v = (int)((b + 32768) >> 16);
    }
    *ox = (u + (w - 1)) >> 1, *oy = (v + (h - 1)) >> 1;
}

static void flip(canvas_t *fb, const box_t *c, int x, int y)
{
    if (x >= c->x0 && x <= c->x1 && y >= c->y0 && y <= c->y1) cv_flip(fb, x, y);
}

#define DASH 14 /* px on, px off */

void mr_guide(canvas_t *fb, int mode, const box_t *clip)
{
    if (mode <= MR_OFF || mode >= MR_N) return;
    box_t c = *clip;
    box_clip(&c, fb->w, fb->h);
    if (box_empty(&c)) return;
    c.x0 &= ~7, c.x1 |= 7; /* the page is copied to the glass in whole bytes: the guide goes over those too */
    if (c.x1 > fb->w - 1) c.x1 = fb->w - 1;
    int w = fb->w, h = fb->h, cx = (w - 1) / 2, cy = (h - 1) / 2;
    int vert = mode != MR_TB, horz = mode != MR_LR;
    if (vert) /* x = centre: two columns (the axis lies between them when the width is even) */
        for (int y = c.y0; y <= c.y1; y++)
            if ((y / DASH) % 2 == 0)
                for (int x = cx; x <= cx + 1 - (w & 1) && x <= cx + 1; x++) flip(fb, &c, x, y);
    if (horz)
        for (int x = c.x0; x <= c.x1; x++)
            if ((x / DASH) % 2 == 0)
                for (int y = cy; y <= cy + 1 - (h & 1) && y <= cy + 1; y++) flip(fb, &c, x, y);
    if (mode == MR_K8) /* the two diagonals, 2 px thick, from the centre outwards */
        for (int d = -1; d <= 1; d += 2)
            for (int t = -(h / 2) - 1; t <= h / 2 + 1; t++)
                if ((((t < 0 ? -t : t) / DASH) % 2) == 0) {
                    flip(fb, &c, cx + t, cy + d * t);
                    flip(fb, &c, cx + t + 1, cy + d * t);
                }
}
