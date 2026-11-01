/* Pressure to width, and the tablet's area onto the page in the four positions. SPDX-License-Identifier: MIT */
#include "check.h"
#include <app_api.h>
#include <math.h>
#include <stdlib.h>
#include "penmap.h"
#include "util.h"
#include "oldsizes.h"

static void test_width(void)
{
    for (int size = 0; size < 3; size++) {
        int prev = 0;
        for (unsigned p = 0; p <= 65535; p += 257) {
            int d = pen_diam(size, p);
            CHECK(d >= pen_min_d(size) && d <= pen_max_d(size));
            CHECK(d >= prev); /* more pressure never thins the line */
            prev = d;
        }
        CHECK(pen_diam(size, 0) == pen_min_d(size));
        CHECK(pen_diam(size, 65535) == pen_max_d(size));
        /* a light hand (a third of the range) already makes about half or more of the extra width */
        CHECK(2 * (pen_diam(size, 21845) - pen_min_d(size)) >= pen_max_d(size) - pen_min_d(size));
        CHECK(pen_erase_d(size) > pen_max_d(size));
    }
    CHECK(pen_max_d(0) < pen_max_d(1) && pen_max_d(1) < pen_max_d(2)); /* thin, medium, thick differ */
    CHECK(pen_diam(1, 70000) == pen_max_d(1)); /* out of range is capped */
    CHECK(pen_diam(7, 30000) == pen_diam(2, 30000)); /* a bad size is capped */
}

static void test_map(void)
{
    for (int o = 0; o < 4; o++) {
        penmap_t m;
        pm_init(&m, o, 1872, 1404, 254000, 158750, 0);
        int fx, fy;
        /* the middle of the tablet is the middle of the page */
        CHECK(pm_map(&m, 32768, 32768, &fx, &fy) == 1);
        CHECK(abs(fx - 936) <= 1 && abs(fy - 702) <= 1);
        /* the user's top left, top right: the tablet's corners map to the user's frame in this position */
        int ax, ay, bx, by;
        pm_map(&m, 32768, 0, &fx, &fy); /* top middle of the tablet */
        app_orient_to_fb(o, 1872, 1404, m.sw / 2, 0, &ax, &ay);
        CHECK(abs(fx - ax) <= 1 && abs(fy - ay) <= 1);
        pm_map(&m, 65535, 32768, &fx, &fy); /* right middle */
        app_orient_to_fb(o, 1872, 1404, m.sw - 1, m.sh / 2, &bx, &by);
        CHECK(abs(fx - bx) <= 1 && abs(fy - by) <= 1);
        /* the whole page can be reached: the cut-off edges clamp, the rest is on the page */
        int inside = 0, outside = 0;
        for (unsigned x = 0; x <= 65535; x += 4096)
            for (unsigned y = 0; y <= 65535; y += 4096) {
                int r = pm_map(&m, x, y, &fx, &fy);
                CHECK(fx >= 0 && fx < 1872 && fy >= 0 && fy < 1404);
                inside += r, outside += !r;
            }
        CHECK(inside > 0 && outside > 0);
        /* no stretching: a step along the tablet's width and one along its height, equal in micrometres, move the
           pen equally far on the page */
        float px_per_um_x = m.wt / 254000.0f, px_per_um_y = m.ht / 158750.0f;
        CHECK(fabsf(px_per_um_x / px_per_um_y - 1.0f) < 0.001f);
        /* and the page is covered: the scaled tablet is at least as big as the screen both ways */
        CHECK(m.wt >= (float)m.sw - 0.5f && m.ht >= (float)m.sh - 0.5f);
    }
}

/* the user's-frame point that lands on framebuffer pixel (fx, fy) in position o */
static void user_of(int o, int sw, int sh, int fx, int fy, int *ux, int *uy)
{
    *ux = *uy = -1;
    for (int ay = 0; ay < sh; ay++)
        for (int ax = 0; ax < sw; ax++) {
            int qx, qy;
            app_orient_to_fb(o, 1872, 1404, ax, ay, &qx, &qy);
            if (qx == fx && qy == fy) {
                *ux = ax, *uy = ay;
                return;
            }
        }
}

