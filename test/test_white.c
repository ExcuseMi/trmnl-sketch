/* White as an ink colour (shade SH_WHITE, the far left of the tone slider): every brush draws it in its own shape over
   black, undo and redo give back the exact pixels, Recent and the settings keep it, the bucket and the text use it.
   SPDX-License-Identifier: MIT */
#include "check.h"
#include <stdio.h>
#include "util.h"

#define DOWN (APP_PEN_TIP | APP_PEN_IN_RANGE)

static const int inks[] = { BR_FINELINER, BR_BRUSHPEN, BR_PENCIL, BR_MARKER, BR_CALLIGRAPHY, BR_SPRAY };

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
        sk_set_size(&a.s, 20, a.now += 50);
        sk_set_shade(&a.s, SH_BLACK, a.now += 50);
        stroke(&a);
        memcpy(blk, a.s.page.px, sizeof(blk));
        int n_black = count(&a.s.page, 0);
        CHECK(n_black > 200);
        rig_new(&b, APP_ORIENT_BAR_BOTTOM);
        black_page(&b);
        sk_set_brush(&b.s, br, b.now += 50);
        sk_set_size(&b.s, 20, b.now += 50);
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

static void test_recent_and_settings(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    sk_set_shade(&r.s, SH_WHITE, r.now += 50);
    CHECK(r.s.note_on && !strcmp(shade_name(SH_WHITE), "White"));
    sk_set_brush(&r.s, BR_PENCIL, r.now += 50);
    sk_set_size(&r.s, 14, r.now += 50);
    stroke(&r);
    CHECK(r.s.rc_n == 1 && r.s.rc[0].shade == SH_WHITE && r.s.rc[0].brush == BR_PENCIL);
    sk_set_shade(&r.s, SH_BLACK, r.now += 50);
    sk_toolbar(&r.s, r.now);
    int x = r.s.tb.cx[TI_RECENT] + r.s.tb.cell / 2, y = r.s.tb.cyi[TI_RECENT] + r.s.tb.cell / 2;
    rig_pen_user(&r, x, y, APP_PEN_IN_RANGE, 0);
    rig_pen_user(&r, x, y, DOWN, 30000);
    rig_pen_user(&r, x, y, APP_PEN_IN_RANGE, 0);
    CHECK(r.s.sty.shade == SH_WHITE); /* a tap on the slot gives white back */
    rig_start(&r, APP_ORIENT_BAR_BOTTOM); /* a restart keeps the colour and the Recent slot */
    CHECK(r.s.sty.shade == SH_WHITE && r.s.rc_n == 1 && r.s.rc[0].shade == SH_WHITE);
    rig_end(&r);
}

static void test_slider(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    sk_toolbar(&r.s, r.now);
    tb_layout_t *L = &r.s.tb;
    CHECK(tb_tone_of(L, L->tn_x0) == SH_WHITE && tb_tone_of(L, L->tn_x1) == SH_BLACK);
    for (int sh = 0; sh <= SH_WHITE; sh++) CHECK(tb_tone_of(L, tb_tone_x(L, sh)) == sh);
    sk_set_shade(&r.s, SH_WHITE + 1, r.now); /* not a tone: refused */
    CHECK(r.s.sty.shade != SH_WHITE + 1);
    rig_end(&r);
}

static void test_fill_and_text(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    black_page(&r);
    /* a white ring on black, the bucket in white inside it turns the black disc white */
    sk_set_shade(&r.s, SH_WHITE, r.now += 50);
    sk_set_brush(&r.s, BR_FINELINER, r.now += 50);
    sk_set_size(&r.s, 6, r.now += 50);
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
    /* text in white over black */
    static uint8_t pre[TS * TH];
    memcpy(pre, r.s.page.px, sizeof(pre));
    sk_set_brush(&r.s, BR_FINELINER, r.now += 50);
    sk_keyboard(&r.s, APP_KBD_USB, r.now);
    sk_set_tool(&r.s, TL_TEXT, r.now);
    rig_pen(&r, 200, 300, APP_PEN_IN_RANGE, 0);
    rig_pen(&r, 200, 300, DOWN, 30000);
    rig_pen(&r, 201, 300, DOWN, 30000);
    rig_pen(&r, 201, 300, APP_PEN_IN_RANGE, 0);
    r.now += 20;
    sk_key(&r.s, 1, APP_KEY_CHAR, 0, 0x04, "H", r.now);
    sk_key(&r.s, 0, APP_KEY_CHAR, 0, 0x04, "H", r.now);
    sk_key(&r.s, 1, APP_KEY_ESCAPE, 0, 0, "", r.now += 20);
    sk_key(&r.s, 0, APP_KEY_ESCAPE, 0, 0, "", r.now);
    int w = 0;
    for (int yy = 0; yy < TH; yy++)
        for (int xx = 0; xx < TW; xx++) w += cv_get(&r.s.page, xx, yy) && !(pre[yy * TS + (xx >> 3)] >> (7 - (xx & 7)) & 1);
    CHECK(w > 100);
    rig_end(&r);
}

int main(void)
{
    test_each_brush();
    test_recent_and_settings();
    test_slider();
    test_fill_and_text();
    return CHECK_DONE("test_white");
}
