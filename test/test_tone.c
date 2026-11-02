/* The tone slider: 9 steps from light to black on the 4 x 4 Bayer levels, a pattern fixed to the page (strokes and fills
   of one tone meet cleanly, undo and replay give the same dots), the earlier three shades map onto it, the slider moves
   the knob with the pen and keeps the tone. SPDX-License-Identifier: MIT */
#include "check.h"
#include <stdio.h>
#include "fill.h"
#include "util.h"

#define DOWN (APP_PEN_TIP | APP_PEN_IN_RANGE)

static void test_levels(void)
{
    static const int want[SH_N] = { 16, 15, 14, 12, 10, 8, 6, 4, 2 }; /* black, then lighter */
    CHECK(SH_N == 9 && SH_BLACK == 0 && cv_shade_level(SH_DARK) == 12 && cv_shade_level(SH_LIGHT) == 6);
    for (int sh = 0; sh < SH_N; sh++) {
        int n = 0;
        for (int y = 0; y < 16; y++)
            for (int x = 0; x < 16; x++) n += cv_shade_black(sh, x + 4 * 7, y + 4 * 3);
        CHECK(n == want[sh] * 16); /* each step's dot density, on any 16 x 16 window */
        CHECK(cv_shade_level(sh) == want[sh]);
        if (sh) CHECK(cv_shade_level(sh) < cv_shade_level(sh - 1)); /* strictly lighter */
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++) CHECK(cv_shade_black(sh, x, y) == cv_shade_black(sh, x + 4, y + 8)); /* the same dots wherever */
    }
    for (int sh = 1; sh < SH_N; sh++) /* each lighter step only drops dots of the one before */
        for (int y = 0; y < 4; y++)
            for (int x = 0; x < 4; x++) CHECK(!cv_shade_black(sh, x, y) || cv_shade_black(sh - 1, x, y));
}

static void test_page_fixed(void)
{
    for (int sh = 0; sh < SH_N; sh++) {
        rig_t r;
        rig_new(&r, APP_ORIENT_BAR_BOTTOM);
        sk_set_shade(&r.s, sh, r.now);
        sk_set_size(&r.s, 30, r.now);
        rig_line(&r, 300, 400, 900, 420, 40000, 0);
        rig_line(&r, 500, 300, 520, 600, 40000, 0); /* a crossing stroke of the same tone: no darker where they meet */
        for (int y = 380; y < 440; y++)
            for (int x = 400; x < 700; x++)
                if (!cv_get(&r.s.page, x, y)) CHECK(cv_shade_black(sh, x, y)); /* every dot is the page's own pattern */
        static uint8_t a[TS * TH];
        memcpy(a, r.s.page.px, sizeof(a));
        sk_undo(&r.s, r.now);
        sk_undo(&r.s, r.now);
        CHECK(sk_is_blank(&r.s.page));
        sk_redo(&r.s, r.now);
        sk_redo(&r.s, r.now);
        CHECK(!memcmp(a, r.s.page.px, sizeof(a))); /* replay: the same dots */
        rig_end(&r);
    }
    /* a fill and a stroke of one tone meet on the same dots */
    static canvas_t c;
    static uint8_t px[TS * TH];
    c = (canvas_t){ px, TW, TH, TS };
    cv_fill(&c, 1);
    for (int k = 0; k < SH_N; k++) {
        for (int y = 100; y < 300; y++) cv_span_s(&c, y, 100, 105, 0, 0), cv_span_s(&c, y, 394, 399, 0, 0);
        for (int x = 100; x < 400; x++) cv_span_s(&c, 100, x, x, 0, 0), cv_span_s(&c, 299, x, x, 0, 0);
        bf_t r;
        CHECK(bf_plan(&r, &c, 250, 200, k, 0, 0) == BF_OK);
        uint32_t n = bf_runs(&r);
        hpt_t *runs = malloc(n * sizeof(hpt_t));
        bf_emit(&r, runs);
        bf_apply(&c, runs, n, k);
        free(runs);
        bf_free(&r);
        for (int y = 110; y < 290; y++)
            for (int x = 110; x < 390; x++) CHECK((!cv_get(&c, x, y)) == cv_shade_black(k, x, y)); /* the fill is the tone */
        CHECK(bf_plan(&r, &c, 250, 200, k, 0, 0) == BF_NOTHING); /* and the bucket knows it by sight, for every step */
        bf_free(&r);
        CHECK(bf_plan(&r, &c, 250, 200, (k + 3) % SH_N, 0, 0) == BF_OK);
        bf_free(&r);
        cv_fill(&c, 1);
    }
}

