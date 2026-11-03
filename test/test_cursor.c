/* The cursors of every tool: black with a 2 px white outline, at least 32 px, drawn exactly (outline included) over white,
   black ink, paper and every tone, and taken away without a mark. SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 */
#include "check.h"
#include <stdio.h>
#include "skui.h"
#include "util.h"

static uint8_t bg[TS * TH], work[TS * TH];

/* the background: 0 white, 1 black, 2 grid paper, 3.. a tone 0..8 */
static void background(canvas_t *c, int kind)
{
    cv_fill(c, 1);
    box_t all = { 0, 0, TW - 1, TH - 1 };
    if (kind == 1) cv_fill(c, 0);
    if (kind == 2) pp_overlay(c, &all, PP_GRID, 20);
    if (kind >= 3)
        for (int y = 300; y < 900; y++) cv_span_s(c, y, 300, 900, 0, kind - 3);
}

static void test_exact_outline(void)
{
    static const int tcs[] = { BR_PEN, BR_PENCIL, BR_MARKER, BR_SPRAY, BR_ERASER, BR_FILL };
    static const int sizes[] = { 1, 5, 9, 16, 17, 30, 33, 48 };
    for (unsigned t = 0; t < sizeof(tcs) / sizeof(tcs[0]); t++)
        for (unsigned z = 0; z < sizeof(sizes) / sizeof(sizes[0]); z++) {
            int tc = tcs[t], size = sizes[z], r = brush_cursor_r(tc, size), minx = 999, maxx = -999, miny = 999, maxy = -999;
            for (int dy = -r; dy <= r; dy++)
                for (int dx = -r; dx <= r; dx++)
                    if (brush_cursor_px(tc, size, dx, dy)) {
                        minx = dx < minx ? dx : minx, maxx = dx > maxx ? dx : maxx, miny = dy < miny ? dy : miny, maxy = dy > maxy ? dy : maxy;
                    }
            CHECK(r >= 16 && minx >= -r && maxx <= r && miny >= -r && maxy <= r);
            for (int kind = 0; kind < 3 + SH_N; kind++) {
                canvas_t c = { work, TW, TH, TS }, b = { bg, TW, TH, TS };
                background(&b, kind);
                memcpy(work, bg, sizeof(work));
                box_t box;
                int cx = 600, cy = 600;
                ui_cursor(&c, cx, cy, tc, size, &box);
                for (int dy = -r - 4; dy <= r + 4; dy++)
                    for (int dx = -r - 4; dx <= r + 4; dx++) {
                        int near = 0, core = brush_cursor_px(tc, size, dx, dy) && dx >= -r && dx <= r && dy >= -r && dy <= r;
                        for (int y = dy - BR_CUR_OUT; y <= dy + BR_CUR_OUT; y++)
                            for (int x = dx - BR_CUR_OUT; x <= dx + BR_CUR_OUT; x++)
                                if (x >= -r && x <= r && y >= -r && y <= r && brush_cursor_px(tc, size, x, y)) near = 1;
                        int want = core ? 0 : near ? 1 : cv_get(&b, cx + dx, cy + dy);
                        CHECK(cv_get(&c, cx + dx, cy + dy) == want); /* black core, white outline, the rest untouched */
                    }
                cv_copy_box(&c, &b, &box); /* taken away: the background as it was */
                CHECK(!memcmp(work, bg, sizeof(work)));
            }
        }
}

static void test_in_the_app(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    for (int k = 0; k < BR_N; k++) {
        sk_set_brush(&r.s, k, r.now);
        rig_pen(&r, 700, 700, APP_PEN_IN_RANGE, 0);
        CHECK(r.s.cursor && r.s.cb == k);
        rig_pen(&r, 10, 600, 0, 0); /* out of range: the cursor goes, no mark is left */
        r.now += 3000;
        sk_tick(&r.s, r.now);
        CHECK(!r.s.cursor && same_page_and_fb(&r));
    }
    rig_end(&r);
}

int main(void)
{
    test_exact_outline();
    test_in_the_app();
    return CHECK_DONE("test_cursor");
}
