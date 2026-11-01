/* Mirror drawing: the images, one undo step, replay, the screen-only guide, the pushed rectangles, every position of
   the X, the bucket. SPDX-License-Identifier: MIT */
#include "util.h"

static void settle(rig_t *r)
{
    rect_t o[SK_MAX_RECTS];
    for (int i = 0; i < 8; i++) rig_flush(r, r->now += 100, o);
}
static void set_mirror(rig_t *r, int mode)
{
    sk_set_mirror(&r->s, mode, r->now += 50);
    r->now += 3000;
    sk_tick(&r->s, r->now);
    settle(r);
}
static void brush(rig_t *r, int b, int shade, int size)
{
    sk_set_brush(&r->s, b, r->now += 50);
    sk_set_shade(&r->s, shade, r->now += 50);
    sk_set_size(&r->s, size, r->now += 50);
    r->now += 3000;
    sk_tick(&r->s, r->now);
}
/* a stroke in the left upper part of the page: the pen's own points, varying pressure */
static void stroke(rig_t *r)
{
    const int p[] = { 200, 180, 330, 260, 480, 220, 600, 340, 560, 470, 430, 520 };
    for (int i = 0; i < 6; i++) rig_pen(r, p[2 * i], p[2 * i + 1], APP_PEN_TIP | APP_PEN_IN_RANGE, 15000 + 8000 * (unsigned)i);
    rig_pen(r, 430, 520, 0, 0);
    settle(r);
}
static int black(const rig_t *r, int x, int y) { return !cv_get(&r->s.page, x, y); }
static int blacks(const canvas_t *c)
{
    int n = 0;
    for (int y = 0; y < c->h; y++)
        for (int x = 0; x < c->w; x++) n += !cv_get(c, x, y);
    return n;
}
/* every pixel equals its flipped one */
static int symmetric(const rig_t *r, int fx, int fy)
{
    for (int y = 0; y < TH; y++)
        for (int x = 0; x < TW; x++)
            if (cv_get(&r->s.page, x, y) != cv_get(&r->s.page, fx ? TW - 1 - x : x, fy ? TH - 1 - y : y)) return 0;
    return 1;
}

static void test_maps(void)
{
    for (int m = MR_OFF; m < MR_N; m++) {
        CHECK(mr_images(m) >= 1 && mr_name(m)[0]);
        for (int img = 0; img < mr_images(m); img++) {
            int x, y;
            mr_map(m, img, TW, TH, 300, 200, &x, &y);
            if (img == 0) CHECK(x == 300 && y == 200); /* image 0 is the pen's own */
            else CHECK(x != 300 || y != 200);
        }
    }
    int x, y, x2, y2;
    mr_map(MR_LR, 1, TW, TH, 300, 200, &x, &y);
    CHECK(x == TW - 1 - 300 && y == 200);
    mr_map(MR_TB, 1, TW, TH, 300, 200, &x, &y);
    CHECK(x == 300 && y == TH - 1 - 200);
    mr_map(MR_4, 3, TW, TH, 300, 200, &x, &y);
    CHECK(x == TW - 1 - 300 && y == TH - 1 - 200);
    for (int img = 0; img < 16; img++) { /* the mirrors are involutions, the quarter turns exact */
        mr_map(MR_K8, img, TW, TH, 777, 123, &x, &y);
        if (img == 8) {
            mr_map(MR_K8, img, TW, TH, x, y, &x2, &y2);
            CHECK(x2 == 777 && y2 == 123);
        }
        if (img == 4) {
            int rx, ry;
            mr_map(MR_K8, 0, TW, TH, 777, 123, &rx, &ry);
            CHECK(x == TW - 1 - 777 && y == TH - 1 - 123 && rx == 777); /* 180 degrees */
        }
    }
    int px = 1500, py = 300; /* eight turns of 45 degrees come back (rounding: a pixel or two) */
    for (int k = 0; k < 8; k++) mr_map(MR_K8, 1, TW, TH, px, py, &px, &py);
    CHECK(px >= 1497 && px <= 1503 && py >= 297 && py <= 303);
}

