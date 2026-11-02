/* The tones: 9 steps from light to black on the 4 x 4 Bayer levels, a pattern fixed to the page (strokes and fills
   of one tone meet cleanly, undo and replay give the same dots). The toolbar offers three: Black, Grey (SH_GREY) and White;
   the settings of the earlier tone slider and three shades map onto them; a tap on a swatch picks it and the next stroke is in it. SPDX-License-Identifier: MIT */
#include "check.h"
#include <stdio.h>
#include "fill.h"
#include "util.h"

#define DOWN (APP_PEN_TIP | APP_PEN_IN_RANGE)

static void test_levels(void)
{
    static const int want[SH_N] = { 16, 15, 14, 12, 10, 8, 6, 4, 2 }; /* black, then lighter */
    CHECK(SH_N == 9 && SH_BLACK == 0 && cv_shade_level(SH_GREY) == 8 && cv_shade_level(SH_DARK) == 12 && cv_shade_level(SH_LIGHT) == 6);
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
    for (int sh = 0; sh < SH_N; sh += 5) { /* black and grey: white is test_white.c */
        rig_t r;
        rig_new(&r, APP_ORIENT_BAR_BOTTOM);
        sk_set_shade(&r.s, sh, r.now);
        sk_set_size(&r.s, 2, r.now);
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

/* The settings of the earlier versions never make an error: the old shade (0 black, 1 and 2 greys) and the old tone (0 black, 1..8 lighter, 9 white) become Black, Grey or White */
static void test_migration(void)
{
    static const struct { const char *shade, *tone, *colour; int want; } c[] = {
        { "0", "", "", SH_BLACK }, { "1", "", "", SH_GREY }, { "2", "", "", SH_GREY },
        { "", "0", "", SH_BLACK }, { "", "3", "", SH_GREY }, { "", "8", "", SH_GREY }, { "", "9", "", SH_WHITE },
        { "", "", "9", SH_WHITE }, { "", "5", "0", SH_BLACK }, { "", "", "4", SH_BLACK } /* not a swatch: Black */, { "", "77", "", SH_GREY },
    };
    for (unsigned k = 0; k < sizeof(c) / sizeof(c[0]); k++) {
        rig_t r;
        rig_new(&r, APP_ORIENT_BAR_BOTTOM);
        if (c[k].shade[0]) rig_cfg(&r, "shade", c[k].shade);
        if (c[k].tone[0]) rig_cfg(&r, "tone", c[k].tone);
        if (c[k].colour[0]) rig_cfg(&r, "colour", c[k].colour);
        rig_start(&r, APP_ORIENT_BAR_BOTTOM);
        CHECK(r.s.sty.shade == c[k].want && tb_item_of_shade(r.s.sty.shade) >= 0);
        sk_set_shade(&r.s, SH_GREY, r.now);
        CHECK(!strcmp(rig_cfgv(&r, "colour"), "5")); /* the new key wins from now on */
        rig_start(&r, APP_ORIENT_BAR_BOTTOM);
        CHECK(r.s.sty.shade == SH_GREY);
        rig_end(&r);
    }
}

/* The swatches: a pen tap picks, no ink, and the next stroke is in that colour (grey = the page-fixed pattern) */
static void test_swatches(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    sk_toolbar(&r.s, r.now);
    const tb_layout_t *L = &r.s.tb;
    CHECK(tb_shade_of(TI_BLACK) == SH_BLACK && tb_shade_of(TI_GREY) == SH_GREY && tb_shade_of(TI_WHITE) == SH_WHITE && tb_shade_of(TI_BRUSH) == -1);
    static uint8_t page[TS * TH];
    memcpy(page, r.s.page.px, sizeof(page));
    int x = L->cx[TI_GREY] + L->cell / 2, y = L->cyi[TI_GREY] + L->cell / 2;
    rig_pen_user(&r, x, y, APP_PEN_IN_RANGE, 0);
    rig_pen_user(&r, x, y, DOWN, 30000);
    rig_pen_user(&r, x, y, APP_PEN_IN_RANGE, 0);
    CHECK(r.s.sty.shade == SH_GREY && r.s.note_on && !memcmp(page, r.s.page.px, sizeof(page)));
    rig_line(&r, 300, 700, 900, 720, 40000, 0);
    int dots = 0, own = 1;
    for (int y2 = 690; y2 < 730; y2++)
        for (int x2 = 300; x2 < 900; x2++)
            if (!cv_get(&r.s.page, x2, y2)) dots++, own &= cv_shade_black(SH_GREY, x2, y2);
    CHECK(dots > 50 && own);
    rig_end(&r);
}

int main(void)
{
    test_levels();
    test_page_fixed();
    test_migration();
    test_swatches();
    return CHECK_DONE("test_tone");
}
