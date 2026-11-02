/* White as an ink colour (shade SH_WHITE, the toolbar's White swatch): every brush draws it in its own shape over
   black, undo and redo give back the exact pixels, the settings keep it, the bucket uses it.
   SPDX-License-Identifier: MIT */
#include "check.h"
#include <stdio.h>
#include "util.h"

#define DOWN (APP_PEN_TIP | APP_PEN_IN_RANGE)

static const int inks[] = { BR_PEN, BR_PENCIL, BR_MARKER, BR_SPRAY };

static void black_page(rig_t *r)
{
    cv_fill(&r->s.page, 0);
    hist_reset(&r->s.hist);
}
static int count(const canvas_t *c, int colour)
{
    int n = 0;
    for (int y = 0; y < c->h; y++)
        for (int x = 0; x < c->w; x++) n += cv_get(c, x, y) == colour;
    return n;
}
static void stroke(rig_t *r)
{
    int p[10] = { 300, 400, 420, 430, 560, 410, 700, 450, 820, 440 };
    rig_stroke(r, p, 5, 30000, 0);
}

/* Each brush: the white stroke over black takes exactly the pixels the black stroke takes over white (its own shape,
   pressure and spray/grain), and nothing black appears. */
static void test_each_brush(void)
{
    for (unsigned i = 0; i < sizeof(inks) / sizeof(inks[0]); i++) {
        int br = inks[i];
        static uint8_t blk[TS * TH];
        rig_t a, b;
        rig_new(&a, APP_ORIENT_BAR_BOTTOM);
        sk_set_brush(&a.s, br, a.now += 50);
        sk_set_size(&a.s, 2, a.now += 50);
        sk_set_shade(&a.s, SH_BLACK, a.now += 50);
        stroke(&a);
        memcpy(blk, a.s.page.px, sizeof(blk));
        int n_black = count(&a.s.page, 0);
        CHECK(n_black > 200);
        rig_new(&b, APP_ORIENT_BAR_BOTTOM);
        black_page(&b);
        sk_set_brush(&b.s, br, b.now += 50);
        sk_set_size(&b.s, 2, b.now += 50);
        sk_set_shade(&b.s, SH_WHITE, b.now += 50);
        CHECK(b.s.sty.shade == SH_WHITE);
        stroke(&b);
        CHECK(count(&b.s.page, 1) == n_black); /* the same shape, now white */
        for (int y = 0; y < TH; y++)
            for (int x = 0; x < TW; x++)
                if (cv_get(&a.s.page, x, y) != 1) CHECK(cv_get(&b.s.page, x, y) == 1); /* white wherever black went */
        /* undo and redo of the white stroke */
        static uint8_t done[TS * TH];
        memcpy(done, b.s.page.px, sizeof(done));
        sk_undo(&b.s, b.now += 50);
        CHECK(count(&b.s.page, 1) == 0);
        sk_redo(&b.s, b.now += 50);
        CHECK(!memcmp(done, b.s.page.px, sizeof(done)));
        rig_end(&a);
        rig_end(&b);
    }
}

static void test_swatch_and_settings(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    sk_toolbar(&r.s, r.now);
    int x = r.s.tb.cx[TI_WHITE] + r.s.tb.cell / 2, y = r.s.tb.cyi[TI_WHITE] + r.s.tb.cell / 2;
    rig_pen_user(&r, x, y, APP_PEN_IN_RANGE, 0);
    rig_pen_user(&r, x, y, DOWN, 30000);
    rig_pen_user(&r, x, y, APP_PEN_IN_RANGE, 0);
    CHECK(r.s.sty.shade == SH_WHITE && r.s.note_on && !strcmp(shade_name(SH_WHITE), "White") && !strcmp(shade_name(SH_GREY), "Grey"));
    sk_set_shade(&r.s, SH_WHITE + 1, r.now += 50); /* not a swatch: refused */
    sk_set_shade(&r.s, 2, r.now += 50);
    CHECK(r.s.sty.shade == SH_WHITE);
    rig_start(&r, APP_ORIENT_BAR_BOTTOM); /* a restart keeps the colour */
    CHECK(r.s.sty.shade == SH_WHITE);
    rig_end(&r);
}

static void test_fill(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    black_page(&r);
    /* a white ring on black, the bucket in white inside it turns the black disc white */
    sk_set_shade(&r.s, SH_WHITE, r.now += 50);
    sk_set_brush(&r.s, BR_PEN, r.now += 50);
    sk_set_size(&r.s, 0, r.now += 50);
    int ring[2 * 13];
    for (int k = 0; k < 13; k++) ring[2 * k] = 800 + (int)(200 * __builtin_cos(k * 6.2831853 / 12)), ring[2 * k + 1] = 800 + (int)(200 * __builtin_sin(k * 6.2831853 / 12));
    rig_stroke(&r, ring, 13, 40000, 0);
    int white0 = count(&r.s.page, 1);
    sk_set_brush(&r.s, BR_FILL, r.now += 50);
    rig_pen(&r, 800, 800, APP_PEN_IN_RANGE, 0);
    rig_pen(&r, 800, 800, DOWN, 30000);
    rig_pen(&r, 800, 800, APP_PEN_IN_RANGE, 0);
    CHECK(cv_get(&r.s.page, 800, 800) == 1 && cv_get(&r.s.page, 820, 790) == 1 && count(&r.s.page, 1) > white0 + 50000);
    CHECK(cv_get(&r.s.page, 800, 1100) == 0); /* outside the ring stays black */
    static uint8_t done[TS * TH];
    memcpy(done, r.s.page.px, sizeof(done));
    sk_undo(&r.s, r.now += 50);
    CHECK(cv_get(&r.s.page, 800, 800) == 0);
    sk_redo(&r.s, r.now += 50);
    CHECK(!memcmp(done, r.s.page.px, sizeof(done)));
    rig_end(&r);
}

int main(void)
{
    test_each_brush();
    test_swatch_and_settings();
    test_fill();
    return CHECK_DONE("test_white");
}