static void test_images_exact(void)
{
    static const int modes[3] = { MR_LR, MR_TB, MR_4 };
    static const int bs[] = { BR_FINELINER, BR_BRUSHPEN, BR_PENCIL, BR_MARKER, BR_CALLIGRAPHY, BR_SPRAY };
    for (int bi = 0; bi < 6; bi++)
        for (int mi = 0; mi < 3; mi++) {
            rig_t r, one;
            rig_new(&r, APP_ORIENT_BAR_BOTTOM);
            rig_new(&one, APP_ORIENT_BAR_BOTTOM);
            brush(&r, bs[bi], SH_BLACK, 12), brush(&one, bs[bi], SH_BLACK, 12);
            set_mirror(&r, modes[mi]);
            stroke(&r);
            stroke(&one); /* the same stroke with no mirror: the expected page is its images */
            static uint8_t exp[TS * TH];
            memset(exp, 0xff, sizeof(exp));
            canvas_t e = { exp, TW, TH, TS };
            for (int y = 0; y < TH; y++)
                for (int x = 0; x < TW; x++)
                    if (!cv_get(&one.s.page, x, y))
                        for (int img = 0; img < mr_images(modes[mi]); img++) {
                            int ox, oy;
                            mr_map(modes[mi], img, TW, TH, x, y, &ox, &oy);
                            cv_set(&e, ox, oy, 0);
                        }
            CHECK(!memcmp(exp, r.s.page.px, sizeof(exp))); /* every brush, exactly: grain and spray dots mirrored too */
            CHECK(blacks(&r.s.page) > 300);
            if (modes[mi] == MR_LR) CHECK(symmetric(&r, 1, 0));
            if (modes[mi] == MR_TB) CHECK(symmetric(&r, 0, 1));
            if (modes[mi] == MR_4) CHECK(symmetric(&r, 1, 0) && symmetric(&r, 0, 1));
            rig_end(&r);
            rig_end(&one);
        }
}

static void test_k8_and_shades(void)
{
    for (int sh = 0; sh < SH_N; sh++) {
        rig_t r;
        rig_new(&r, APP_ORIENT_BAR_BOTTOM);
        brush(&r, BR_FINELINER, sh, 10);
        set_mirror(&r, MR_K8);
        stroke(&r);
        /* the pen's own points and their 15 images are all inked (the dither only thins it: look about each) */
        const hist_t *h = &r.s.hist;
        CHECK(h->st_cur == 1 && h->st[0].n >= 4);
        for (uint32_t i = 0; i < h->st[0].n; i++)
            for (int img = 0; img < 16; img++) {
                int ox, oy, hit = 0;
                mr_map(MR_K8, img, TW, TH, h->pts[i].x, h->pts[i].y, &ox, &oy);
                if (ox < 8 || oy < 8 || ox > TW - 9 || oy > TH - 9) continue; /* a turned page is cropped */
                for (int dy = -6; dy <= 6; dy++)
                    for (int dx = -6; dx <= 6; dx++) hit += black(&r, ox + dx, oy + dy);
                CHECK(hit > 0);
            }
        CHECK(blacks(&r.s.page) > 2000);
        rig_end(&r);
    }
    rig_t r; /* the dither: the two halves of a left-right stroke carry about the same ink */
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    brush(&r, BR_MARKER, SH_DARK, 12);
    set_mirror(&r, MR_LR);
    stroke(&r);
    int l = 0, rr = 0;
    for (int y = 0; y < TH; y++)
        for (int x = 0; x < TW / 2; x++) l += black(&r, x, y), rr += black(&r, x + TW / 2, y);
    CHECK(l > 1000 && rr > l * 9 / 10 && rr < l * 11 / 10);
    rig_end(&r);
}

static void test_undo_replay_png(void)
{
    for (int m = MR_LR; m < MR_N; m++) {
        rig_t r;
        rig_new(&r, APP_ORIENT_BAR_BOTTOM);
        brush(&r, BR_PENCIL, SH_LIGHT, 10);
        set_mirror(&r, m);
        static uint8_t before[TS * TH], after[TS * TH];
        memcpy(before, r.s.page.px, sizeof(before));
        stroke(&r);
        memcpy(after, r.s.page.px, sizeof(after));
        CHECK(memcmp(before, after, sizeof(before)));
        CHECK(r.s.hist.st_cur == 1); /* one stroke in the log, however many images */
        sk_undo(&r.s, r.now += 50);
        CHECK(!memcmp(before, r.s.page.px, sizeof(before)) && !hist_can_undo(&r.s.hist)); /* ONE undo takes them all */
        sk_redo(&r.s, r.now += 50);
        CHECK(!memcmp(after, r.s.page.px, sizeof(after)));
        sk_undo(&r.s, r.now += 50);
        sk_redo(&r.s, r.now += 50);
        CHECK(!memcmp(after, r.s.page.px, sizeof(after)));
        /* a second stroke, then the first one baked into the base by many more: still the same pixels */
        sk_save_now(&r.s);
        rig_start(&r, APP_ORIENT_BAR_BOTTOM); /* the page file, as the restart finds it: no guide in it */
        CHECK(!memcmp(after, r.s.page.px, sizeof(after)));
        CHECK(r.s.sty.mirror == MR_OFF);
        rig_end(&r);
    }
}