/* Moving the pen right or down on the tablet moves the cursor right or down for the user, in every position */
static void test_direction(void)
{
    for (int o = 0; o < 4; o++) {
        penmap_t m;
        pm_init(&m, o, 1872, 1404, 254000, 158750, 0);
        int f[2], g[2], u0[2], u1[2], u2[2];
        pm_map(&m, 30000, 30000, &f[0], &f[1]);
        pm_map(&m, 40000, 30000, &g[0], &g[1]); /* right on the tablet */
        user_of(o, m.sw, m.sh, f[0], f[1], &u0[0], &u0[1]);
        user_of(o, m.sw, m.sh, g[0], g[1], &u1[0], &u1[1]);
        CHECK(u1[0] > u0[0] && u1[1] == u0[1]);
        pm_map(&m, 30000, 40000, &g[0], &g[1]); /* down on the tablet */
        user_of(o, m.sw, m.sh, g[0], g[1], &u2[0], &u2[1]);
        CHECK(u2[1] > u0[1] && u2[0] == u0[0]);
    }
}

static void test_unknown_size(void)
{
    penmap_t m;
    pm_init(&m, 0, 1872, 1404, 0, 0, 0); /* pen_info does not know: 16:10 */
    CHECK(fabsf(m.wt / m.ht - 1.6f) < 0.001f);
    penmap_t sq;
    pm_init(&sq, 0, 1872, 1404, 100000, 100000, 0); /* a square tablet: width fits, the top and bottom are cut */
    CHECK(fabsf(sq.wt - 1872.0f) < 0.5f && sq.ht == sq.wt);
}

/* fill (the default): every tablet corner is a page corner in all four positions, and the whole tablet is on the page */
static void test_fill(void)
{
    for (int o = 0; o < 4; o++) {
        penmap_t m;
        pm_init(&m, o, 1872, 1404, 254000, 158750, 1);
        CHECK(m.wt == (float)(m.sw - 1) && m.ht == (float)(m.sh - 1) && m.ox == 0 && m.oy == 0);
        const unsigned c[4][2] = { { 0, 0 }, { 65535, 0 }, { 0, 65535 }, { 65535, 65535 } };
        for (int k = 0; k < 4; k++) {
            int fx, fy, ax, ay;
            CHECK(pm_map(&m, c[k][0], c[k][1], &fx, &fy) == 1);
            app_orient_to_fb(o, 1872, 1404, c[k][0] ? m.sw - 1 : 0, c[k][1] ? m.sh - 1 : 0, &ax, &ay);
            CHECK(fx == ax && fy == ay);
        }
        for (unsigned x = 0; x <= 65535; x += 4096)
            for (unsigned y = 0; y <= 65535; y += 4096) {
                int fx, fy;
                CHECK(pm_map(&m, x, y, &fx, &fy) == 1); /* nothing is cut off */
            }
    }
    /* keep still cuts the long side: its corners are not the page's */
    penmap_t k;
    pm_init(&k, 0, 1872, 1404, 254000, 158750, 0);
    int fx, fy;
    CHECK(pm_map(&k, 0, 0, &fx, &fy) == 0);
}

/* The setting: no value means fill; "keep" keeps the aspect; the settings page closing applies a change */
static void test_setting(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    CHECK(r.s.fill && r.s.map.ox == 0); /* default: fill */
    strcpy(r.cfg_pen_area, "keep");
    rig_start(&r, APP_ORIENT_BAR_BOTTOM);
    CHECK(!r.s.fill && r.s.map.ox > 0);
    strcpy(r.cfg_pen_area, "fill");
    sk_cfg_reload(&r.s);
    CHECK(r.s.fill && r.s.map.ox == 0);
    strcpy(r.cfg_pen_area, "junk"); /* a bad value: the default */
    sk_cfg_reload(&r.s);
    CHECK(r.s.fill);
    rig_end(&r);
}

int main(void)
{
    test_fill();
    test_setting();
    test_width();
    test_map();
    test_direction();
    test_unknown_size();
    return CHECK_DONE("test_pen");
}
