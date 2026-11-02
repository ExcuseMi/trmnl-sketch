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
    for (unsigned k = 0; k < 2 * sizeof(old) / sizeof(old[0]); k++) {
        int is_new = k >= sizeof(old) / sizeof(old[0]); /* the second round: the same values in the new key, which may also be damaged */
        static rig_t r;
        static uint8_t saved[TS * TH];
        rig_new(&r, APP_ORIENT_BAR_BOTTOM);
        draw_and_save(&r);
        memcpy(saved, r.s.page.px, sizeof(saved));
        rig_cfg(&r, is_new ? "tool" : "brush", old[k % (sizeof(old) / sizeof(old[0]))]);
        rig_cfg(&r, "tone", "5");
        rig_cfg(&r, "size", "30");
        rig_cfg(&r, "esize", "40");
        rig_cfg(&r, "rc0", "9.4.12");
        rig_cfg(&r, "rc1", "0.1.5");
        rig_cfg(&r, "smooth", "10");
        rig_cfg(&r, "use_tilt", "off");
        rig_cfg(&r, "pressure_curve", "firm");
        r.keep_tool = 1;
        rig_start(&r, APP_ORIENT_BAR_BOTTOM);
        CHECK(!memcmp(saved, r.s.page.px, sizeof(saved))); /* the page is as it was */
        CHECK(r.s.sty.brush < BR_N && r.s.sty.brush != BR_ERASER && tb_item_of_shade(r.s.sty.shade) >= 0 && r.s.size_cls >= 0 && r.s.size_cls < BR_SIZES);
        CHECK(r.s.sty.size == brush_size(r.s.sty.brush, r.s.size_cls));
        CHECK(r.s.sty.brush == BR_BRUSH); /* owner 2026-11-02: Sketch opens with the Brush, whatever tool was saved */
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

int main(void)
{
    test_old_settings();
    return CHECK_DONE("test_oldpage");
}