static void test_migration(void)
{
    static const int old_to_new[3] = { SH_BLACK, SH_DARK, SH_LIGHT };
    for (int old = 0; old < 3; old++) {
        rig_t r;
        rig_new(&r, APP_ORIENT_BAR_BOTTOM);
        r.cfg_shade[0] = (char)(0x30 + old), r.cfg_shade[1] = 0; /* a settings file of the earlier version */
        r.cfg_tone[0] = 0;
        rig_start(&r, APP_ORIENT_BAR_BOTTOM);
        CHECK(r.s.sty.shade == old_to_new[old]);
        sk_set_shade(&r.s, 5, r.now);
        CHECK(!strcmp(r.cfg_tone, "5")); /* the new key wins from now on */
        rig_start(&r, APP_ORIENT_BAR_BOTTOM);
        CHECK(r.s.sty.shade == 5);
        rig_end(&r);
    }
}

static void test_slider(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    sk_toolbar(&r.s, r.now);
    const tb_layout_t *L = &r.s.tb;
    CHECK(tb_tone_of(L, L->tn_x0 - 50) == SH_WHITE && tb_tone_of(L, L->tn_x1 + 50) == SH_BLACK); /* white at the far left, then light, black at the right */
    int prev = SH_WHITE + 1;
    for (int x = L->tn_x0; x <= L->tn_x1; x++) {
        int t = tb_tone_of(L, x);
        CHECK(t <= prev && t >= 0 && t <= SH_WHITE); /* monotonic */
        prev = t;
    }
    for (int sh = 0; sh <= SH_WHITE; sh++) CHECK(tb_tone_of(L, tb_tone_x(L, sh)) == sh); /* the knob's place gives its tone back */
    CHECK(tb_hit(L, tb_tone_x(L, 4), L->tn_y) == TI_TONE && tb_hit(L, L->cx[TI_TONE + 2] + 5, L->tn_y) == TI_TONE);
    /* the pen picks the knob up and drags: the tone follows, no ink, the toast names it on the lift */
    static uint8_t page[TS * TH];
    memcpy(page, r.s.page.px, sizeof(page));
    rig_pen_user(&r, tb_tone_x(L, SH_BLACK), L->tn_y, APP_PEN_IN_RANGE, 0);
    rig_pen_user(&r, tb_tone_x(L, SH_BLACK), L->tn_y, DOWN, 30000);
    CHECK(r.s.tb_drag == 2);
    rig_pen_user(&r, tb_tone_x(L, 4), L->tn_y + 30, DOWN, 30000);
    CHECK(r.s.sty.shade == 4);
    rig_pen_user(&r, tb_tone_x(L, SH_N - 1), L->tn_y + 300, DOWN, 30000); /* dragged off the strip: still the slider's */
    CHECK(r.s.sty.shade == SH_N - 1);
    rig_pen_user(&r, tb_tone_x(L, 2), L->tn_y, APP_PEN_IN_RANGE, 0);
    CHECK(r.s.tb_drag == 0);
    CHECK(r.s.note_on && !memcmp(page, r.s.page.px, sizeof(page)));
    int kept = r.s.sty.shade;
    CHECK(kept > 0 && kept < SH_N);
    /* the next stroke is in that tone */
    rig_line(&r, 300, 700, 900, 720, 40000, 0);
    int dots = 0, own = 1;
    for (int y = 690; y < 730; y++)
        for (int x = 300; x < 900; x++)
            if (!cv_get(&r.s.page, x, y)) dots++, own &= cv_shade_black(kept, x, y);
    CHECK(dots > 50 && own);
    rig_end(&r);
}

int main(void)
{
    test_levels();
    test_page_fixed();
    test_migration();
    test_slider();
    return CHECK_DONE("test_tone");
}
