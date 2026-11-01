/* SPDX-License-Identifier: MIT */
#include "sketch.h"
#include <stdlib.h>
#include <string.h>
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
static void show_page(sketch_t *s) { box_t b; whole(&b, &s->page), cv_copy_box(&s->fb, &s->page, &b); }

static void cursor_hide(sketch_t *s)
{
    if (!s->cursor) return;
    box_t b = { s->cx - s->cr, s->cy - s->cr, s->cx + s->cr, s->cy + s->cr };
    box_clip(&b, s->fb.w, s->fb.h);
    cv_copy_box(&s->fb, &s->page, &b);
    mark(s, &b);
    s->cursor = 0;
}

static int erasing(const sketch_t *s) { return s->eraser ^ (s->barrel != 0); }

static void cursor_show(sketch_t *s, int fx, int fy)
{
    int r = erasing(s) ? pen_erase_d(s->size) / 2 + 1 : SK_CURSOR_R;
    box_t want = { fx - r, fy - r, fx + r, fy + r };
    if (s->cursor && s->cr == r && (fx - s->cx) * (fx - s->cx) + (fy - s->cy) * (fy - s->cy) < 9) return;
    cursor_hide(s);
    if (s->note_on && hit(&want, &s->note)) return; /* never over a note: restoring the page would eat it */
    box_t b;
    ui_cursor(&s->fb, fx, fy, r, &b);
    mark(s, &b);
    s->cursor = 1, s->cx = fx, s->cy = fy, s->cr = r;
}

