/* SPDX-License-Identifier: MIT */
/* The gallery, the action list, the print screens and the pen and touch bar in them: the controller of the menu screens
   (sketch.c holds drawing, pages and saving). They run in the panel's 1-bit mode: the pointer and the highlight are
   partial refreshes, a full one only when a screen opens or closes. */
#include "sketch.h"
#include <stdlib.h>
#include <string.h>
#include "sk_cfg.h"

enum { MBLK_ = GFX_BLACK, MWHT_ = GFX_WHITE };

/* ---- the gallery and the menus ---- */

/* copies s into d (cap bytes) cut to fit; no strcpy/strncpy: the X's loader does not export them */
void sk_cpy(char *d, size_t cap, const char *s)
{
    size_t n = 0;
    while (s[n] && n + 1 < cap) d[n] = s[n], n++;
    d[n] = 0;
}

static int gcount(const sketch_t *s) { return s->pg.n + 1; }
static int gnum(const sketch_t *s, int idx) { return idx <= 0 || idx > s->pg.n ? 0 : s->pg.nums[s->pg.n - idx]; }
static int gidx_of(const sketch_t *s, int num)
{
    int i = pg_index(&s->pg, num);
    return i < 0 ? 0 : s->pg.n - i;
}
static int user_w(const sketch_t *s) { return s->orient == APP_ORIENT_BAR_LEFT || s->orient == APP_ORIENT_BAR_RIGHT ? s->fb.h : s->fb.w; }
static int user_h(const sketch_t *s) { return s->orient == APP_ORIENT_BAR_LEFT || s->orient == APP_ORIENT_BAR_RIGHT ? s->fb.w : s->fb.h; }

static void printer_addr(sketch_t *s, char *out, int cap)
{
    int n = s->env.cfg_get ? s->env.cfg_get(s->env.user, "printer", out, cap - 1) : -1;
    if (n <= 0) n = (int)strlen(SK_PRINTER_DEFAULT), memcpy(out, SK_PRINTER_DEFAULT, (size_t)n);
    out[n] = 0;
}

/* The thumbnail of a page: cached, else made from the page on screen or from its file (read through `ui` as scratch) */
static const uint8_t *thumb_for(sketch_t *s, int num)
{
    const uint8_t *t = gal_find(&s->cache, num);
    if (t) return t;
    if (num == s->cur) return gal_make(&s->cache, num, &s->page);
    if (pg_load(&s->pg, num, &s->ui) == PG_MISSING) return 0;
    return gal_make(&s->cache, num, &s->ui);
}

static void blit_add(sketch_t *s, int num, int x, int y)
{
    if (s->blit_n < SK_BLITS && thumb_for(s, num)) s->blit[s->blit_n].num = num, s->blit[s->blit_n].x = x, s->blit[s->blit_n].y = y, s->blit_n++;
}

static void ui_g(sketch_t *s, gfx_t *g) { gfx_init(g, s->ui.px, s->ui.w, s->ui.h, s->ui.stride, s->orient); }

static void split_msg(const char *m, char a[64], char b[64])
{
    size_t n = strlen(m), cut = n > 46 ? 46 : n;
    if (n > 46)
        while (cut > 20 && m[cut] != ' ') cut--;
    memset(a, 0, 64), memset(b, 0, 64);
    memcpy(a, m, cut > 63 ? 63 : cut);
    if (cut < n) sk_cpy(b, 64, m + cut + (m[cut] == ' '));
}

static int gpages(const sketch_t *s) { return (gcount(s) + s->L.per - 1) / s->L.per; }
/* The control as drawn on this screen (CTL_NONE: the screen has none like it) */
static int ctl_live(const sketch_t *s, int c)
{
    if (!s->ui.px || (s->mode != SK_GALLERY && s->mode != SK_ACTIONS && s->mode != SK_CONFIRM)) return 0;
    if (c == CTL_PREV) return s->mode == SK_GALLERY && s->sel / s->L.per > 0;
    if (c == CTL_NEXT) return s->mode == SK_GALLERY && s->sel / s->L.per + 1 < gpages(s);
    return c == CTL_UP || c == CTL_SEL || c == CTL_DOWN;
}

