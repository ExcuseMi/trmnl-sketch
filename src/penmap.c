/* SPDX-License-Identifier: MIT */
#include "penmap.h"
#include <app_api.h>
#include "fmath.h"

static const int min_d[3] = { 1, 2, 4 }, max_d[3] = { 5, 9, 17 }, era_d[3] = { 13, 27, 49 };

static int clampi(int v) { return v < 0 ? 0 : v > 2 ? 2 : v; }
int pen_min_d(int size) { return min_d[clampi(size)]; }
int pen_max_d(int size) { return max_d[clampi(size)]; }
int pen_erase_d(int size) { return era_d[clampi(size)]; }

int pen_diam(int size, unsigned pressure)
{
    if (pressure > 65535) pressure = 65535;
    /* 1 - (1 - p)^2 in 0..65535 */
    unsigned long long q = 65535 - pressure;
    unsigned curve = (unsigned)(65535 - q * q / 65535);
    int lo = pen_min_d(size), hi = pen_max_d(size);
    return lo + (int)(((unsigned long long)(hi - lo) * curve + 32767) / 65535);
}

void pm_init(penmap_t *m, int orient, int fb_w, int fb_h, uint32_t width_um, uint32_t height_um)
{
    m->orient = orient;
    m->fb_w = fb_w;
    m->fb_h = fb_h;
    int turned = orient == APP_ORIENT_BAR_LEFT || orient == APP_ORIENT_BAR_RIGHT;
    m->sw = turned ? fb_h : fb_w;
    m->sh = turned ? fb_w : fb_h;
    if (!width_um || !height_um) width_um = 16, height_um = 10;
    float a = (float)width_um / (float)height_um; /* tablet aspect */
    if (a >= (float)m->sw / (float)m->sh) { /* wider than the screen: its height fits, the sides are cut */
        m->ht = (float)m->sh;
        m->wt = m->ht * a;
    } else {
        m->wt = (float)m->sw;
        m->ht = m->wt / a;
    }
}

int pm_map(const penmap_t *m, unsigned x, unsigned y, int *fx, int *fy)
{
    float u = (float)x / 65535.0f * m->wt - (m->wt - (float)m->sw) / 2;
    float v = (float)y / 65535.0f * m->ht - (m->ht - (float)m->sh) / 2;
    int inside = u >= -0.5f && v >= -0.5f && u < (float)m->sw - 0.5f && v < (float)m->sh - 0.5f;
    int ax = fm_round(u), ay = fm_round(v);
    if (ax < 0) ax = 0;
    if (ay < 0) ay = 0;
    if (ax > m->sw - 1) ax = m->sw - 1;
    if (ay > m->sh - 1) ay = m->sh - 1;
    app_orient_to_fb(m->orient, m->fb_w, m->fb_h, ax, ay, fx, fy);
    return inside;
}
