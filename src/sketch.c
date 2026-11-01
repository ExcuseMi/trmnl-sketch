/* SPDX-License-Identifier: MIT */
#include "sketch.h"
#include <stdlib.h>
#include <string.h>
#include "fmath.h"
#include "sk_cfg.h"

#define TOAST_MS 2500
#define LEGEND_MS 40000
#define RETRY_MS 5000

static void mkg(const sketch_t *s, gfx_t *g) { gfx_init(g, s->fb.px, s->fb.w, s->fb.h, s->fb.stride, s->orient); }
static void mark(sketch_t *s, const box_t *b) { rs_add(&s->pend, b, s->fb.w, s->fb.h); }
static int hit(const box_t *a, const box_t *b) { return !box_empty(a) && !box_empty(b) && a->x0 <= b->x1 && b->x0 <= a->x1 && a->y0 <= b->y1 && b->y0 <= a->y1; }
static void cfg_num(sketch_t *s, const char *key, int v)
{
    char d[8];
    int n = 0, t[8], k = 0;
    if (!s->env.cfg_set) return;
    do t[k++] = v % 10, v /= 10; while (v && k < 7);
    while (k) d[n++] = (char)('0' + t[--k]);
    s->env.cfg_set(s->env.user, key, d, n);
}
static int cfg_int(sketch_t *s, const char *key, int def)
{
    char b[12];
    int n = s->env.cfg_get ? s->env.cfg_get(s->env.user, key, b, 11) : -1, v = 0;
    if (n <= 0 || n > 6) return def;
    for (int i = 0; i < n; i++) {
        if (b[i] < '0' || b[i] > '9') return def;
        v = v * 10 + b[i] - '0';
    }
    return v;
}

/* ---- the page on the screen ---- */

static void whole(box_t *b, const canvas_t *c) { *b = (box_t){ 0, 0, c->w - 1, c->h - 1 }; }
static void tb_paint(sketch_t *s)
{
    gfx_t g;
    mkg(s, &g);
    tb_draw(&g, &s->tb, &s->sty, s->tb_hot);
}

/* The page in box b goes to the screen; the strip of an open toolbar is drawn over it again if the box touched it */
static void fb_put(sketch_t *s, const box_t *b)
{
    cv_copy_box(&s->fb, &s->page, b);
    if (s->tb_on && hit(b, &s->tb_box)) tb_paint(s);
}

void sk_show_page(sketch_t *s)
{
    box_t b;
    whole(&b, &s->page);
    cv_copy_box(&s->fb, &s->page, &b);
    if (s->tb_on) tb_paint(s);
}

void sk_cursor_hide(sketch_t *s)
{
    if (s->tb_ptr) { /* the arrow over the toolbar: the page and the strip come back under it */
        s->tb_ptr = 0;
        fb_put(s, &s->tb_ptr_box);
        mark(s, &s->tb_ptr_box);
    }
    if (!s->cursor) return;
    if (s->mode >= SK_GALLERY) { /* the menus' pointer: the clean screen comes back */
        if (s->ui.px) cv_copy_box(&s->fb, &s->ui, &s->cur_box);
        mark(s, &s->cur_box);
        s->cursor = 0;
        return;
    }
    int o = s->cr + BR_CUR_OUT;
    box_t b = { s->cx - o, s->cy - o, s->cx + o, s->cy + o };
    box_clip(&b, s->fb.w, s->fb.h);
    fb_put(s, &b);
    mark(s, &b);
    s->cursor = 0;
}

/* The framebuffer box of toolbar item i */
static void tb_item_box(sketch_t *s, int i, box_t *b)
{
    gfx_t g;
    mkg(s, &g);
    ui_fb_box(&g, s->tb.cx[i], s->tb.cy, s->tb.cx[i] + s->tb.cell - 1, s->tb.cy + s->tb.cell - 1, b);
    box_clip(b, s->fb.w, s->fb.h);
}

