/* The Text tool: a tap puts the caret, a keyboard types (layout characters arrive as APP_KEY_CHAR with utf8), Backspace, Enter,
   Escape or a tap elsewhere ends it; the text is on the screen only until then and ink in one undo step. Without a
   keyboard the tool says so. SPDX-License-Identifier: MIT */
#include "check.h"
#include <stdio.h>
#include "textdraw.h"
#include "util.h"

#define DOWN (APP_PEN_TIP | APP_PEN_IN_RANGE)
static void tap_at(rig_t *r, int x, int y)
{
    rig_pen(r, x, y, APP_PEN_IN_RANGE, 0);
    rig_pen(r, x, y, DOWN, 30000);
    rig_pen(r, x + 1, y, DOWN, 30000);
    rig_pen(r, x + 1, y, APP_PEN_IN_RANGE, 0);
}
static void type(rig_t *r, const char *utf8)
{
    r->now += 20;
    sk_key(&r->s, 1, APP_KEY_CHAR, 0, 0x04, utf8, r->now);
    sk_key(&r->s, 0, APP_KEY_CHAR, 0, 0x04, utf8, r->now);
}
static void press(rig_t *r, unsigned key)
{
    r->now += 20;
    sk_key(&r->s, 1, key, 0, 0, "", r->now);
    sk_key(&r->s, 0, key, 0, 0, "", r->now);
}
static void say(rig_t *r, const char *s)
{
    for (const char *p = s; *p;) {
        char c[5] = { 0 };
        int n = (*p & 0xe0) == 0xc0 ? 2 : 1;
        for (int i = 0; i < n; i++) c[i] = *p++;
        type(r, c);
    }
}
static int black_in(const canvas_t *c, int x0, int y0, int x1, int y1)
{
    int n = 0;
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) n += !cv_get(c, x, y);
    return n;
}
static void settle(rig_t *r) { rig_pen(r, 10, 600, 0, 0); r->now += 3000; sk_tick(&r->s, r->now); }

static void test_no_keyboard(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    sk_set_tool(&r.s, TL_TEXT, r.now);
    CHECK(r.s.note_on && r.s.kbd_req); /* "Connect a keyboard", and the glue is asked to enable them */
    tap_at(&r, 400, 500);
    CHECK(!r.s.tx_on && r.s.note_on);
    say(&r, "abc");
    CHECK(r.s.tx_n == 0 && r.s.hist.st_cur == 0);
    rig_end(&r);
}

static void test_typing(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    sk_keyboard(&r.s, APP_KBD_USB, r.now);
    sk_set_tool(&r.s, TL_TEXT, r.now);
    static uint8_t before[TS * TH];
    memcpy(before, r.s.page.px, sizeof(before));
    tap_at(&r, 400, 500);
    CHECK(r.s.tx_on && r.s.tx_n == 0);
    int caret0 = fb_black(&r);
    say(&r, "Hello");
    CHECK(r.s.tx_n == 5 && r.s.tx_cp[0] == 'H' && r.s.tx_cp[4] == 'o');
    CHECK(!memcmp(before, r.s.page.px, sizeof(before)) && r.s.hist.st_cur == 0 && !r.s.dirty); /* typed text is on the screen only */
    CHECK(fb_black(&r) > caret0 + 100);
    /* Backspace takes the last one off, and the screen follows */
    int with5 = fb_black(&r);
    press(&r, APP_KEY_BACKSPACE);
    CHECK(r.s.tx_n == 4 && fb_black(&r) < with5);
    type(&r, "o");
    CHECK(r.s.tx_n == 5 && fb_black(&r) == with5);
    /* layout characters: the keyboard layer sends the X's characters (AZERTY: "é", "à") as utf8 */
    say(&r, " \xc3\xa9\xc3\xa0");
    CHECK(r.s.tx_n == 8 && r.s.tx_cp[6] == 0xe9 && r.s.tx_cp[7] == 0xe0);
    press(&r, APP_KEY_BACKSPACE);
    press(&r, APP_KEY_BACKSPACE);
    press(&r, APP_KEY_BACKSPACE);
    /* Enter: a new line, shown lower */
    press(&r, APP_KEY_ENTER);
    say(&r, "Wo");
    CHECK(r.s.tx_n == 8 && r.s.tx_cp[5] == 10);
    box_t bb;
    int ex, ey;
    tx_draw(0, 0, 0, r.s.tx_x, r.s.tx_y, r.s.tx_cls, r.s.tx_cp, r.s.tx_n, &bb, &ex, &ey);
    CHECK(ey == r.s.tx_y + tx_line_h(r.s.tx_cls) && bb.y1 > r.s.tx_y + tx_line_h(r.s.tx_cls));
    /* Escape ends it: ink, one undo step */
    press(&r, APP_KEY_ESCAPE);
    CHECK(!r.s.tx_on && r.s.hist.st_cur == 1 && r.s.dirty);
    int ink = black_in(&r.s.page, bb.x0, bb.y0, bb.x1, bb.y1);
    CHECK(ink > 200);
    settle(&r);
    CHECK(same_page_and_fb(&r)); /* no caret left on the glass */
    sk_undo(&r.s, r.now);
    CHECK(!memcmp(before, r.s.page.px, sizeof(before)) && r.s.hist.st_cur == 0); /* one step */
    sk_redo(&r.s, r.now);
    CHECK(black_in(&r.s.page, bb.x0, bb.y0, bb.x1, bb.y1) == ink); /* the same pixels again */
    rig_end(&r);
}

