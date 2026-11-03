/* Grey ink over black: the brush lays its own pattern, black dots and white dots, in its shape (grey on a black page
   showed nothing: grey only added black dots, 2026-11-03). Over white paper grey is the same as before: only its own
   black dots. SPDX-License-Identifier: MIT */
#include "check.h"
#include "util.h"

static const int brs[] = { BR_PEN, BR_BRUSH, BR_PENCIL, BR_MARKER, BR_SPRAY };

static void stroke(rig_t *r)
{
    int p[10] = { 300, 400, 420, 430, 560, 410, 700, 450, 820, 440 };
    rig_stroke(r, p, 5, 30000, 0);
}
static void draw(rig_t *r, int br, int shade, int page)
{
    rig_new(r, APP_ORIENT_BAR_BOTTOM);
    cv_fill(&r->s.page, page);
    hist_reset(&r->s.hist);
    sk_set_brush(&r->s, br, r->now += 50);
    sk_set_size(&r->s, 2, r->now += 50);
    sk_set_shade(&r->s, shade, r->now += 50);
    stroke(r);
}

int main(void)
{
    for (unsigned i = 0; i < sizeof(brs) / sizeof(brs[0]); i++) {
        rig_t k, g, w;
        draw(&k, brs[i], SH_BLACK, 1); /* the shape: black over white */
        draw(&g, brs[i], SH_GREY, 0);  /* grey over black */
        draw(&w, brs[i], SH_GREY, 1);  /* grey over white */
        int whites = 0, room = 0, bad = 0;
        for (int y = 0; y < TH; y++)
            for (int x = 0; x < TW; x++) {
                int in = !cv_get(&k.s.page, x, y), pat = cv_shade_black(SH_GREY, x, y);
                if (cv_get(&g.s.page, x, y)) whites++, bad += !in || pat; /* white only in the shape, off the pattern */
                room += in && !pat;
                if (!cv_get(&w.s.page, x, y)) bad += !in || !pat; /* over white: its own dots, in the shape */
            }
        CHECK(bad == 0);
        CHECK(whites > 100 && whites * 4 > room); /* grey shows on black */
        if (brs[i] == BR_PEN) CHECK(whites == room); /* the solid pen: exactly the pattern */
        rig_end(&k), rig_end(&g), rig_end(&w);
    }
    return CHECK_DONE("test_grey");
}