/* The pen over the strip: the arrow pointer, and the item under it framed. Only the old and the new item (and the
   arrow's boxes) are refreshed; the strip is repainted whole in the framebuffer, which changes nothing else. */
static void tb_pointer(sketch_t *s, int ux, int uy)
{
    int item = tb_hit(&s->tb, ux, uy), old = s->tb_hot;
    int fx, fy;
    app_orient_to_fb(s->orient, s->fb.w, s->fb.h, ux, uy, &fx, &fy);
    if (s->tb_ptr && item == old && (fx - s->cx) * (fx - s->cx) + (fy - s->cy) * (fy - s->cy) < 9) return;
    s->tb_hot = item;
    sk_cursor_hide(s);
    tb_paint(s);
    box_t b;
    if (item != old)
        for (int k = 0; k < 2; k++) {
            int i = k ? item : old;
            if (i >= 0) tb_item_box(s, i, &b), mark(s, &b);
        }
    ui_arrow(&s->fb, s->orient, ux, uy, &b);
    box_clip(&b, s->fb.w, s->fb.h);
    if (s->note_on && hit(&b, &s->note)) { /* never over a note: restoring the page would eat it */
        fb_put(s, &b);
        return;
    }
    s->tb_ptr = 1, s->tb_ptr_box = b, s->cx = fx, s->cy = fy;
    mark(s, &b);
}

/* The pen left the strip: the pointer and the frame go */
static void tb_pointer_off(sketch_t *s)
{
    int old = s->tb_hot;
    if (!s->tb_ptr && old < 0) return;
    s->tb_hot = -1;
    sk_cursor_hide(s);
    if (old >= 0 && s->tb_on) {
        box_t b;
        tb_paint(s);
        tb_item_box(s, old, &b);
        mark(s, &b);
    }
}

/* The brush that draws now: the pen's button inverts the eraser (held on an ink brush: erase; on the eraser: ink) */
static int eff_brush(const sketch_t *s)
{
    if (!s->barrel) return s->sty.brush;
    return s->sty.brush == BR_ERASER ? s->last_ink : BR_ERASER;
}

static void cursor_show(sketch_t *s, int fx, int fy)
{
    int br = eff_brush(s), r = brush_cursor_r(br, s->sty.size);
    box_t want = { fx - r - BR_CUR_OUT, fy - r - BR_CUR_OUT, fx + r + BR_CUR_OUT, fy + r + BR_CUR_OUT };
    if (s->cursor && s->cr == r && s->cb == br && (fx - s->cx) * (fx - s->cx) + (fy - s->cy) * (fy - s->cy) < 9) return;
    sk_cursor_hide(s);
    if (s->note_on && hit(&want, &s->note)) return; /* never over a note: restoring the page would eat it */
    if (s->tb_on && hit(&want, &s->tb_box)) return;  /* nor over the toolbar */
    box_t b;
    ui_cursor(&s->fb, fx, fy, br, s->sty.size, &b);
    mark(s, &b);
    s->cursor = 1, s->cx = fx, s->cy = fy, s->cr = r, s->cb = br;
}

void sk_note_hide(sketch_t *s)
{
    if (!s->note_on) return;
    fb_put(s, &s->note);
    mark(s, &s->note);
    s->note_on = s->legend = 0;
}

static void note_set(sketch_t *s, const box_t *b, uint32_t until)
{
    s->note = *b;
    box_clip(&s->note, s->fb.w, s->fb.h);
    s->note_on = 1;
    s->note_until = until;
    mark(s, &s->note);
}

void sk_toast(sketch_t *s, const char *text, uint32_t now)
{
    if (s->mode != SK_DRAW) return;
    sk_cursor_hide(s);
    sk_note_hide(s);
    gfx_t g;
    box_t b;
    mkg(s, &g);
    ui_toast(&g, text, s->tb_on && s->tb.top ? s->tb.y1 + 14 : 24, &b);
    note_set(s, &b, now + TOAST_MS);
}

