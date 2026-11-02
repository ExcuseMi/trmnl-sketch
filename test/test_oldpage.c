/* A page drawn with a tool that is gone still opens and shows as it was: a page is its pixels (a 1-bit PNG), and the
   settings of the earlier toolbar (a removed brush id, the tone slider, sizes, the Recent slots, smooth, tilt, pressure
   curve) are read without an error and become a valid pick. SPDX-License-Identifier: MIT */
#include "check.h"
#include "util.h"

static void draw_and_save(rig_t *r)
{
    int p[] = { 300, 400, 500, 450, 700, 400, 900, 460 };
    rig_stroke(r, p, 4, 40000, 0);
    rig_pen(r, 900, 460, APP_PEN_IN_RANGE, 0);
    sk_save_now(&r->s);
}

static void test_old_settings(void)
{
    /* every brush id of the earlier table (0 fineliner .. 7 fill), then junk */
    static const char *const old[] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "99", "-1", "x" };
    for (unsigned k = 0; k < sizeof(old) / sizeof(old[0]); k++) {
        static rig_t r;
        static uint8_t saved[TS * TH];
        rig_new(&r, APP_ORIENT_BAR_BOTTOM);
        draw_and_save(&r);
        memcpy(saved, r.s.page.px, sizeof(saved));
        rig_cfg(&r, "brush", old[k]);
        rig_cfg(&r, "tone", "5");
        rig_cfg(&r, "size", "30");
        rig_cfg(&r, "esize", "40");
        rig_cfg(&r, "rc0", "9.4.12");
        rig_cfg(&r, "rc1", "0.1.5");
        rig_cfg(&r, "smooth", "10");
        rig_cfg(&r, "use_tilt", "off");
        rig_cfg(&r, "pressure_curve", "firm");
        rig_start(&r, APP_ORIENT_BAR_BOTTOM);
        CHECK(!memcmp(saved, r.s.page.px, sizeof(saved))); /* the page is as it was */
        CHECK(r.s.sty.brush < BR_N && r.s.sty.brush != BR_ERASER && tb_item_of_shade(r.s.sty.shade) >= 0 && r.s.size_cls >= 0 && r.s.size_cls < BR_SIZES);
        CHECK(r.s.sty.size == brush_size(r.s.sty.brush, r.s.size_cls));
        if (k < 8) CHECK(r.s.sty.brush == brush_from_old((int)k == 6 ? 0 : (int)k) || k == 6); /* the eraser becomes the Pen on start, as it always did */
        CHECK(r.s.sty.shade == SH_GREY); /* the old tone 5 */
        /* the toolbar draws and works with it, and a new stroke inks */
        sk_toolbar(&r.s, r.now);
        sk_toolbar(&r.s, r.now);
        if (r.s.sty.brush == BR_FILL) sk_set_brush(&r.s, BR_PEN, r.now += 50); /* (the bucket inks nothing on a drag) */
        rig_line(&r, 300, 700, 800, 710, 40000, 0);
        CHECK(memcmp(saved, r.s.page.px, sizeof(saved)));
        rig_end(&r);
    }
}

static void test_brush_map(void)
{
    CHECK(brush_from_old(0) == BR_PEN && brush_from_old(1) == BR_PEN && brush_from_old(4) == BR_PEN); /* fineliner, brush pen and calligraphy are the Pen */
    CHECK(brush_from_old(2) == BR_PENCIL && brush_from_old(3) == BR_MARKER && brush_from_old(5) == BR_SPRAY && brush_from_old(6) == BR_ERASER && brush_from_old(7) == BR_FILL);
    CHECK(brush_from_old(8) == BR_PEN && brush_from_old(-1) == BR_PEN && brush_from_old(1000) == BR_PEN);
}

int main(void)
{
    test_brush_map();
    test_old_settings();
    return CHECK_DONE("test_oldpage");
}