/* The user-frame box of a control, for drawing and for the pen */
static int ctl_box(const sketch_t *s, int c, int *x0, int *y0, int *x1, int *y1)
{
    if (c == CTL_PREV || c == CTL_NEXT) return ui_nav_box(&s->L, c, x0, y0, x1, y1), 1;
    if (c < CTL_UP || c > CTL_DOWN) return 0;
    int z = c == CTL_UP ? APP_TOUCH_LEFT : c == CTL_DOWN ? APP_TOUCH_RIGHT : APP_TOUCH_MIDDLE;
    const char *lab = c == CTL_UP ? "Up" : c == CTL_DOWN ? "Down" : s->mode == SK_GALLERY ? (s->sel == 0 ? "New page" : "Open") : "Select";
    ui_pill_box(user_w(s), user_h(s), &s->bar, z, lab, c == CTL_UP ? PILL_UP : c == CTL_DOWN ? PILL_DOWN : PILL_NONE, x0, y0, x1, y1);
    return 1;
}

/* Draws the screen of the current mode into `ui`: the chrome, then the thumbnails */
static void menu_draw(sketch_t *s)
{
    if (!s->ui.px) return;
    gfx_t g;
    s->blit_n = 0;
    gal_layout(&s->L, s->orient, user_w(s), user_h(s));
    int first = (s->sel / s->L.per) * s->L.per, n = gcount(s) - first, nums[64];
    if (n > s->L.per) n = s->L.per;
    if (n > 64) n = 64;
    if (s->mode == SK_GALLERY)
        for (int i = 0; i < n; i++) {
            int x, y;
            gal_tile_xy(&s->L, i, &x, &y);
            nums[i] = gnum(s, first + i);
            if (nums[i]) blit_add(s, nums[i], x, y);
        }
    int px = s->L.ax1 - s->L.tw - 12, py = (s->L.ay0 + s->L.ay1) / 2 - s->L.th / 2; /* the preview, framed as a gallery tile */
    if (s->mode == SK_ACTIONS || s->mode == SK_CONFIRM) blit_add(s, s->act_num, px, py);
    cv_fill(&s->ui, 1);
    ui_g(s, &g);
    int hot = ctl_live(s, s->hot) ? s->hot : CTL_NONE;
    char t[24], l1[64], l2[64];
    switch (s->mode) {
    case SK_GALLERY: {
        int sel_slot = s->sel - first;
        ui_gallery(&g, &s->L, nums, n, sel_slot, s->cur, s->sel / s->L.per, gpages(s), s->gnote, hot);
        ui_menu_pills(&g, &s->bar, s->sel == 0 ? "New page" : "Open", hot);
        break;
    }
    case SK_ACTIONS: {
        static const char *const items[] = { "Open", "Print", "Send to phone", "Settings", "Back" };
        int k = 0, v = s->act_num;
        char d[8];
        int m = 0;
        memcpy(t, "Sketch ", 7), k = 7;
        do d[m++] = (char)('0' + v % 10), v /= 10; while (v);
        while (m) t[k++] = d[--m];
        t[k] = 0;
        ui_list(&g, &s->L, t, items, 5, s->act_sel, &s->lg);
        s->list_n = 5;
        gfx_rrect(&g, px - 10, py - 10, px + s->L.tw + 9, py + s->L.th + 9, 12, 2, MWHT_, MBLK_);
        ui_menu_pills(&g, &s->bar, "Select", hot);
        break;
    }
    case SK_CONFIRM: {
        static const char *const items[] = { "Print", "Back" };
        ui_list(&g, &s->L, "Print this drawing?", items, 2, s->act_sel, &s->lg);
        s->list_n = 2;
        gfx_rrect(&g, px - 10, py - 10, px + s->L.tw + 9, py + s->L.th + 9, 12, 2, MWHT_, MBLK_);
        char a[48] = "Printer: ";
        printer_addr(s, a + 9, 36);
        ui_footer(&g, &s->L, a);
        ui_menu_pills(&g, &s->bar, "Select", hot);
        break;
    }
    case SK_BUSY: ui_message(&g, &s->L, "Printing...", "Sending the page to the printer.", "This takes a few seconds.", 0); break;
    default:
        split_msg(s->pres.msg, l1, l2);
        ui_message(&g, &s->L, s->pres.pages && s->pres.jobs ? "Printed" : "Not printed", l1, l2, 0);
        ui_pill(&g, &s->bar, APP_TOUCH_MIDDLE, "OK", PILL_NONE, 0);
        break;
    }
    for (int i = 0; i < s->blit_n; i++) {
        const uint8_t *tb = gal_find(&s->cache, s->blit[i].num);
        if (tb) gal_blit(&s->ui, s->orient, tb, s->blit[i].x, s->blit[i].y);
    }
}