/* a change of the page in box b reaches the screen */
static void page_changed(sketch_t *s, const box_t *b, uint32_t now)
{
    sk_cursor_hide(s);
    if (s->note_on && hit(b, &s->note)) sk_note_hide(s);
    fb_put(s, b);
    mark(s, b);
    s->dirty = 1;
    s->save_at = now + SK_AUTOSAVE_MS;
}

/* ---- saving ---- */

static int do_save(sketch_t *s, uint32_t now)
{
    if (s->bad_pending) { /* the unreadable file stays, under another name */
        if (pg_keep_bad(&s->pg, s->cur) < 0 && pg_index(&s->pg, s->cur) >= 0) return s->save_failed = 1, s->save_at = now + RETRY_MS, PG_IO;
        s->bad_pending = 0;
    }
    int rc = pg_save(&s->pg, s->cur, &s->page);
    if (rc) return s->save_failed = 1, s->save_at = now + RETRY_MS, rc;
    s->dirty = s->save_failed = 0;
    gal_invalidate(&s->cache, s->cur);
    return 0;
}

void sk_save_now(sketch_t *s)
{
    if (s->dirty) do_save(s, s->last_push);
}

/* ---- pages ---- */

int sk_is_blank(const canvas_t *c)
{
    int rb = (c->w + 7) / 8;
    for (int y = 0; y < c->h; y++)
        for (int x = 0; x < rb; x++)
            if (c->px[y * c->stride + x] != 0xff) return 0;
    return 1;
}

void sk_open_page(sketch_t *s, int num, uint32_t now)
{
    s->cur = num;
    s->bad_pending = 0;
    int rc = pg_load(&s->pg, num, &s->page);
    if (rc == PG_MISSING) cv_fill(&s->page, 1);
    if (rc == PG_BAD) s->bad_pending = 1;
    hist_reset(&s->hist);
    s->cursor = s->note_on = s->legend = s->stroke = 0;
    sk_show_page(s);
    s->want_full = 1;
    s->dirty = 0;
    cfg_num(s, "cur", num);
    (void)now;
}

static const char *page_text(sketch_t *s, char *buf)
{
    int pos = 0;
    for (int i = 0; i < s->pg.n; i++)
        if (s->pg.nums[i] <= s->cur) pos = i + 1;
    int v[2] = { pos, s->pg.n }, n = 0;
    const char *pre = "Page ";
    while (*pre) buf[n++] = *pre++;
    for (int k = 0; k < 2; k++) {
        char d[8];
        int m = 0, x = v[k];
        do d[m++] = (char)('0' + x % 10), x /= 10; while (x);
        while (m) buf[n++] = d[--m];
        if (k == 0) buf[n++] = ' ', buf[n++] = 'o', buf[n++] = 'f', buf[n++] = ' ';
    }
    buf[n] = 0;
    return buf;
}

void sk_make_page(sketch_t *s, uint32_t now);

int sk_leave_page(sketch_t *s, uint32_t now) /* saves the page on screen first; 0 when it could not */
{
    hist_end(&s->hist);
    s->stroke = 0;
    if (s->dirty && do_save(s, now)) return 0;
    return 1;
}

void sk_goto_page(sketch_t *s, int index, uint32_t now)
{
    if (!sk_leave_page(s, now)) return sk_toast(s, "Could not save this page", now);
    pg_scan(&s->pg);
    if (s->pg.n == 0) return sk_make_page(s, now);
    if (index < 0 || index >= s->pg.n) return sk_toast(s, index < 0 ? "First page" : "Last page", now);
    sk_open_page(s, s->pg.nums[index], now);
    char b[24];
    sk_toast(s, page_text(s, b), now);
}

static int cur_pos(const sketch_t *s, int *present) /* where the page on screen is in the list */
{
    int i = pg_index(&s->pg, s->cur);
    *present = i >= 0;
    if (i >= 0) return i;
    i = 0;
    while (i < s->pg.n && s->pg.nums[i] < s->cur) i++;
    return i;
}