/* pixels on the page's two axes (the columns and rows through its centre) where the glass differs from the page */
static int guide_px(const rig_t *r)
{
    int n = 0;
    for (int y = 0; y < TH; y++) n += cv_get(&r->s.fb, TW / 2 - 1, y) != cv_get(&r->s.page, TW / 2 - 1, y), n += cv_get(&r->s.fb, TW / 2, y) != cv_get(&r->s.page, TW / 2, y);
    for (int x = 0; x < TW; x++) n += cv_get(&r->s.fb, x, TH / 2 - 1) != cv_get(&r->s.page, x, TH / 2 - 1), n += cv_get(&r->s.fb, x, TH / 2) != cv_get(&r->s.page, x, TH / 2);
    return n;
}

static void test_guide(void)
{
    for (int m = MR_LR; m < MR_N; m++) {
        rig_t r;
        rig_new(&r, APP_ORIENT_BAR_BOTTOM);
        set_mirror(&r, MR_OFF);
        CHECK(guide_px(&r) == 0);
        set_mirror(&r, m);
        CHECK(sk_is_blank(&r.s.page)); /* the guide is on the glass only: the page is still white */
        CHECK(guide_px(&r) > 200); /* dashes on the axes */
        sk_save_now(&r.s);
        CHECK(!r.s.dirty);
        stroke(&r); /* ink, then the page file: the guide pixels are not in it */
        sk_save_now(&r.s);
        rig_start(&r, APP_ORIENT_BAR_BOTTOM);
        if (m != MR_K8) { /* the strokes stay off the axes: the saved page has nothing on them, so none of the guide */
            int axis_ink = 0;
            for (int y = 0; y < TH; y++) axis_ink += black(&r, TW / 2 - 1, y) + black(&r, TW / 2, y);
            for (int x = 0; x < TW; x++) axis_ink += black(&r, x, TH / 2 - 1) + black(&r, x, TH / 2);
            CHECK(axis_ink == 0);
        }
        CHECK(guide_px(&r) == 0); /* a restart starts with the mirror off */
        set_mirror(&r, m);
        int on = guide_px(&r);
        set_mirror(&r, MR_OFF); /* off: the glass is the page again, no mark */
        CHECK(on > 200 && guide_px(&r) == 0);
        rig_end(&r);
    }
}

static void test_rects_cover(void)
{
    for (int m = MR_LR; m < MR_N; m++) {
        rig_t r;
        rig_new(&r, APP_ORIENT_BAR_BOTTOM);
        brush(&r, BR_MARKER, SH_BLACK, 6);
        set_mirror(&r, m);
        static uint8_t before[TS * TH], seen[TS * TH];
        memcpy(before, r.fb, sizeof(before));
        memcpy(seen, before, sizeof(seen)); /* the panel as the pushes build it up */
        const int p[] = { 200, 180, 330, 260, 480, 220, 600, 340 };
        rect_t o[SK_MAX_RECTS];
        for (int i = 0; i < 4; i++) {
            rig_pen(&r, p[2 * i], p[2 * i + 1], APP_PEN_TIP | APP_PEN_IN_RANGE, 30000);
            for (int k = 0; k < 3; k++) {
                int n = rig_frame(&r, o);
                for (int j = 0; j < n && n != SK_FULL; j++)
                    for (int y = o[j].y; y < o[j].y + o[j].h; y++) memcpy(seen + y * TS + o[j].x / 8, r.fb + y * TS + o[j].x / 8, (size_t)o[j].w / 8);
            }
        }
        rig_pen(&r, 600, 340, 0, 0);
        for (int k = 0; k < 4; k++) {
            int n = rig_frame(&r, o);
            for (int j = 0; j < n && n != SK_FULL; j++)
                for (int y = o[j].y; y < o[j].y + o[j].h; y++) memcpy(seen + y * TS + o[j].x / 8, r.fb + y * TS + o[j].x / 8, (size_t)o[j].w / 8);
        }
        CHECK(!memcmp(seen, r.fb, sizeof(seen))); /* what was pushed is the whole glass: every copy of the stroke was in a rectangle */
        CHECK(memcmp(before, r.fb, sizeof(before)));
        rig_end(&r);
    }
}

