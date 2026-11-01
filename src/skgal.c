/* SPDX-License-Identifier: MIT */
/* The gallery, the action list, the print screens and the pen and touch bar in them: the controller of the gray screens
   (sketch.c holds drawing, pages and saving). */
#include "sketch.h"
#include <stdlib.h>
#include <string.h>
#include "sk_cfg.h"

/* ---- the gallery and the menus (gray screens) ---- */

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

int sk_screen_gray(const sketch_t *s) { return s->mode >= SK_GALLERY; }

void sk_set_screens(sketch_t *s, uint8_t *fb1, uint8_t *gray)
{
    if (fb1) s->fb.px = fb1, s->fb_stale = 1;
    if (gray) s->gfb = gray;
}

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

/* Draws the screen of the current gray mode into `ui` and lists the thumbnails to lay over it */
void sk_screen_render(sketch_t *s)
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
    int px = s->L.ax1 - s->L.tw, py = (s->L.ay0 + s->L.ay1) / 2 - s->L.th / 2;
    if (s->mode == SK_ACTIONS || s->mode == SK_CONFIRM) blit_add(s, s->act_num, px, py);
    cv_fill(&s->ui, 1);
    ui_g(s, &g);
    char t[24], l1[64], l2[64];
    switch (s->mode) {
    case SK_GALLERY: {
        int sel_slot = s->sel - first;
        ui_gallery(&g, &s->L, nums, n, sel_slot, s->cur, s->sel / s->L.per, (gcount(s) + s->L.per - 1) / s->L.per, s->gnote);
        ui_menu_pills(&g, &s->bar, s->sel == 0 ? "New page" : "Open");
        break;
    }
    case SK_ACTIONS: {
        static const char *const items[] = { "Open", "Print", "Send to phone", "Back" };
        int k = 0, v = s->act_num;
        char d[8];
        int m = 0;
        memcpy(t, "Sketch ", 7), k = 7;
        do d[m++] = (char)('0' + v % 10), v /= 10; while (v);
        while (m) t[k++] = d[--m];
        t[k] = 0;
        ui_list(&g, &s->L, t, items, 4, s->act_sel, &s->list_x, &s->list_y0);
        s->list_n = 4;
        ui_menu_pills(&g, &s->bar, "Select");
        break;
    }
    case SK_CONFIRM: {
        static const char *const items[] = { "Print", "Back" };
        ui_list(&g, &s->L, "Print this drawing?", items, 2, s->act_sel, &s->list_x, &s->list_y0);
        s->list_n = 2;
        char a[48] = "Printer: ";
        printer_addr(s, a + 9, 36);
        ui_footer(&g, &s->L, a);
        ui_menu_pills(&g, &s->bar, "Select");
        break;
    }
    case SK_BUSY: ui_message(&g, &s->L, "Printing...", "Sending the page to the printer.", "This takes a few seconds.", 0); break;
    default:
        split_msg(s->pres.msg, l1, l2);
        ui_message(&g, &s->L, s->pres.pages && s->pres.jobs ? "Printed" : "Not printed", l1, l2, 0);
        ui_pill(&g, &s->bar, APP_TOUCH_MIDDLE, "OK", PILL_NONE);
        break;
    }
    s->want_full = 1;
}

void sk_compose_gray(sketch_t *s)
{
    if (!s->gfb || !s->ui.px) return;
    gal_gray_from_mono(s->gfb, &s->ui);
    for (int i = 0; i < s->blit_n; i++) {
        const uint8_t *t = gal_find(&s->cache, s->blit[i].num);
        if (t) gal_gray_blit(s->gfb, s->fb.w, s->fb.h, s->orient, t, s->blit[i].x, s->blit[i].y);
    }
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
            sk_cpy(s->gnote, sizeof(s->gnote), "This page is still empty");
            return sk_screen_render(s);
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
        case 2:
            s->mode = SK_SHARE;
            s->share_req = 1;
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
    if (s->mode == SK_GALLERY) {
        int v = s->sel + dir;
        if (v < 0 || v >= gcount(s)) return;
        s->sel = v;
        s->gnote[0] = 0;
    } else if (s->mode == SK_ACTIONS || s->mode == SK_CONFIRM) {
        int v = s->act_sel + dir;
        if (v < 0 || v >= s->list_n) return;
        s->act_sel = v;
    } else {
        return;
    }
    sk_screen_render(s);
}

void sk_menu_touch(sketch_t *s, int zone, int gesture, uint32_t now)
{
    if (s->mode == SK_BUSY) return;
    if (s->mode == SK_RESULT) {
        if (gesture == APP_GESTURE_TAP) s->mode = SK_GALLERY, sk_screen_render(s);
        return;
    }
    if (s->mode == SK_GALLERY && (gesture == APP_GESTURE_SWIPE_LEFT || gesture == APP_GESTURE_SWIPE_RIGHT)) {
        int v = s->sel + (gesture == APP_GESTURE_SWIPE_LEFT ? 1 : -1) * s->L.per;
        s->sel = v < 0 ? 0 : v >= gcount(s) ? gcount(s) - 1 : v;
        s->gnote[0] = 0;
        return sk_screen_render(s);
    }
    if (gesture != APP_GESTURE_TAP) return;
    if (zone == APP_TOUCH_LEFT) menu_move(s, -1);
    else if (zone == APP_TOUCH_RIGHT) menu_move(s, 1);
    else if (zone == APP_TOUCH_MIDDLE) {
        if (s->mode == SK_GALLERY) gal_activate(s, now);
        else list_select(s, now);
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
            int b = s->list_y0 + i * 110;
            if (ux >= s->list_x - 100 && ux < s->L.ax1 && uy >= b - 80 && uy < b + 30) return i;
        }
        return -1;
    }
    return s->mode == SK_RESULT || s->mode == SK_SHARE ? 0 : -1;
}

/* A press and release on the same item with little movement opens it; a drag across items does nothing. Never ink. */
void sk_menu_pen(sketch_t *s, unsigned flags, int fx, int fy, int inside, uint32_t now)
{
    int tip = (flags & APP_PEN_TIP) != 0, ux, uy;
    gal_from_fb(s->orient, s->fb.w, s->fb.h, fx, fy, &ux, &uy);
    if (tip && inside) {
        if (!s->ptr_down) s->ptr_down = 1, s->ptr_moved = 0, s->ptr_item = item_at(s, ux, uy), s->ptr_x = ux, s->ptr_y = uy;
        else if ((ux - s->ptr_x) * (ux - s->ptr_x) + (uy - s->ptr_y) * (uy - s->ptr_y) > 40 * 40) s->ptr_moved = 1;
        return;
    }
    if (!s->ptr_down) return;
    s->ptr_down = 0;
    if (s->ptr_moved || s->ptr_item < 0 || item_at(s, ux, uy) != s->ptr_item) return;
    if (s->mode == SK_GALLERY) s->sel = s->ptr_item, gal_activate(s, now);
    else if (s->mode == SK_SHARE) sk_touch(s, APP_TOUCH_MIDDLE, APP_GESTURE_TAP, now);
    else if (s->mode == SK_RESULT) sk_menu_touch(s, APP_TOUCH_MIDDLE, APP_GESTURE_TAP, now);
    else if (s->mode != SK_BUSY) s->act_sel = s->ptr_item, list_select(s, now);
}

void sk_run_print(sketch_t *s)
{
    s->print_req = 0;
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