static void step_page(sketch_t *s, int dir, uint32_t now)
{
    if (!sk_leave_page(s, now)) return sk_toast(s, "Could not save this page", now);
    pg_scan(&s->pg);
    int present, pos = cur_pos(s, &present);
    int target = dir > 0 ? (present ? pos + 1 : pos) : pos - 1;
    if (s->pg.n == 0 || target < 0 || target >= s->pg.n) {
        if (s->pg.n == 0) return sk_make_page(s, now);
        return sk_toast(s, target < 0 ? "First page" : "Last page", now);
    }
    sk_open_page(s, s->pg.nums[target], now);
    char b[24];
    sk_toast(s, page_text(s, b), now);
}

/* A new page only from a page with ink: no run of empty pages (owner 2026-11-01) */
void sk_new_page(sketch_t *s, uint32_t now)
{
    if (sk_is_blank(&s->page)) return sk_toast(s, "This page is still empty", now);
    sk_make_page(s, now);
}

void sk_make_page(sketch_t *s, uint32_t now)
{
    if (!sk_leave_page(s, now)) return sk_toast(s, "Could not save this page", now);
    pg_scan(&s->pg);
    int num = pg_next_num(&s->pg);
    if (!num) return sk_toast(s, "No more pages", now);
    s->cur = num;
    cv_fill(&s->page, 1);
    s->bad_pending = 0;
    hist_reset(&s->hist);
    s->cursor = s->note_on = s->legend = 0;
    sk_show_page(s);
    s->want_full = 1;
    cfg_num(s, "cur", num);
    if (pg_save(&s->pg, num, &s->page)) s->dirty = 1, s->save_at = now + RETRY_MS; /* listed once it is written */
    char b[24];
    sk_toast(s, page_text(s, b), now);
}

/* ---- actions ---- */

void sk_undo(sketch_t *s, uint32_t now)
{
    box_t b;
    s->stroke = 0;
    if (hist_undo(&s->hist, &b)) page_changed(s, &b, now);
    else sk_toast(s, "Nothing to undo", now);
}

void sk_redo(sketch_t *s, uint32_t now)
{
    box_t b;
    s->stroke = 0;
    if (hist_redo(&s->hist, &b)) page_changed(s, &b, now);
    else sk_toast(s, "Nothing to redo", now);
}

void sk_clear(sketch_t *s, uint32_t now)
{
    hist_end(&s->hist);
    s->stroke = 0;
    if (sk_is_blank(&s->page)) return sk_toast(s, "The page is empty", now);
    if (s->dirty) do_save(s, now);
    pg_save_as(&s->pg, s->cur, "-cleared.png", &s->page); /* what was on the page stays on the flash */
    box_t b;
    hist_clear(&s->hist, &b);
    sk_cursor_hide(s);
    sk_note_hide(s);
    sk_show_page(s);
    s->want_full = 1;
    s->dirty = 1;
    s->save_at = now + SK_AUTOSAVE_MS;
    sk_toast(s, "Cleared. Undo brings it back", now);
}

static void toggle_eraser(sketch_t *s, uint32_t now)
{
    if (s->sty.brush == BR_ERASER) s->sty.brush = (uint8_t)s->last_ink;
    else s->last_ink = s->sty.brush, s->sty.brush = BR_ERASER;
    if (s->tb_on) tb_paint(s), mark(s, &s->tb_box);
    sk_toast(s, s->sty.brush == BR_ERASER ? "Eraser on" : "Eraser off", now);
}

/* ---- brush, shade, size and the toolbar ---- */

static void picked(sketch_t *s, const char *text, uint32_t now)
{
    if (s->tb_on) tb_paint(s), mark(s, &s->tb_box);
    sk_toast(s, text, now);
}

void sk_set_brush(sketch_t *s, int brush, uint32_t now)
{
    if (brush < 0 || brush >= BR_N) return;
    if (brush != BR_ERASER) s->last_ink = brush, cfg_num(s, "brush", brush);
    s->sty.brush = (uint8_t)brush;
    picked(s, brush_name(brush), now);
}