void sk_screen_render(sketch_t *s)
{
    if (!s->ui.px) return;
    menu_draw(s);
    s->want_full = 1;
}

/* ---- the pointer and the highlight: partial refreshes ---- */

static int menu_pointer(const sketch_t *s) { return s->ui.px && (s->mode == SK_GALLERY || s->mode == SK_ACTIONS || s->mode == SK_CONFIRM || s->mode == SK_RESULT); }

void sk_menu_cursor(sketch_t *s)
{
    if (!s->hover_valid || !menu_pointer(s)) return;
    int ux, uy;
    gal_from_fb(s->orient, s->fb.w, s->fb.h, s->hover_x, s->hover_y, &ux, &uy);
    ui_arrow(&s->fb, s->orient, ux, uy, &s->cur_box);
    rs_add(&s->pend, &s->cur_box, s->fb.w, s->fb.h);
    s->cursor = 1;
}

/* The framebuffer box of the highlight of the selected item (empty: none) */
static void sel_box(sketch_t *s, int sel, box_t *b)
{
    gfx_t g;
    int x0, y0, x1, y1;
    box_clear(b);
    ui_g(s, &g);
    if (s->mode == SK_GALLERY) {
        int x, y, slot = sel - (s->sel / s->L.per) * s->L.per;
        gal_tile_xy(&s->L, slot, &x, &y);
        x0 = x - 10, y0 = y - 10, x1 = x + s->L.tw + 9, y1 = y + s->L.th + GAL_LABEL;
    } else if (s->mode == SK_ACTIONS || s->mode == SK_CONFIRM) {
        ui_list_row(&s->lg, sel, &x0, &y0, &x1, &y1);
    } else {
        return;
    }
    ui_fb_box(&g, x0, y0, x1, y1, b);
    box_clip(b, s->fb.w, s->fb.h);
}

static void note_box(sketch_t *s, box_t *b)
{
    gfx_t g;
    ui_g(s, &g);
    ui_fb_box(&g, s->L.ax0, s->L.ay1 - 50, s->L.ax1, s->L.ay1 + 10, b);
    box_clip(b, s->fb.w, s->fb.h);
}

/* Redraws the screen into `ui` and brings only the boxes a and b (and c) to the framebuffer; the pointer is lifted
   first and put back after, so none of it is left behind */
static void redraw_boxes(sketch_t *s, const box_t *a, const box_t *b, const box_t *c)
{
    sk_cursor_hide(s);
    menu_draw(s);
    const box_t *bx[3] = { a, b, c };
    for (int i = 0; i < 3; i++)
        if (bx[i] && !box_empty(bx[i])) {
            cv_copy_box(&s->fb, &s->ui, bx[i]);
            rs_add(&s->pend, bx[i], s->fb.w, s->fb.h);
        }
    sk_menu_cursor(s);
}

/* The selection (a gallery tile or a list row) becomes v: only the old and the new highlight are refreshed, unless the
   gallery turns to another screen of tiles */
static void sel_set(sketch_t *s, int v)
{
    int gal = s->mode == SK_GALLERY, *f = gal ? &s->sel : &s->act_sel;
    if (v == *f) return;
    box_t a, b, n;
    int page = s->L.per ? s->sel / s->L.per : 0, had_note = gal && s->gnote[0];
    sel_box(s, *f, &a);
    *f = v;
    if (gal) s->gnote[0] = 0;
    if (s->want_full || (gal && v / s->L.per != page)) return sk_screen_render(s);
    sel_box(s, v, &b);
    box_clear(&n);
    if (had_note) note_box(s, &n);
    redraw_boxes(s, &a, &b, &n);
}