static void test_end_by_tap_and_size_and_tone(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    sk_keyboard(&r.s, APP_KBD_BLE, r.now);
    sk_set_tool(&r.s, TL_TEXT, r.now);
    int heights[3];
    for (int k = 0; k < 3; k++) {
        sk_set_size(&r.s, k == 0 ? 8 : k == 1 ? 24 : 40, r.now);
        tap_at(&r, 300, 300 + k * 330);
        say(&r, "Hg");
        heights[k] = tx_line_h(r.s.tx_cls);
        CHECK(r.s.tx_cls == k);
        tap_at(&r, 1500, 1300); /* a tap elsewhere ends it (it does not start another) */
        CHECK(!r.s.tx_on && r.s.hist.st_cur == k + 1);
    }
    CHECK(heights[0] < heights[1] && heights[1] < heights[2]); /* 3 sizes */
    /* a gray tone: the page-fixed dither, fewer black pixels than black */
    rig_t q;
    rig_new(&q, APP_ORIENT_BAR_BOTTOM);
    sk_keyboard(&q.s, APP_KBD_USB, q.now);
    sk_set_tool(&q.s, TL_TEXT, q.now);
    sk_set_size(&q.s, 40, q.now);
    tap_at(&q, 300, 300);
    say(&q, "MM");
    sk_text_end(&q.s, q.now);
    sk_set_shade(&q.s, SH_LIGHT, q.now);
    tap_at(&q, 300, 700);
    say(&q, "MM");
    sk_text_end(&q.s, q.now);
    int a = black_in(&q.s.page, 0, 250, 800, 600), b = black_in(&q.s.page, 0, 650, 800, 1000);
    CHECK(a > 1000 && b > 100 && b < a * 2 / 3);
    rig_end(&q);
    rig_end(&r);
}

static void test_ends_with_tool_change_and_page_leave(void)
{
    rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    sk_keyboard(&r.s, APP_KBD_USB, r.now);
    sk_set_tool(&r.s, TL_TEXT, r.now);
    tap_at(&r, 300, 300);
    say(&r, "ab");
    sk_set_brush(&r.s, BR_PENCIL, r.now); /* another tool: the text is kept as ink */
    CHECK(!r.s.tx_on && r.s.hist.st_cur == 1);
    sk_set_tool(&r.s, TL_TEXT, r.now);
    tap_at(&r, 300, 600);
    say(&r, "cd");
    sk_to_pagemenu(&r.s, r.now); /* the menu: kept too, and saved */
    CHECK(!r.s.tx_on && r.s.hist.st_cur == 2 && !r.s.dirty);
    /* the keyboard is unplugged while typing: kept */
    rig_end(&r);
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    sk_keyboard(&r.s, APP_KBD_USB, r.now);
    sk_set_tool(&r.s, TL_TEXT, r.now);
    tap_at(&r, 300, 300);
    say(&r, "xy");
    sk_keyboard(&r.s, 0, r.now);
    CHECK(!r.s.tx_on && r.s.hist.st_cur == 1);
    /* a very long text is bounded, never a crash */
    sk_keyboard(&r.s, APP_KBD_USB, r.now);
    tap_at(&r, 100, 100);
    for (int i = 0; i < SK_TEXT_MAX + 50; i++) type(&r, "W");
    CHECK(r.s.tx_n == SK_TEXT_MAX);
    press(&r, APP_KEY_ESCAPE);
    CHECK(r.s.hist.st_cur == 2);
    rig_end(&r);
}

int main(void)
{
    test_no_keyboard();
    test_typing();
    test_end_by_tap_and_size_and_tone();
    test_ends_with_tool_change_and_page_leave();
    return CHECK_DONE("test_text");
}