void sk_set_shade(sketch_t *s, int shade, uint32_t now)
{
    if (shade < 0 || shade >= SH_N) return;
    s->sty.shade = (uint8_t)shade;
    cfg_num(s, "shade", shade);
    picked(s, shade_name(shade), now);
}

void sk_set_size(sketch_t *s, int size, uint32_t now)
{
    static const char *const name[SK_SIZES] = { "Size: thin", "Size: medium", "Size: thick" };
    if (size < 0 || size >= SK_SIZES) return;
    s->sty.size = (uint8_t)size;
    cfg_num(s, "size", size);
    picked(s, name[size], now);
}

static void tb_flash(sketch_t *s) { box_union(&s->flash, &s->tb_box); }

static void tb_place(sketch_t *s) /* the strip for the screen in the user's frame, and its box in the framebuffer */
{
    gfx_t g;
    box_t b;
    mkg(s, &g);
    tb_layout(&s->tb, s->orient, g.SW, g.SH);
    ui_fb_box(&g, s->tb.x0, s->tb.y0, s->tb.x1, s->tb.y1, &b);
    b.x0 &= ~7;
    b.x1 |= 7;
    box_clip(&b, s->fb.w, s->fb.h);
    s->tb_box = b;
}

void sk_toolbar(sketch_t *s, uint32_t now)
{
    (void)now;
    if (s->mode != SK_DRAW) return;
    sk_cursor_hide(s);
    if (s->tb_on) { /* the page under the strip comes back as it was */
        s->tb_on = s->tb_hold = 0;
        s->tb_hot = -1;
        cv_copy_box(&s->fb, &s->page, &s->tb_box);
        return tb_flash(s);
    }
    sk_note_hide(s);
    hist_end(&s->hist);
    s->stroke = 0;
    tb_place(s);
    s->tb_on = 1;
    tb_paint(s);
    tb_flash(s);
}

static void tb_press(sketch_t *s, int item, uint32_t now)
{
    if (item < 0) return;
    if (item < TI_SHADE) sk_set_brush(s, item - TI_BRUSH, now);
    else if (item < TI_SIZE) sk_set_shade(s, item - TI_SHADE, now);
    else if (item < TI_SIZE + SK_SIZES) sk_set_size(s, item - TI_SIZE, now);
    else if (item == TI_UNDO) sk_undo(s, now);
    else if (item == TI_REDO) sk_redo(s, now);
    else sk_clear(s, now);
}

/* ---- input ---- */

/* A point of the open stroke: drawn on the page, then on the screen */
static void stroke_add(sketch_t *s, int fx, int fy, int p, uint32_t now)
{
    box_t dirty;
    box_clear(&dirty);
    hist_add(&s->hist, fx, fy, p, &dirty);
    if (!box_empty(&dirty)) {
        fb_put(s, &dirty);
        mark(s, &dirty);
    }
    s->px = s->lx, s->py = s->ly, s->lx = fx, s->ly = fy, s->lp = p;
    s->dirty = 1;
    s->save_at = now + SK_AUTOSAVE_MS;
}

/* The brush pen's end tapers out along the last movement */
static void stroke_tail(sketch_t *s, uint32_t now)
{
    if (!brush_tail(s->stk.brush) || (s->lx == s->px && s->ly == s->py)) return;
    float dx = (float)(s->lx - s->px), dy = (float)(s->ly - s->py), len = fm_sqrt(dx * dx + dy * dy);
    int p = s->lp, x = s->lx, y = s->ly;
    for (int k = 1; k <= 3; k++)
        stroke_add(s, x + fm_round(dx / len * 4.0f * (float)k), y + fm_round(dy / len * 4.0f * (float)k), p * (3 - k) / 3, now);
}