void sk_to_gallery(sketch_t *s, uint32_t now)
{
    if (s->mode == SK_DRAW && !sk_leave_page(s, now)) return sk_toast(s, "Could not save this page", now);
    if (!s->ui.px) {
        s->ui.px = malloc((size_t)s->fb.stride * (size_t)s->fb.h);
        s->ui.w = s->fb.w, s->ui.h = s->fb.h, s->ui.stride = s->fb.stride;
        if (!s->ui.px) return sk_toast(s, "Not enough memory for the gallery", now);
    }
    pg_scan(&s->pg);
    sk_cursor_hide(s);
    sk_note_hide(s);
    s->cursor = 0;
    s->tb_on = s->tb_hold = 0; /* the toolbar is for drawing only */
    s->mode = SK_GALLERY;
    s->sel = gidx_of(s, s->cur);
    s->gnote[0] = 0;
    s->ptr_down = 0;
    sk_screen_render(s);
}

static void to_draw(sketch_t *s)
{
    s->cursor = 0; /* the page comes back whole */
    free(s->ui.px);
    s->ui.px = 0;
    gal_cache_free(&s->cache);
    s->mode = SK_DRAW;
    sk_show_page(s);
    s->want_full = 1;
}

static void gal_activate(sketch_t *s, uint32_t now)
{
    int num = gnum(s, s->sel);
    if (num == 0) {
        if (sk_is_blank(&s->page)) { /* no run of empty pages (owner 2026-11-01) */
            box_t n;
            sk_cpy(s->gnote, sizeof(s->gnote), "This page is still empty");
            if (s->want_full) return sk_screen_render(s);
            note_box(s, &n);
            return redraw_boxes(s, &n, 0, 0);
        }
        to_draw(s);
        return sk_make_page(s, now);
    }
    s->act_num = num;
    s->act_sel = 0;
    s->mode = SK_ACTIONS;
    sk_screen_render(s);
}

static void list_select(sketch_t *s, uint32_t now)
{
    if (s->mode == SK_ACTIONS) {
        switch (s->act_sel) {
        case 0:
            to_draw(s);
            if (s->act_num != s->cur) sk_open_page(s, s->act_num, now);
            break;
        case 1:
            if (!s->env.net.post) {
                s->pres = (pj_result_t){ "Printing is not available on this firmware.", 0, 0 };
                s->mode = SK_RESULT;
            } else {
                s->mode = SK_CONFIRM;
                s->act_sel = 0;
            }
            sk_screen_render(s);
            break;
        case 2: case 3:
            sk_cursor_hide(s);
            /* the phone page: the files (download the drawings) or the settings form (the printer's address) */
            s->mode = SK_SHARE;
            s->share_req = s->act_sel == 2 ? 1 : 2;
            s->share_kind = s->share_req;
            s->share_rc = 0;
            memset(&s->share_shown, 0xff, sizeof(s->share_shown));
            break;
        default:
            s->mode = SK_GALLERY;
            sk_screen_render(s);
        }
    } else if (s->mode == SK_CONFIRM) {
        if (s->act_sel == 0) {
            s->mode = SK_BUSY;
            s->print_req = 1;
        } else {
            s->mode = SK_ACTIONS;
            s->act_sel = 1;
        }
        sk_screen_render(s);
    }
}

static void menu_move(sketch_t *s, int dir)
{
    int v, n;
    if (s->mode == SK_GALLERY) v = s->sel + dir, n = gcount(s);
    else if (s->mode == SK_ACTIONS || s->mode == SK_CONFIRM) v = s->act_sel + dir, n = s->list_n;
    else return;
    if (v >= 0 && v < n) sel_set(s, v);
}

/* A whole screen of tiles back or forward, same slot (the last tile when that one is past the end); none past the ends */
static void gal_page(sketch_t *s, int dir)
{
    int np = s->sel / s->L.per + dir, v = np * s->L.per + s->sel % s->L.per;
    if (np < 0 || np >= gpages(s)) return;
    sel_set(s, v >= gcount(s) ? gcount(s) - 1 : v);
}