static void note_hide(sketch_t *s)
{
    if (!s->note_on) return;
    cv_copy_box(&s->fb, &s->page, &s->note);
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

static void toast(sketch_t *s, const char *text, uint32_t now)
{
    if (s->mode != SK_DRAW) return;
    cursor_hide(s);
    note_hide(s);
    gfx_t g;
    box_t b;
    mkg(s, &g);
    ui_toast(&g, text, &b);
    note_set(s, &b, now + TOAST_MS);
}

/* a change of the page in box b reaches the screen */
static void page_changed(sketch_t *s, const box_t *b, uint32_t now)
{
    cursor_hide(s);
    if (s->note_on && hit(b, &s->note)) note_hide(s);
    cv_copy_box(&s->fb, &s->page, b);
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

static int is_blank(const canvas_t *c)
{
    int rb = (c->w + 7) / 8;
    for (int y = 0; y < c->h; y++)
        for (int x = 0; x < rb; x++)
            if (c->px[y * c->stride + x] != 0xff) return 0;
    return 1;
}

static void open_page(sketch_t *s, int num, uint32_t now)
{
    s->cur = num;
    s->bad_pending = 0;
    int rc = pg_load(&s->pg, num, &s->page);
    if (rc == PG_MISSING) cv_fill(&s->page, 1);
    if (rc == PG_BAD) s->bad_pending = 1;
    hist_reset(&s->hist);
    s->cursor = s->note_on = s->legend = s->stroke = 0;
    show_page(s);
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

static void new_page(sketch_t *s, uint32_t now);

static int leave_page(sketch_t *s, uint32_t now) /* saves the page on screen first; 0 when it could not */
{
    hist_end(&s->hist);
    s->stroke = 0;
    if (s->dirty && do_save(s, now)) return 0;
    return 1;
}

void sk_goto_page(sketch_t *s, int index, uint32_t now)
{
    if (!leave_page(s, now)) return toast(s, "Could not save this page", now);
    pg_scan(&s->pg);
    if (s->pg.n == 0) return new_page(s, now);
    if (index < 0 || index >= s->pg.n) return toast(s, index < 0 ? "First page" : "Last page", now);
    open_page(s, s->pg.nums[index], now);
    char b[24];
    toast(s, page_text(s, b), now);
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
    if (!leave_page(s, now)) return toast(s, "Could not save this page", now);
    pg_scan(&s->pg);
    int present, pos = cur_pos(s, &present);
    int target = dir > 0 ? (present ? pos + 1 : pos) : pos - 1;
    if (s->pg.n == 0 || target < 0 || target >= s->pg.n) {
        if (s->pg.n == 0) return new_page(s, now);
        return toast(s, target < 0 ? "First page" : "Last page", now);
    }
    open_page(s, s->pg.nums[target], now);
    char b[24];
    toast(s, page_text(s, b), now);
}

/* A new page only from a page with ink: no run of empty pages (owner 2026-11-01) */
void sk_new_page(sketch_t *s, uint32_t now)
{
    if (is_blank(&s->page)) return toast(s, "This page is still empty", now);
    new_page(s, now);
}

static void new_page(sketch_t *s, uint32_t now)
{
    if (!leave_page(s, now)) return toast(s, "Could not save this page", now);
    pg_scan(&s->pg);
    int num = pg_next_num(&s->pg);
    if (!num) return toast(s, "No more pages", now);
    s->cur = num;
    cv_fill(&s->page, 1);
    s->bad_pending = 0;
    hist_reset(&s->hist);
    s->cursor = s->note_on = s->legend = 0;
    show_page(s);
    s->want_full = 1;
    cfg_num(s, "cur", num);
    if (pg_save(&s->pg, num, &s->page)) s->dirty = 1, s->save_at = now + RETRY_MS; /* listed once it is written */
    char b[24];
    toast(s, page_text(s, b), now);
}

/* ---- actions ---- */

void sk_undo(sketch_t *s, uint32_t now)
{
    box_t b;
    s->stroke = 0;
    if (hist_undo(&s->hist, &b)) page_changed(s, &b, now);
    else toast(s, "Nothing to undo", now);
}

void sk_redo(sketch_t *s, uint32_t now)
{
    box_t b;
    s->stroke = 0;
    if (hist_redo(&s->hist, &b)) page_changed(s, &b, now);
    else toast(s, "Nothing to redo", now);
}

void sk_clear(sketch_t *s, uint32_t now)
{
    hist_end(&s->hist);
    s->stroke = 0;
    if (is_blank(&s->page)) return toast(s, "The page is empty", now);
    if (s->dirty) do_save(s, now);
    pg_save_as(&s->pg, s->cur, "-cleared.png", &s->page); /* what was on the page stays on the flash */
    box_t b;
    hist_clear(&s->hist, &b);
    cursor_hide(s);
    note_hide(s);
    show_page(s);
    s->want_full = 1;
    s->dirty = 1;
    s->save_at = now + SK_AUTOSAVE_MS;
    toast(s, "Cleared. Undo brings it back", now);
}

static void next_size(sketch_t *s, uint32_t now)
{
    static const char *const name[SK_SIZES] = { "Pen: thin", "Pen: medium", "Pen: thick" };
    s->size = (s->size + 1) % SK_SIZES;
    cfg_num(s, "size", s->size);
    toast(s, name[s->size], now);
}

static void toggle_eraser(sketch_t *s, uint32_t now)
{
    s->eraser = !s->eraser;
    toast(s, s->eraser ? "Eraser on" : "Eraser off", now);
}

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
static void screen_render(sketch_t *s)
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

static void compose_gray(sketch_t *s)
{
    if (!s->gfb || !s->ui.px) return;
    gal_gray_from_mono(s->gfb, &s->ui);
    for (int i = 0; i < s->blit_n; i++) {
        const uint8_t *t = gal_find(&s->cache, s->blit[i].num);
        if (t) gal_gray_blit(s->gfb, s->fb.w, s->fb.h, s->orient, t, s->blit[i].x, s->blit[i].y);
    }
}

static void to_gallery(sketch_t *s, uint32_t now)
{
    if (s->mode == SK_DRAW && !leave_page(s, now)) return toast(s, "Could not save this page", now);
    if (!s->ui.px) {
        s->ui.px = malloc((size_t)s->fb.stride * (size_t)s->fb.h);
        s->ui.w = s->fb.w, s->ui.h = s->fb.h, s->ui.stride = s->fb.stride;
        if (!s->ui.px) return toast(s, "Not enough memory for the gallery", now);
    }
    pg_scan(&s->pg);
    cursor_hide(s);
    note_hide(s);
    s->cursor = 0;
    s->mode = SK_GALLERY;
    s->sel = gidx_of(s, s->cur);
    s->gnote[0] = 0;
    s->ptr_down = 0;
    screen_render(s);
}

static void to_draw(sketch_t *s)
{
    free(s->ui.px);
    s->ui.px = 0;
    gal_cache_free(&s->cache);
    s->mode = SK_DRAW;
    show_page(s);
    s->want_full = 1;
}

static void gal_activate(sketch_t *s, uint32_t now)
{
    int num = gnum(s, s->sel);
    if (num == 0) {
        if (is_blank(&s->page)) { /* no run of empty pages (owner 2026-11-01) */
            sk_cpy(s->gnote, sizeof(s->gnote), "This page is still empty");
            return screen_render(s);
        }
        to_draw(s);
        return new_page(s, now);
    }
    s->act_num = num;
    s->act_sel = 0;
    s->mode = SK_ACTIONS;
    screen_render(s);
}

static void list_select(sketch_t *s, uint32_t now)
{
    if (s->mode == SK_ACTIONS) {
        switch (s->act_sel) {
        case 0:
            to_draw(s);
            if (s->act_num != s->cur) open_page(s, s->act_num, now);
            break;
        case 1:
            if (!s->env.net.post) {
                s->pres = (pj_result_t){ "Printing is not available on this firmware.", 0, 0 };
                s->mode = SK_RESULT;
            } else {
                s->mode = SK_CONFIRM;
                s->act_sel = 0;
            }
            screen_render(s);
            break;
        case 2:
            s->mode = SK_SHARE;
            s->share_req = 1;
            s->share_rc = 0;
            memset(&s->share_shown, 0xff, sizeof(s->share_shown));
            break;
        default:
            s->mode = SK_GALLERY;
            screen_render(s);
        }
    } else if (s->mode == SK_CONFIRM) {
        if (s->act_sel == 0) {
            s->mode = SK_BUSY;
            s->print_req = 1;
        } else {
            s->mode = SK_ACTIONS;
            s->act_sel = 1;
        }
        screen_render(s);
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
    screen_render(s);
}

static void menu_touch(sketch_t *s, int zone, int gesture, uint32_t now)
{
    if (s->mode == SK_BUSY) return;
    if (s->mode == SK_RESULT) {
        if (gesture == APP_GESTURE_TAP) s->mode = SK_GALLERY, screen_render(s);
        return;
    }
    if (s->mode == SK_GALLERY && (gesture == APP_GESTURE_SWIPE_LEFT || gesture == APP_GESTURE_SWIPE_RIGHT)) {
        int v = s->sel + (gesture == APP_GESTURE_SWIPE_LEFT ? 1 : -1) * s->L.per;
        s->sel = v < 0 ? 0 : v >= gcount(s) ? gcount(s) - 1 : v;
        s->gnote[0] = 0;
        return screen_render(s);
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
static void menu_pen(sketch_t *s, unsigned flags, int fx, int fy, int inside, uint32_t now)
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
    else if (s->mode == SK_RESULT) menu_touch(s, APP_TOUCH_MIDDLE, APP_GESTURE_TAP, now);
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
        return toast(s, s->pres.pages ? "Printed" : "Not printed", s->last_push);
    }
    s->mode = SK_RESULT;
    screen_render(s);
}

/* ---- input ---- */

void sk_pen(sketch_t *s, unsigned flags, unsigned x, unsigned y, unsigned pressure, uint32_t now)
{
    s->barrel = (flags & (APP_PEN_BARREL | APP_PEN_BARREL2)) != 0;
    int fx, fy, inside = pm_map(&s->map, x, y, &fx, &fy);
    if (s->mode != SK_DRAW) return menu_pen(s, flags, fx, fy, inside, now);
    int tip = (flags & APP_PEN_TIP) != 0, range = (flags & (APP_PEN_IN_RANGE | APP_PEN_TIP)) != 0;
    if (tip && inside) {
        if (!s->stroke) {
            cursor_hide(s);
            note_hide(s);
            s->stroke_erase = erasing(s);
            hist_begin(&s->hist, s->stroke_erase ? HK_ERASE : HK_INK);
            s->stroke = 1;
            s->pressure = pressure;
        } else {
            s->pressure = (s->pressure + pressure) / 2;
        }
        int d = s->stroke_erase ? pen_erase_d(s->size) : pen_diam(s->size, s->pressure);
        box_t dirty;
        box_clear(&dirty);
        hist_add(&s->hist, fx, fy, d, &dirty);
        if (!box_empty(&dirty)) {
            cv_copy_box(&s->fb, &s->page, &dirty);
            mark(s, &dirty);
        }
        s->dirty = 1;
        s->save_at = now + SK_AUTOSAVE_MS;
        return;
    }
    if (s->stroke) {
        hist_end(&s->hist);
        s->stroke = 0;
        s->save_at = now + SK_AUTOSAVE_MS;
    }
    s->hover_valid = range && !tip && inside;
    s->hover_x = fx, s->hover_y = fy;
    if (s->hover_valid) cursor_show(s, fx, fy);
    else cursor_hide(s);
}

void sk_pen_key(sketch_t *s, unsigned keys, unsigned changed, uint32_t now)
{
    if (s->mode != SK_DRAW) return;
    unsigned down = keys & changed;
    for (int b = 0; b < 8; b++) {
        if (!(down >> b & 1)) continue;
        switch (b) {
        case 0: sk_undo(s, now); break;
        case 1: sk_redo(s, now); break;
        case 2: toggle_eraser(s, now); break;
        case 3: next_size(s, now); break;
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
        screen_render(s);
        return;
    }
    if (s->mode >= SK_GALLERY) return menu_touch(s, zone, gesture, now);
    if (gesture == APP_GESTURE_SWIPE_LEFT) step_page(s, 1, now);
    else if (gesture == APP_GESTURE_SWIPE_RIGHT) step_page(s, -1, now);
    else if (gesture == APP_GESTURE_HOLD && zone == APP_TOUCH_MIDDLE) sk_clear(s, now);
    else if (gesture == APP_GESTURE_TAP && zone == APP_TOUCH_LEFT) sk_undo(s, now);
    else if (gesture == APP_GESTURE_TAP && zone == APP_TOUCH_RIGHT) sk_redo(s, now);
    else if (gesture == APP_GESTURE_TAP && zone == APP_TOUCH_MIDDLE) to_gallery(s, now);
}

void sk_share_update(sketch_t *s, int start_rc, const app_webshare_status_t *st)
{
    if (s->mode != SK_SHARE) return;
    share_view_t v;
    share_view(&v, start_rc, st);
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
        note_hide(s);
        if (s->hover_valid) cursor_show(s, s->hover_x, s->hover_y);
    }
    if (s->legend && (int32_t)(now - s->note_until) >= 0) note_hide(s);
    if (s->dirty && !s->stroke && (int32_t)(now - s->save_at) >= 0) do_save(s, now);
}

int sk_flush(sketch_t *s, uint32_t now, rect_t *out)
{
    if (s->want_full) {
        s->want_full = 0;
        if (s->mode >= SK_GALLERY) compose_gray(s);
        else if (s->fb_stale) {
            s->fb_stale = 0;
            if (s->mode == SK_DRAW) show_page(s);
            else if (s->share.steps[0][0] || s->share.note[0]) {
                gfx_t g;
                mkg(s, &g);
                ui_share(&g, &s->bar, &s->share);
            }
        }
        rs_init(&s->pend);
        s->last_push = now;
        s->pushed_px += (uint32_t)(s->fb.w * s->fb.h);
        s->pushed_rects++;
        return SK_FULL;
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
    if (s->want_full) return 0;
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
        cursor_hide(s);
        note_hide(s);
        show_page(s); /* nothing but the page is left on the screen */
    } else if (s->mode >= SK_GALLERY) {
        screen_render(s);
    } else {
        memset(&s->share_shown, 0xff, sizeof(s->share_shown));
    }
    s->want_full = 1;
}

int sk_init(sketch_t *s, const sk_env_t *env, uint8_t *fb, int w, int h, int stride, int orient, const app_touchbar_t *bar)
{
    memset(s, 0, sizeof(*s));
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
    s->size = cfg_int(s, "size", 1) % SK_SIZES;
    s->orient = orient;
    s->bar = *bar;
    pm_init(&s->map, orient, w, h, 0, 0);
    int want = cfg_int(s, "cur", 0);
    int num = pg_index(&s->pg, want) >= 0 ? want : s->pg.n ? s->pg.nums[s->pg.n - 1] : 1;
    s->cur = num;
    if (s->pg.n == 0) {
        pg_save(&s->pg, num, &s->page); /* page 1 exists from the start */
    } else {
        open_page(s, num, 0);
    }
    show_page(s);
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