void sk_pen(sketch_t *s, unsigned flags, unsigned x, unsigned y, unsigned pressure, uint32_t now)
{
    s->barrel = (flags & (APP_PEN_BARREL | APP_PEN_BARREL2)) != 0;
    int fx, fy, inside = pm_map(&s->map, x, y, &fx, &fy);
    if (s->mode != SK_DRAW) return sk_menu_pen(s, flags, fx, fy, inside, now);
    int tip = (flags & APP_PEN_TIP) != 0, range = (flags & (APP_PEN_IN_RANGE | APP_PEN_TIP)) != 0;
    int on_bar = 0, ux, uy;
    gal_from_fb(s->orient, s->fb.w, s->fb.h, fx, fy, &ux, &uy);
    if (s->tb_on && inside) on_bar = tb_in(&s->tb, ux, uy);
    if (!tip) s->tb_hold = 0;
    if (tip && on_bar) { /* a press on the strip picks, never inks; a stroke dragged onto it just ends */
        int was = s->stroke;
        if (was) stroke_tail(s, now), hist_end(&s->hist), s->stroke = 0;
        if (!s->tb_hold) {
            s->tb_hold = 1;
            sk_cursor_hide(s);
            if (!was) tb_press(s, tb_hit(&s->tb, ux, uy), now);
        }
        return;
    }
    if (tip && s->tb_hold) return; /* dragged off the strip: no ink until the pen lifts */
    if (tip && inside) {
        if (!s->stroke) {
            int br = eff_brush(s);
            sk_cursor_hide(s);
            sk_note_hide(s);
            s->stk = s->sty;
            s->stk.brush = (uint8_t)br;
            s->stroke_erase = brush_is_eraser(br);
            hist_begin_style(&s->hist, s->stroke_erase ? HK_ERASE : HK_INK, &s->stk);
            s->stroke = 1;
            s->lx = s->px = fx, s->ly = s->py = fy;
            s->pressure = brush_tail(br) ? pressure / 4 : pressure; /* the brush pen swells from nothing */
        } else {
            s->pressure = (s->pressure + pressure) / 2;
        }
        stroke_add(s, fx, fy, brush_param(s->stk.brush, s->stk.size, s->pressure), now);
        return;
    }
    if (s->stroke) {
        stroke_tail(s, now);
        hist_end(&s->hist);
        s->stroke = 0;
        s->save_at = now + SK_AUTOSAVE_MS;
    }
    if (s->tb_on && range && !tip && inside && on_bar) { /* the arrow over the strip, not the brush's cursor */
        s->hover_valid = 0;
        return tb_pointer(s, ux, uy);
    }
    tb_pointer_off(s);
    s->hover_valid = range && !tip && inside && !on_bar;
    s->hover_x = fx, s->hover_y = fy;
    if (s->hover_valid) cursor_show(s, fx, fy);
    else sk_cursor_hide(s);
}

void sk_pen_key(sketch_t *s, unsigned keys, unsigned changed, uint32_t now)
{
    unsigned down = keys & changed;
    if (s->mode != SK_DRAW) {
        if (down) sk_menu_key(s, down, now);
        return;
    }
    for (int b = 0; b < 8; b++) {
        if (!(down >> b & 1)) continue;
        switch (b) {
        case 0: sk_undo(s, now); break;
        case 1: sk_redo(s, now); break;
        case 2: toggle_eraser(s, now); break;
        case 3: sk_toolbar(s, now); break;
        case 4: sk_clear(s, now); break;
        case 5: sk_new_page(s, now); break;
        case 6: step_page(s, -1, now); break;
        default: step_page(s, 1, now); break;
        }
    }
}