static void test_orientations(void)
{
    static const int os[4] = { APP_ORIENT_BAR_BOTTOM, APP_ORIENT_BAR_TOP, APP_ORIENT_BAR_LEFT, APP_ORIENT_BAR_RIGHT };
    for (int o = 0; o < 4; o++) {
        rig_t r;
        rig_new(&r, os[o]);
        brush(&r, BR_FINELINER, SH_BLACK, 12);
        set_mirror(&r, MR_4);
        rig_pen_user(&r, 500, 400, APP_PEN_TIP | APP_PEN_IN_RANGE, 30000);
        for (int i = 1; i <= 20; i++) rig_pen_user(&r, 500 + i * 25, 400 + i * 12, APP_PEN_TIP | APP_PEN_IN_RANGE, 30000);
        rig_pen_user(&r, 1000, 640, 0, 0);
        settle(&r);
        CHECK(blacks(&r.s.page) > 500 && symmetric(&r, 1, 0) && symmetric(&r, 0, 1)); /* about the page's centre whichever way the X is turned */
        int on = 0; /* the guide is on the page's axes in the framebuffer */
        for (int y = 0; y < 14; y++) on += cv_get(&r.s.fb, TW / 2 - 1, y) != cv_get(&r.s.page, TW / 2 - 1, y);
        CHECK(on > 0);
        rig_end(&r);
    }
}

static void test_fill_mirrored(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    brush(&r, BR_FINELINER, SH_BLACK, 8);
    set_mirror(&r, MR_LR);
    rig_line(&r, 300, 300, 700, 300, 60000, 0); /* a box on the left, its mirror on the right */
    rig_line(&r, 700, 300, 700, 600, 60000, 0);
    rig_line(&r, 700, 600, 300, 600, 60000, 0);
    rig_line(&r, 300, 600, 300, 300, 60000, 0);
    settle(&r);
    CHECK(symmetric(&r, 1, 0));
    static uint8_t before[TS * TH];
    memcpy(before, r.s.page.px, sizeof(before));
    sk_set_brush(&r.s, BR_FILL, r.now += 50);
    sk_set_shade(&r.s, SH_BLACK, r.now += 50);
    r.now += 3000;
    sk_tick(&r.s, r.now);
    rig_pen(&r, 500, 450, APP_PEN_TIP | APP_PEN_IN_RANGE, 30000);
    rig_pen(&r, 500, 450, 0, 0);
    settle(&r);
    CHECK(black(&r, 500, 450) && black(&r, TW - 1 - 500, 450)); /* both boxes */
    CHECK(!black(&r, TW / 2, 450) && symmetric(&r, 1, 0));
    sk_undo(&r.s, r.now += 50);
    CHECK(!memcmp(before, r.s.page.px, sizeof(before))); /* one step */
    rig_end(&r);
}

static void test_cursor_and_toolbar(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    sk_pen_key(&r.s, 1u << 3, 1u << 3, r.now += 50);
    sk_pen_key(&r.s, 0, 1u << 3, r.now);
    rig_pen_user(&r, r.s.tb.cx[TI_MIRROR] + 20, r.s.tb.cy + 20, APP_PEN_TIP | APP_PEN_IN_RANGE, 30000);
    rig_pen_user(&r, r.s.tb.cx[TI_MIRROR] + 20, r.s.tb.cy + 20, APP_PEN_IN_RANGE, 0);
    CHECK(r.s.sty.mirror == MR_LR && r.s.note_on); /* a tap cycles, a toast names it */
    CHECK(TI_MIRROR == TI_SIZE + 3 && r.s.tb.cx[TI_MIRROR] + r.s.tb.cell <= r.s.tb.cx[TI_UNDO]);
    for (int o = 0; o < 2; o++) {
        tb_layout_t L;
        tb_layout(&L, o ? APP_ORIENT_BAR_TOP : APP_ORIENT_BAR_BOTTOM, 1404, 1872);
        CHECK(L.cell >= 68 && L.cx[TI_N - 1] + L.cell <= 1404);
    }
    rig_end(&r);
}

int main(void)
{
    test_maps();
    test_images_exact();
    test_k8_and_shades();
    test_undo_replay_png();
    test_guide();
    test_rects_cover();
    test_orientations();
    test_fill_mirrored();
    test_cursor_and_toolbar();
    return CHECK_DONE("test_mirror");
}