void sk_menu_touch(sketch_t *s, int zone, int gesture, uint32_t now)
{
    if (s->mode == SK_BUSY) return;
    if (s->mode == SK_RESULT) {
        if (gesture == APP_GESTURE_TAP) s->mode = SK_GALLERY, sk_screen_render(s);
        return;
    }
    if (s->mode == SK_GALLERY && (gesture == APP_GESTURE_SWIPE_LEFT || gesture == APP_GESTURE_SWIPE_RIGHT)) {
        return gal_page(s, gesture == APP_GESTURE_SWIPE_LEFT ? 1 : -1);
    }
    if (gesture != APP_GESTURE_TAP) return;
    if (zone == APP_TOUCH_LEFT) menu_move(s, -1);
    else if (zone == APP_TOUCH_RIGHT) menu_move(s, 1);
    else if (zone == APP_TOUCH_MIDDLE) {
        if (s->mode == SK_GALLERY) gal_activate(s, now);
        else list_select(s, now);
    }
}

/* The control under a user-frame point (a little generous: the pen is not exact), or CTL_NONE */
static int ctl_at(const sketch_t *s, int ux, int uy)
{
    for (int c = CTL_UP; c <= CTL_NEXT; c++) {
        int x0, y0, x1, y1;
        if (ctl_live(s, c) && ctl_box(s, c, &x0, &y0, &x1, &y1) && ux >= x0 - 20 && ux <= x1 + 20 && uy >= y0 - 20 && uy <= y1 + 20) return c;
    }
    return CTL_NONE;
}

static void ctl_fb_box(sketch_t *s, int c, box_t *b)
{
    gfx_t g;
    int x0, y0, x1, y1;
    box_clear(b);
    if (!ctl_live(s, c) || !ctl_box(s, c, &x0, &y0, &x1, &y1)) return;
    ui_g(s, &g);
    ui_fb_box(&g, x0, y0, x1, y1, b);
    box_clip(b, s->fb.w, s->fb.h);
}

/* The pen moves onto control c (or off all): the old and the new one are redrawn, nothing else */
static void hot_set(sketch_t *s, int c)
{
    if (c == s->hot) return;
    box_t a, b;
    ctl_fb_box(s, s->hot, &a);
    s->hot = c;
    ctl_fb_box(s, c, &b);
    if (s->want_full) return sk_screen_render(s);
    redraw_boxes(s, &a, &b, 0);
}

static void ctl_press(sketch_t *s, int c, uint32_t now)
{
    if (c == CTL_UP || c == CTL_DOWN) menu_move(s, c == CTL_UP ? -1 : 1);
    else if (c == CTL_PREV || c == CTL_NEXT) gal_page(s, c == CTL_PREV ? -1 : 1);
    else if (s->mode == SK_GALLERY) gal_activate(s, now);
    else list_select(s, now);
}

/* The tablet's buttons in the menu screens: 1 and 2 (and 7, 8 in lists) move the selection, 7 and 8 turn the gallery's
   screen of tiles, 4 opens, 5 goes back (the gallery: closes it), 6 is the New page tile */
void sk_menu_key(sketch_t *s, unsigned down, uint32_t now)
{
    if (s->mode == SK_BUSY) return;
    if (s->mode == SK_SHARE || s->mode == SK_RESULT) return sk_touch(s, APP_TOUCH_MIDDLE, APP_GESTURE_TAP, now);
    for (int b = 0; b < 8; b++) {
        int gal = s->mode == SK_GALLERY;
        if (!(down >> b & 1) || (s->mode < SK_GALLERY)) continue;
        if (b == 0 || (b == 6 && !gal)) menu_move(s, -1);
        else if (b == 1 || (b == 7 && !gal)) menu_move(s, 1);
        else if (b == 6) gal_page(s, -1);
        else if (b == 7) gal_page(s, 1);
        else if (b == 3) ctl_press(s, CTL_SEL, now);
        else if (b == 5 && gal) sel_set(s, 0), gal_activate(s, now);
        else if (b == 4 && gal) to_draw(s);
        else if (b == 4 && s->mode == SK_ACTIONS) s->mode = SK_GALLERY, sk_screen_render(s);
        else if (b == 4 && s->mode == SK_CONFIRM) s->mode = SK_ACTIONS, s->act_sel = 1, sk_screen_render(s);
    }
}