void sk_touch(sketch_t *s, int zone, int gesture, uint32_t now)
{
    if (s->mode == SK_SHARE) { /* any touch ends the phone page */
        s->share_req = -1;
        s->mode = SK_GALLERY;
        sk_screen_render(s);
        return;
    }
    if (s->mode >= SK_GALLERY) return sk_menu_touch(s, zone, gesture, now);
    if (gesture == APP_GESTURE_SWIPE_LEFT) step_page(s, 1, now);
    else if (gesture == APP_GESTURE_SWIPE_RIGHT) step_page(s, -1, now);
    else if (gesture == APP_GESTURE_HOLD && zone == APP_TOUCH_MIDDLE) sk_clear(s, now);
    else if (gesture == APP_GESTURE_TAP && zone == APP_TOUCH_LEFT) sk_undo(s, now);
    else if (gesture == APP_GESTURE_TAP && zone == APP_TOUCH_RIGHT) sk_redo(s, now);
    else if (gesture == APP_GESTURE_TAP && zone == APP_TOUCH_MIDDLE) sk_to_gallery(s, now);
}

void sk_share_update(sketch_t *s, int start_rc, const app_webshare_status_t *st)
{
    if (s->mode != SK_SHARE) return;
    share_view_t v;
    share_view(&v, start_rc, st);
    v.settings = s->share_kind == 2;
    if (v.settings && v.up) sk_cpy(v.note, sizeof(v.note), "Save the form on your phone, then touch the bar.");
    if (share_view_same(&v, &s->share_shown)) return;
    s->share_shown = s->share = v;
    gfx_t g;
    mkg(s, &g);
    ui_share(&g, &s->bar, &v);
    s->want_full = 1;
}

/* ---- time ---- */

void sk_tick(sketch_t *s, uint32_t now)
{
    if (s->note_on && !s->legend && (int32_t)(now - s->note_until) >= 0) {
        sk_note_hide(s);
        if (s->hover_valid) cursor_show(s, s->hover_x, s->hover_y);
    }
    if (s->legend && (int32_t)(now - s->note_until) >= 0) sk_note_hide(s);
    if (s->dirty && !s->stroke && (int32_t)(now - s->save_at) >= 0) do_save(s, now);
}

int sk_flush(sketch_t *s, uint32_t now, rect_t *out)
{
    if (s->want_full) {
        s->want_full = 0;
        if (s->mode >= SK_GALLERY && s->ui.px) { /* the screen as it is, then the pointer on it */
            memcpy(s->fb.px, s->ui.px, (size_t)s->fb.stride * (size_t)s->fb.h);
            s->cursor = 0;
            sk_menu_cursor(s);
        }
        rs_init(&s->pend);
        box_clear(&s->flash);
        s->last_push = now;
        s->pushed_px += (uint32_t)(s->fb.w * s->fb.h);
        s->pushed_rects++;
        return SK_FULL;
    }
    if (!box_empty(&s->flash)) { /* the toolbar came or went: its strip alone, clean */
        out[0] = (rect_t){ (int16_t)s->flash.x0, (int16_t)s->flash.y0, (int16_t)(s->flash.x1 - s->flash.x0 + 1), (int16_t)(s->flash.y1 - s->flash.y0 + 1), 1 };
        box_clear(&s->flash);
        s->pushed_px += (uint32_t)(out[0].w * out[0].h);
        s->pushed_rects++;
        s->last_push = now;
        return 1;
    }
    if (!s->pend.n || (uint32_t)(now - s->last_push) < SK_BATCH_MS) return 0;
    int n = rs_take(&s->pend, out);
    for (int i = 0; i < n; i++) s->pushed_px += (uint32_t)(out[i].w * out[i].h);
    s->pushed_rects += (uint32_t)n;
    s->last_push = now;
    return n;
}

uint32_t sk_next_due(const sketch_t *s, uint32_t now)
{
    uint32_t due = 1000000;
    if (s->want_full || !box_empty(&s->flash)) return 0;
    if (s->pend.n) {
        int32_t d = (int32_t)(s->last_push + SK_BATCH_MS - now);
        due = d < 0 ? 0 : (uint32_t)d;
    }
    if (s->dirty && !s->stroke) {
        int32_t d = (int32_t)(s->save_at - now);
        if ((uint32_t)(d < 0 ? 0 : d) < due) due = d < 0 ? 0 : (uint32_t)d;
    }
    if (s->note_on) {
        int32_t d = (int32_t)(s->note_until - now);
        if ((uint32_t)(d < 0 ? 0 : d) < due) due = d < 0 ? 0 : (uint32_t)d;
    }
    return due;
}

