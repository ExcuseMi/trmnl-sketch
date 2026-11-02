/* The Text tool: a tap on the page puts the caret there, a USB or Bluetooth keyboard types (characters arrive mapped for the
   X's layout, keyboard_chars(1)); Backspace, Enter, Escape. Shown on the screen only until it ends (Escape, a tap
   elsewhere, another tool); then it becomes ink in one undo step (HK_TEXT: the points are the code points). SPDX-License-Identifier: MIT */
#include "sketch.h"
#include "font.h"
#include "textdraw.h"

#define TX_TAP_PX 24
#define CARET_W 3

static int cls_of(const sketch_t *s) { return tx_class(s->sty.size); }

/* The text and its caret drawn on the screen (not the page), the old drawing taken away first */
static void redraw(sketch_t *s)
{
    if (!box_empty(&s->tx_box)) sk_fb_restore(s, &s->tx_box);
    box_t bb;
    int ex, ey, cls = s->tx_cls;
    tx_draw(&s->fb, 0, s->sty.shade, s->tx_x, s->tx_y, cls, s->tx_cp, s->tx_n, &bb, &ex, &ey);
    int lh = tx_line_h(cls);
    for (int y = ey; y < ey + lh; y++) cv_span_s(&s->fb, y, ex, ex + CARET_W - 1, 0, SH_BLACK);
    box_add(&bb, ex, ey), box_add(&bb, ex + CARET_W - 1, ey + lh - 1);
    box_clip(&bb, s->fb.w, s->fb.h);
    s->tx_box = bb;
    sk_mark(s, &bb);
}

/* Ends the text: it becomes ink, or nothing when empty */
void sk_text_end(sketch_t *s, uint32_t now)
{
    if (!s->tx_on) return;
    s->tx_on = 0;
    if (!box_empty(&s->tx_box)) sk_fb_restore(s, &s->tx_box);
    box_clear(&s->tx_box);
    int n = 0;
    for (int i = 0; i < s->tx_n; i++) n += s->tx_cp[i] != 10;
    if (!n) return;
    bstyle_t st = { BR_FINELINER, s->sty.shade, s->sty.size, 0, 0 };
    hpt_t *slots = hist_text_begin(&s->hist, (uint32_t)s->tx_n + 1, &st);
    if (!slots) return sk_toast(s, "Not enough room to keep this text", now);
    slots[0] = (hpt_t){ (int16_t)s->tx_x, (int16_t)s->tx_y, (uint8_t)s->tx_cls, 0 };
    for (int i = 0; i < s->tx_n; i++) slots[1 + i] = (hpt_t){ (int16_t)s->tx_cp[i], 0, 0, 0 };
    box_t bb;
    tx_draw(0, 0, 0, s->tx_x, s->tx_y, s->tx_cls, s->tx_cp, s->tx_n, &bb, 0, 0);
    box_clip(&bb, s->page.w, s->page.h);
    hist_fill_end(&s->hist, (uint32_t)s->tx_n + 1, &bb);
    sk_page_changed(s, &bb, now);
}

static void begin(sketch_t *s, int fx, int fy)
{
    sk_cursor_hide(s);
    sk_note_hide(s);
    s->tx_on = 1, s->tx_n = 0, s->tx_x = fx, s->tx_y = fy, s->tx_cls = cls_of(s);
    box_clear(&s->tx_box);
    redraw(s);
}

/* The pen with the Text tool: a tap (down and up without moving) starts the text there, or ends the one under way */
int sk_text_pen(sketch_t *s, int tip, int inside, int fx, int fy, uint32_t now)
{
    if (s->sty.tool != TL_TEXT) return 0;
    if (tip && !s->tx_down) {
        if (!inside || s->stroke) return 0;
        s->tx_down = 1, s->tx_moved = 0, s->tx_dx = fx, s->tx_dy = fy;
        return 1;
    }
    if (tip) {
        if (fx - s->tx_dx > TX_TAP_PX || s->tx_dx - fx > TX_TAP_PX || fy - s->tx_dy > TX_TAP_PX || s->tx_dy - fy > TX_TAP_PX) s->tx_moved = 1;
        return 1;
    }
    if (!s->tx_down) return 0;
    s->tx_down = 0;
    if (s->tx_moved) return 0;
    if (s->tx_on) return sk_text_end(s, now), 0; /* a tap elsewhere ends it */
    if (!s->kbd) return sk_toast(s, "Connect a keyboard", now), s->kbd_req = 1, 0;
    int y = s->tx_dy - tx_line_h(cls_of(s)) / 2; /* the I-beam was over the line: its middle is where the pen is */
    begin(s, s->tx_dx, y < 0 ? 0 : y);
    return 0;
}

void sk_keyboard(sketch_t *s, uint32_t bits, uint32_t now)
{
    s->kbd = bits;
    if (!bits) sk_text_end(s, now); /* the keyboard went: what is typed is kept */
}

void sk_key(sketch_t *s, int down, unsigned key, unsigned mods, unsigned hid, const char *utf8, uint32_t now)
{
    if (hid == 0xE1 || hid == 0xE5) { /* a Shift key: shapes read it as the constraint */
        s->mods = down ? (mods | APP_MOD_SHIFT) : (mods & ~(unsigned)APP_MOD_SHIFT);
        return;
    }
    s->mods = mods;
    s->last_act = now;
    if (!down || !s->tx_on || s->mode != SK_DRAW) return;
    if (key == APP_KEY_ESCAPE) return sk_text_end(s, now);
    if (key == APP_KEY_BACKSPACE || key == APP_KEY_DELETE) {
        if (s->tx_n) s->tx_n--, redraw(s);
        return;
    }
    if (key == APP_KEY_ENTER) {
        if (s->tx_n < SK_TEXT_MAX) s->tx_cp[s->tx_n++] = 10, redraw(s);
        return;
    }
    if (key != APP_KEY_CHAR || !utf8 || !utf8[0] || (mods & (APP_MOD_CTRL | APP_MOD_ALT | APP_MOD_META))) return;
    int add = 0;
    for (const char *p = utf8; s->tx_n + add < SK_TEXT_MAX;) {
        uint32_t cp = font_next_cp(&p);
        if (!cp) break;
        if (cp >= 32 && cp < 0x10000) s->tx_cp[s->tx_n + add++] = (uint16_t)cp;
    }
    if (add) s->tx_n += add, redraw(s);
}