/* What the pen or mouse points at in the screen of the current mode: a gallery index, a list row, or -1 */
static int item_at(const sketch_t *s, int ux, int uy)
{
    if (s->mode == SK_GALLERY) {
        int slot = gal_hit(&s->L, ux, uy), idx = (s->sel / s->L.per) * s->L.per + slot;
        return slot >= 0 && idx < gcount(s) ? idx : -1;
    }
    if (s->mode == SK_ACTIONS || s->mode == SK_CONFIRM) {
        for (int i = 0; i < s->list_n; i++) {
            int b = s->lg.y0 + i * LIST_PITCH;
            if (ux >= s->lg.x - 100 && ux < s->L.ax1 && uy >= b - 80 && uy < b + 30) return i;
        }
        return -1;
    }
    return s->mode == SK_RESULT || s->mode == SK_SHARE ? 0 : -1;
}

/* The pen or mouse over the screen: the pointer follows it, and the item under it becomes the selection (the same one
   the touch bar moves) */
static void menu_hover(sketch_t *s, int over, int fx, int fy, int ux, int uy)
{
    if (!menu_pointer(s)) return;
    if (!over) {
        s->hover_valid = 0;
        if (s->hot) hot_set(s, CTL_NONE);
        return sk_cursor_hide(s);
    }
    if (s->hover_valid && s->cursor && (fx - s->hover_x) * (fx - s->hover_x) + (fy - s->hover_y) * (fy - s->hover_y) < 9) return;
    s->hover_valid = 1, s->hover_x = fx, s->hover_y = fy;
    int c = ctl_at(s, ux, uy);
    if (c != s->hot) return hot_set(s, c); /* puts the pointer back too */
    int it = s->mode == SK_RESULT || c ? -1 : item_at(s, ux, uy);
    if (it >= 0 && it != (s->mode == SK_GALLERY ? s->sel : s->act_sel)) return sel_set(s, it); /* puts the pointer back too */
    sk_cursor_hide(s);
    sk_menu_cursor(s);
}

/* A press and release on the same item with little movement opens it; a drag across items does nothing. Never ink. */
void sk_menu_pen(sketch_t *s, unsigned flags, int fx, int fy, int inside, uint32_t now)
{
    int tip = (flags & APP_PEN_TIP) != 0, ux, uy;
    gal_from_fb(s->orient, s->fb.w, s->fb.h, fx, fy, &ux, &uy);
    menu_hover(s, (flags & (APP_PEN_TIP | APP_PEN_IN_RANGE)) != 0 && inside, fx, fy, ux, uy);
    if (tip && inside) {
        if (!s->ptr_down) s->ptr_down = 1, s->ptr_moved = 0, s->ptr_ctl = ctl_at(s, ux, uy), s->ptr_item = s->ptr_ctl ? -1 : item_at(s, ux, uy), s->ptr_x = ux, s->ptr_y = uy;
        else if ((ux - s->ptr_x) * (ux - s->ptr_x) + (uy - s->ptr_y) * (uy - s->ptr_y) > 40 * 40) s->ptr_moved = 1;
        return;
    }
    if (!s->ptr_down) return;
    s->ptr_down = 0;
    if (!s->ptr_moved && s->ptr_ctl && ctl_at(s, ux, uy) == s->ptr_ctl) return ctl_press(s, s->ptr_ctl, now);
    if (s->ptr_moved || s->ptr_item < 0 || item_at(s, ux, uy) != s->ptr_item) return;
    if (s->mode == SK_GALLERY) s->sel = s->ptr_item, gal_activate(s, now);
    else if (s->mode == SK_SHARE) sk_touch(s, APP_TOUCH_MIDDLE, APP_GESTURE_TAP, now);
    else if (s->mode == SK_RESULT) sk_menu_touch(s, APP_TOUCH_MIDDLE, APP_GESTURE_TAP, now);
    else if (s->mode != SK_BUSY) s->act_sel = s->ptr_item, list_select(s, now);
}

void sk_run_print(sketch_t *s)
{
    s->print_req = 0;
    s->cursor = 0;
    gal_cache_free(&s->cache); /* memory for the job */
    free(s->ui.px);
    s->ui.px = 0;
    sk_print_page(s, s->act_num);
    s->ui.px = malloc((size_t)s->fb.stride * (size_t)s->fb.h);
    if (!s->ui.px) { /* no memory for the result screen: back to the page, with the reason as a note */
        to_draw(s);
        return sk_toast(s, s->pres.pages ? "Printed" : "Not printed", s->last_push);
    }
    s->mode = SK_RESULT;
    sk_screen_render(s);
}