/* ---- set up ---- */

void sk_set_tablet(sketch_t *s, uint32_t width_um, uint32_t height_um, int connected)
{
    s->um_w = width_um, s->um_h = height_um, s->tablet = connected;
    pm_init(&s->map, s->orient, s->fb.w, s->fb.h, width_um, height_um);
}

void sk_set_orient(sketch_t *s, int orient, const app_touchbar_t *bar)
{
    s->orient = orient;
    s->bar = *bar;
    pm_init(&s->map, orient, s->fb.w, s->fb.h, s->um_w, s->um_h);
    s->hover_valid = 0;
    if (s->mode == SK_DRAW) {
        sk_cursor_hide(s);
        sk_note_hide(s);
        if (s->tb_on) tb_place(s);
        sk_show_page(s); /* nothing but the page (and the toolbar, if it is up) is left on the screen */
    } else if (s->mode >= SK_GALLERY) {
        sk_screen_render(s);
    } else {
        memset(&s->share_shown, 0xff, sizeof(s->share_shown));
    }
    s->want_full = 1;
}

int sk_init(sketch_t *s, const sk_env_t *env, uint8_t *fb, int w, int h, int stride, int orient, const app_touchbar_t *bar)
{
    memset(s, 0, sizeof(*s));
    s->tb_hot = -1;
    s->env = *env;
    s->fb = (canvas_t){ fb, w, h, stride };
    size_t bytes = (size_t)stride * (size_t)h;
    uint8_t *pg = malloc(bytes), *base = malloc(bytes);
    hpt_t *pts = malloc(sizeof(hpt_t) * SK_HIST_POINTS);
    hstroke_t *st = malloc(sizeof(hstroke_t) * SK_HIST_STROKES);
    if (!pg || !base || !pts || !st) return free(pg), free(base), free(pts), free(st), APP_ENOMEM;
    s->page = (canvas_t){ pg, w, h, stride };
    cv_fill(&s->page, 1);
    hist_init(&s->hist, &s->page, base, pts, SK_HIST_POINTS, st, SK_HIST_STROKES);
    pg_init(&s->pg, env->fs);
    pg_scan(&s->pg);
    s->sty.size = (uint8_t)(cfg_int(s, "size", 1) % SK_SIZES);
    s->sty.shade = (uint8_t)(cfg_int(s, "shade", 0) % SH_N);
    s->sty.brush = (uint8_t)(cfg_int(s, "brush", 0) % BR_N);
    if (s->sty.brush == BR_ERASER) s->sty.brush = BR_FINELINER;
    s->last_ink = s->sty.brush;
    box_clear(&s->flash);
    s->orient = orient;
    s->bar = *bar;
    pm_init(&s->map, orient, w, h, 0, 0);
    int want = cfg_int(s, "cur", 0);
    int num = pg_index(&s->pg, want) >= 0 ? want : s->pg.n ? s->pg.nums[s->pg.n - 1] : 1;
    s->cur = num;
    if (s->pg.n == 0) {
        pg_save(&s->pg, num, &s->page); /* page 1 exists from the start */
    } else {
        sk_open_page(s, num, 0);
    }
    sk_show_page(s);
    s->want_full = 1;
    gfx_t g;
    box_t b;
    mkg(s, &g);
    ui_legend(&g, &b);
    s->legend = 1;
    note_set(s, &b, LEGEND_MS);
    return 0;
}

void sk_free(sketch_t *s)
{
    free(s->ui.px);
    gal_cache_free(&s->cache);
    free(s->page.px);
    free(s->hist.base.px);
    free(s->hist.pts);
    free(s->hist.st);
    s->page.px = 0;
}
