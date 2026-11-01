/* SPDX-License-Identifier: MIT */
#include "penmap.h"
#include <app_api.h>
#include "fmath.h"

int pen_lerp(int size, int a, int b, int c, int lo, int hi)
{
    if (size < 1) size = 1;
    if (size > 48) size = 48;
    int v = size <= 9 ? a + (size - 5) * (b - a) / 4 : b + (size - 9) * (c - b) / 8;
    return v < lo ? lo : v > hi ? hi : v;
}
int pen_min_d(int size) { return pen_lerp(size, 1, 2, 4, 1, 12); }
int pen_max_d(int size) { return pen_lerp(size, 5, 9, 17, 1, 48); }
int pen_erase_d(int size) { return pen_lerp(size, 13, 27, 49, 5, 92); }

int pen_diam(int size, unsigned pressure)
{
    if (pressure > 65535) pressure = 65535;
    /* 1 - (1 - p)^2 in 0..65535 */
    unsigned long long q = 65535 - pressure;
    unsigned curve = (unsigned)(65535 - q * q / 65535);
    int lo = pen_min_d(size), hi = pen_max_d(size);
    return lo + (int)(((unsigned long long)(hi - lo) * curve + 32767) / 65535);
}

void pm_init(penmap_t *m, int orient, int fb_w, int fb_h, uint32_t width_um, uint32_t height_um, int fill, int turn)
{
    m->orient = orient;
    m->fb_w = fb_w;
    m->fb_h = fb_h;
    int turned = orient == APP_ORIENT_BAR_LEFT || orient == APP_ORIENT_BAR_RIGHT;
    m->sw = turned ? fb_h : fb_w;
    m->sh = turned ? fb_w : fb_h;
    if (!width_um || !height_um) width_um = 16, height_um = 10;
    m->turn = turn && (orient == APP_ORIENT_BAR_LEFT || orient == APP_ORIENT_BAR_RIGHT) ? (orient == APP_ORIENT_BAR_LEFT ? 1 : 2) : 0;
    if (m->turn) { uint32_t t = width_um; width_um = height_um, height_um = t; } /* the turned tablet's width is its height */
    float a = (float)width_um / (float)height_um; /* tablet aspect */
    if (fill) { /* every tablet edge is a screen edge (the aspect is not kept) */
        m->wt = (float)(m->sw - 1); /* the last tablet unit is the last pixel */
        m->ht = (float)(m->sh - 1);
        m->ox = m->oy = 0;
        return;
    }
    if (a >= (float)m->sw / (float)m->sh) { /* wider than the screen: its height fits, the sides are cut */
        m->ht = (float)m->sh;
        m->wt = m->ht * a;
    } else {
        m->wt = (float)m->sw;
        m->ht = m->wt / a;
    }
    m->ox = (m->wt - (float)m->sw) / 2, m->oy = (m->ht - (float)m->sh) / 2;
}

int pm_map(const penmap_t *m, unsigned x, unsigned y, int *fx, int *fy)
{
    if (m->turn == 1) { unsigned t = x; x = 65535 - y, y = t; }       /* clockwise: the tablet's right is down, its down is left */
    else if (m->turn == 2) { unsigned t = x; x = y, y = 65535 - t; }  /* counter-clockwise */
    float u = (float)x / 65535.0f * m->wt - m->ox;
    float v = (float)y / 65535.0f * m->ht - m->oy;
    int inside = u >= -0.5f && v >= -0.5f && u < (float)m->sw - 0.5f && v < (float)m->sh - 0.5f;
    int ax = fm_round(u), ay = fm_round(v);
    if (ax < 0) ax = 0;
    if (ay < 0) ay = 0;
    if (ax > m->sw - 1) ax = m->sw - 1;
    if (ay > m->sh - 1) ay = m->sh - 1;
    app_orient_to_fb(m->orient, m->fb_w, m->fb_h, ax, ay, fx, fy);
    return inside;
}
