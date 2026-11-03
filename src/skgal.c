/* SPDX-License-Identifier: MIT */
/* The gallery, the action list, the print screens and the pen and touch bar in them: the controller of the menu screens
   (sketch.c holds drawing, pages and saving). They run in the panel's 1-bit mode: the pointer and the highlight are
   partial refreshes, a full one only when a screen opens or closes. */
#include "sketch.h"
#include <stdlib.h>
#include <string.h>
#include "sk_cfg.h"
#include "font.h"
#include "sksettings.h"

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

static int is_dmg(const sketch_t *s, int num)
{
    for (int i = 0; i < s->dmg_n; i++)
        if (s->dmg[i] == num) return 1;
    return 0;
}

/* The thumbnail of a page: cached, else made from the page on screen or from its file (read through `sc` as scratch) */
static const uint8_t *thumb_in(sketch_t *s, int num, canvas_t *sc)
{
    const uint8_t *t = gal_find(&s->cache, num);
    if (t) return t;
    if (num == s->cur && !s->paper) return gal_make(&s->cache, num, &s->page);
    if (num == s->cur) { /* the paper shows in the thumbnail, lightly (the pattern is one pixel in three) */
        memcpy(sc->px, s->page.px, (size_t)sc->stride * (size_t)sc->h);
        box_t all = { 0, 0, sc->w - 1, sc->h - 1 };
        pp_overlay(sc, &all, s->paper, s->paper_sp);
        return gal_make(&s->cache, num, sc);
    }
    int rc = pg_load(&s->pg, num, sc);
    if (rc == PG_MISSING) return 0;
    if (rc == PG_BAD) { /* never deleted, never opened for drawing: a "Damaged" tile */
        if (!is_dmg(s, num) && s->dmg_n < SK_DMG_MAX) s->dmg[s->dmg_n++] = (uint16_t)num;
        return 0;
    }
    int pk, psp;
    pg_paper_load(&s->pg, num, &pk, &psp);
    box_t all = { 0, 0, sc->w - 1, sc->h - 1 };
    pp_overlay(sc, &all, pk, psp);
    return gal_make(&s->cache, num, sc);
}

static const uint8_t *thumb_for(sketch_t *s, int num) { return thumb_in(s, num, &s->ui); } /* before ui is drawn */

static void blit_add(sketch_t *s, int num, int x, int y)
{
    if (s->blit_n < SK_BLITS && thumb_for(s, num)) s->blit[s->blit_n].num = num, s->blit[s->blit_n].x = x, s->blit[s->blit_n].y = y, s->blit_n++;
}

/* A gallery tile's thumbnail when it costs nothing (cached, or the page on screen, which is in memory); else the tile
   waits for sk_gal_step (owner 2026-11-03: a screen of 15 tiles decoded every page file before it showed, 3 to 5 s) */
static void blit_cheap(sketch_t *s, int slot, int num, int x, int y)
{
    if (gal_find(&s->cache, num) || num == s->cur) return blit_add(s, num, x, y);
    if (!is_dmg(s, num) && !(s->gal_tried >> slot & 1)) s->gal_todo = 1; /* a damaged or missing file is read once */
}

static void ui_g(sketch_t *s, gfx_t *g) { gfx_init(g, s->ui.px, s->ui.w, s->ui.h, s->ui.stride, s->orient); }

/* The frame round the page's thumbnail (page menu, confirm, printing, result) */
static void preview_frame(const sketch_t *s, gfx_t *g, int px, int py)
{
    gfx_rrect(g, px - 10, py - 10, px + s->L.tw + 9, py + s->L.th + 9, 12, 2, MWHT_, MBLK_);
}

/* The page on screen was printed and has not changed since */
int sk_print_again(const sketch_t *s) { return s->printed_num && s->printed_num == s->act_num && s->printed_rev == s->page_rev; }

static void split_msg(const char *m, char a[64], char b[64])
{
    size_t n = strlen(m), cut = n > 46 ? 46 : n;
    if (n > 46)
        while (cut > 20 && m[cut] != ' ') cut--;
    memset(a, 0, 64), memset(b, 0, 64);
    memcpy(a, m, cut > 63 ? 63 : cut);
    if (cut < n) sk_cpy(b, 64, m + cut + (m[cut] == ' '));
}

/* "Page 12" into t (24 bytes) */
static void num_text(char *t, const char *pre, int v)
{
    char d[8];
    int k = 0, m = 0;
    while (*pre) t[k++] = *pre++;
    do d[m++] = (char)('0' + v % 10), v /= 10; while (v);
    while (m) t[k++] = d[--m];
    t[k] = 0;
}

static void mtext_c(gfx_t *g, int cx, int y, const char *s)
{
    const font_face_t *f = font_get(FONT_BOLD, 46);
    gfx_text(g, f, (int32_t)(cx - (int)((font_width64(f, s, 0) + 32) >> 6) / 2) * 64, y, s, 0, MBLK_);
}

static int gpages(const sketch_t *s) { return (gcount(s) + s->L.per - 1) / s->L.per; }
/* The control as drawn on this screen (CTL_NONE: the screen has none like it) */
static int ctl_live(const sketch_t *s, int c)
{
    if (!s->ui.px || s->mode < SK_GALLERY || s->mode == SK_BUSY) return 0;
    if (s->mode == SK_RESULT) return c == CTL_SEL || c == CTL_BACK || (c == CTL_SETUP && s->need_setup);
    if (c == CTL_BACK) return 1;
    if (c == CTL_PREV) return s->mode == SK_GALLERY && s->sel / s->L.per > 0;
    if (c == CTL_NEXT) return s->mode == SK_GALLERY && s->sel / s->L.per + 1 < gpages(s);
    return c == CTL_UP || c == CTL_SEL || c == CTL_DOWN;
}

/* The user-frame box of a control, for drawing and for the pen */
static int ctl_box(const sketch_t *s, int c, int *x0, int *y0, int *x1, int *y1)
{
    if (c == CTL_PREV || c == CTL_NEXT) return ui_nav_box(&s->L, c, x0, y0, x1, y1), 1;
    if (c == CTL_BACK || c == CTL_SETUP) return ui_ctl_box(&s->L, c, x0, y0, x1, y1), 1;
    if (c < CTL_UP || c > CTL_DOWN) return 0;
    int z = c == CTL_UP ? APP_TOUCH_LEFT : c == CTL_DOWN ? APP_TOUCH_RIGHT : APP_TOUCH_MIDDLE;
    const char *lab = c == CTL_UP ? "Up" : c == CTL_DOWN ? "Down" : s->mode == SK_GALLERY ? (s->sel == 0 ? "New page" : "Open") : s->mode == SK_RESULT ? "OK" : "Select";
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
    if (first != s->gal_first) s->gal_first = first, s->gal_tried = 0; /* another screen of tiles */
    s->gal_todo = 0;
    uint8_t dmg[64];
    memset(dmg, 0, sizeof(dmg));
    if (n > s->L.per) n = s->L.per;
    if (n > 64) n = 64;
    if (s->mode == SK_GALLERY)
        for (int i = 0; i < n; i++) {
            int x, y;
            gal_tile_xy(&s->L, i, &x, &y);
            nums[i] = gnum(s, first + i);
            if (nums[i]) blit_cheap(s, i, nums[i], x, y);
            dmg[i] = nums[i] && is_dmg(s, nums[i]);
        }
    int px = s->L.ax1 - s->L.tw - 12, py = (s->L.ay0 + s->L.ay1) / 2 - s->L.th / 2; /* the preview, framed as a gallery tile */
    if (s->mode == SK_ACTIONS || s->mode == SK_CONFIRM || s->mode == SK_PAGEMENU || s->mode == SK_BUSY || s->mode == SK_RESULT) blit_add(s, s->act_num, px, py);
    cv_fill(&s->ui, 1);
    ui_g(s, &g);
    int hot = ctl_live(s, s->hot) ? s->hot : CTL_NONE;
    char t[24], l1[64], l2[64];
    switch (s->mode) {
    case SK_GALLERY: {
        int sel_slot = s->sel - first;
        ui_gallery(&g, &s->L, nums, dmg, n, sel_slot, s->cur, s->sel / s->L.per, gpages(s), s->gnote, hot);
        ui_menu_pills(&g, &s->bar, s->sel == 0 ? "New page" : "Open", hot);
        ui_button(&g, &s->L, CTL_BACK, hot == CTL_BACK);
        break;
    }
    case SK_PAGEMENU: {
        char pap[24] = "Paper: ";
        sk_cpy(pap + 7, sizeof(pap) - 7, pp_name(s->paper));
        const char *const items[] = { "Back", "Print", "Send to phone", "Settings", pap, "New page", "Gallery", "Exit Sketch" }; /* Back on top (owner 2026-11-02) */
        num_text(t, "Page ", s->act_num);
        ui_list(&g, &s->L, t, items, 8, s->act_sel, &s->lg);
        s->list_n = 8;
        gfx_rrect(&g, px - 10, py - 10, px + s->L.tw + 9, py + s->L.th + 9, 12, 2, MWHT_, MBLK_);
        ui_menu_pills(&g, &s->bar, "Select", hot);
        ui_button(&g, &s->L, CTL_BACK, hot == CTL_BACK);
        break;
    }
    case SK_SETTINGS: {
        char nm[SKS_N][32], vl[SKS_N][16];
        const char *names[SKS_N + 1] = { "Back" }, *vals[SKS_N + 1] = { 0 };
        for (int i = 0; i < SKS_N; i++) sks_row(s, i, nm[i], vl[i]), names[i + 1] = nm[i], vals[i + 1] = vl[i];
        ui_settings(&g, &s->L, "Settings", names, vals, SKS_N + 1, s->act_sel, &s->lg);
        s->list_n = SKS_N + 1;
        if (s->gnote[0]) ui_footer(&g, &s->L, s->gnote);
        else ui_footer(&g, &s->L, "Tap < or > to change a setting. Changes apply at once.");
        ui_menu_pills(&g, &s->bar, "Change", hot);
        ui_button(&g, &s->L, CTL_BACK, hot == CTL_BACK);
        break;
    }
    case SK_ACTIONS: { /* a damaged page: the file can be rescued through the phone page */
        static const char *const items[] = { "Back", "Send to phone" };
        num_text(t, "Damaged ", s->act_num);
        ui_list(&g, &s->L, t, items, 2, s->act_sel, &s->lg);
        s->list_n = 2;
        gfx_rrect(&g, px - 10, py - 10, px + s->L.tw + 9, py + s->L.th + 9, 12, 2, MWHT_, MBLK_);
        mtext_c(&g, px + s->L.tw / 2, py + s->L.th / 2 + 14, "Damaged");
        ui_footer(&g, &s->L, "This page cannot be opened. Send it to your phone to keep the file.");
        ui_menu_pills(&g, &s->bar, "Select", hot);
        ui_button(&g, &s->L, CTL_BACK, hot == CTL_BACK);
        break;
    }
    case SK_CONFIRM: {
        static const char *const items[] = { "Back", "Print" }, *const items2[] = { "Back", "Exit without saving" };
        ui_list(&g, &s->L, s->exit_unsaved ? "Could not save this page" : sk_print_again(s) ? "Printed already. Print again?" : "Print this drawing?", s->exit_unsaved ? items2 : items, 2, s->act_sel, &s->lg);
        s->list_n = 2;
        preview_frame(s, &g, px, py);
        char a[100] = "Printer: ";
        if (!s->exit_unsaved) {
            pj_printer_label(&s->prn, a + 9, 80);
            ui_footer(&g, &s->L, a);
        }
        ui_menu_pills(&g, &s->bar, "Select", hot);
        ui_button(&g, &s->L, CTL_BACK, hot == CTL_BACK);
        break;
    }
    case SK_BUSY: { /* which printer it goes to (owner 2026-11-02) */
        char to[80] = "To: ";
        pj_printer_label(&s->prn, to + 4, sizeof(to) - 4);
        ui_message(&g, &s->L, "Printing...", s->prn.host[0] ? to : "Sending the page to the printer.", "This takes a few seconds.", 0);
        preview_frame(s, &g, px, py);
        break;
    }
    default:
        split_msg(s->pres.msg, l1, l2);
        ui_message(&g, &s->L, s->pres.pages && s->pres.jobs ? "Printed" : "Not printed", l1, l2, 0);
        preview_frame(s, &g, px, py);
        if (s->prn.host[0] && !s->need_setup) {
            char a[100] = "Printer: ";
            pj_printer_label(&s->prn, a + 9, 80);
            ui_footer(&g, &s->L, a);
        }
        ui_pill(&g, &s->bar, APP_TOUCH_MIDDLE, "OK", PILL_NONE, hot == CTL_SEL);
        ui_button(&g, &s->L, CTL_BACK, hot == CTL_BACK);
        if (s->need_setup) ui_button(&g, &s->L, CTL_SETUP, hot == CTL_SETUP);
        if (s->need_setup) ui_footer(&g, &s->L, "Tablet: button 6 sets up the printer, button 5 goes back.");
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

static int is_list(const sketch_t *s) { return s->mode == SK_ACTIONS || s->mode == SK_CONFIRM || s->mode == SK_PAGEMENU || s->mode == SK_SETTINGS; }
static int menu_pointer(const sketch_t *s) { return s->ui.px && (s->mode == SK_GALLERY || is_list(s) || s->mode == SK_RESULT); }

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
    } else if (is_list(s)) {
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

void sk_gal_step(sketch_t *s, int early)
{
    if (!s->gal_todo || s->mode != SK_GALLERY || !s->ui.px || s->fl_phase || !box_empty(&s->flash) || (s->want_full && !early)) return;
    int first = s->gal_first, n = gcount(s) - first;
    if (n > s->L.per) n = s->L.per;
    for (int i = 0; i < n && i < 64; i++) {
        int num = gnum(s, first + i), x, y;
        if (!num || (s->gal_tried >> i & 1) || gal_find(&s->cache, num) || is_dmg(s, num) || num == s->cur) continue;
        s->gal_tried |= 1ull << i;
        int dmg0 = s->dmg_n;
        canvas_t sc = s->ui; /* the file is decoded beside the screen being shown */
        sc.px = malloc((size_t)sc.stride * (size_t)sc.h);
        const uint8_t *t = sc.px ? thumb_in(s, num, &sc) : 0; /* one file decoded a frame; no memory: the tile stays empty */
        free(sc.px);
        box_t b;
        gfx_t g;
        gal_tile_xy(&s->L, i, &x, &y);
        ui_g(s, &g);
        ui_fb_box(&g, x, y, x + s->L.tw - 1, y + s->L.th - 1, &b);
        box_clip(&b, s->fb.w, s->fb.h);
        if (s->dmg_n != dmg0) return redraw_boxes(s, &b, 0, 0); /* the "Damaged" tile: the screen drawn again (sets gal_todo) */
        if (t && s->want_full) gal_blit(&s->ui, s->orient, t, x, y); /* the screen's refresh is still to come: it takes ui */
        else if (t) {
            sk_cursor_hide(s);
            gal_blit(&s->ui, s->orient, t, x, y);
            cv_copy_box(&s->fb, &s->ui, &b);
            rs_add(&s->pend, &b, s->fb.w, s->fb.h);
            sk_menu_cursor(s);
        }
        return; /* gal_todo stays: the next frame looks again */
    }
    s->gal_todo = 0;
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

static void to_menu(sketch_t *s, int mode, int sel)
{
    s->mode = mode;
    s->act_sel = sel;
    sk_screen_render(s);
}

/* The page menu: the menu of the page on screen. The page is written first (Print and Send to phone work on its file) and
   stays in memory: Back brings it, the tool state and the toolbar back as they were */
void sk_to_pagemenu(sketch_t *s, uint32_t now)
{
    if (s->mode != SK_DRAW) return;
    sk_mp_close(s);
    if (!sk_save_page(s, now)) return sk_toast(s, "Could not save this page", now);
    if (!s->ui.px) {
        s->ui.px = malloc((size_t)s->fb.stride * (size_t)s->fb.h);
        s->ui.w = s->fb.w, s->ui.h = s->fb.h, s->ui.stride = s->fb.stride;
        if (!s->ui.px) return sk_toast(s, "Not enough memory for the menu", now);
    }
    sk_cursor_hide(s);
    sk_note_hide(s);
    s->cursor = 0;
    s->tb_back = s->tb_on;
    s->tb_on = s->tb_hold = 0; /* the toolbar is for drawing only */
    s->act_num = s->cur;
    s->exit_unsaved = s->need_setup = 0;
    s->ptr_down = 0;
    to_menu(s, SK_PAGEMENU, 0); /* Back is selected first: a tap on Select closes the menu, it never prints */
}

/* The gallery, from the page menu: the page on screen is not left until another one is picked */
void sk_to_gallery(sketch_t *s, uint32_t now)
{
    (void)now;
    if (s->mode != SK_PAGEMENU || !s->ui.px) return;
    pg_scan(&s->pg);
    s->dmg_n = 0;
    s->gal_first = -1;
    s->sel = gidx_of(s, s->cur);
    s->gnote[0] = 0;
    s->ptr_down = 0;
    to_menu(s, SK_GALLERY, s->act_sel);
}

static void to_draw(sketch_t *s)
{
    s->cursor = 0; /* the page comes back whole */
    free(s->ui.px);
    s->ui.px = 0; /* the thumbnails stay cached for the next visit (a save drops its page's) */
    s->mode = SK_DRAW;
    sk_resume_draw(s);
    s->want_full = 1;
}

static void gal_activate(sketch_t *s, uint32_t now)
{
    int num = gnum(s, s->sel);
    if (num == 0) {
        if (sk_is_blank(&s->page)) { /* the one blank page that may exist: it opens, no second one is made (owner 2026-11-01) */
            to_draw(s);
            return sk_toast(s, "This page is still empty", now);
        }
        to_draw(s);
        return sk_make_page(s, now);
    }
    if (!is_dmg(s, num) && num != s->cur && !gal_find(&s->cache, num)) { /* a tile picked before its thumbnail came: is the file good? */
        canvas_t sc = s->ui;
        sc.px = malloc((size_t)sc.stride * (size_t)sc.h);
        if (sc.px) thumb_in(s, num, &sc);
        free(sc.px);
    }
    if (is_dmg(s, num)) { /* cannot be opened for drawing; its one action rescues the file */
        s->act_num = num;
        return to_menu(s, SK_ACTIONS, 1);
    }
    to_draw(s);
    if (num == s->cur) return;
    if (!sk_leave_page(s, now)) return sk_toast(s, "Could not save this page", now);
    pg_scan(&s->pg);
    sk_open_page(s, num, now);
}

/* Back: the screen before this one (the drawing from the page menu and the gallery) */
static void menu_back(sketch_t *s)
{
    switch (s->mode) {
    case SK_ACTIONS: to_menu(s, SK_GALLERY, s->act_sel); break;
    case SK_CONFIRM: to_menu(s, SK_PAGEMENU, s->exit_unsaved ? 7 : 1); break; /* the row that opened it */
    case SK_RESULT: to_menu(s, SK_PAGEMENU, 0); break;
    case SK_SETTINGS: to_menu(s, SK_PAGEMENU, 3); break; /* the row that opened it */
    default: to_draw(s); /* the page menu, the gallery */
    }
}

static void share_from(sketch_t *s)
{
    sk_cursor_hide(s);
    /* the phone page: the files (download the drawings) */
    s->ret_mode = s->mode;
    s->mode = SK_SHARE;
    s->share_req = 1;
    s->share_rc = 0;
    memset(&s->share_shown, 0xff, sizeof(s->share_shown));
}

static void print_start(sketch_t *s)
{
    s->ret_mode = SK_PAGEMENU;
    if (sk_is_blank(&s->page)) {
        memset(&s->pres, 0, sizeof(s->pres));
        sk_cpy(s->pres.msg, sizeof(s->pres.msg), "This page is empty: nothing to print.");
        s->need_setup = 0;
        return to_menu(s, SK_RESULT, 0);
    }
    if (sk_print_check(s) < 0) return to_menu(s, SK_RESULT, 0);
    s->exit_unsaved = 0;
    to_menu(s, SK_CONFIRM, 0); /* Back first */
}

/* A setting row stepped: only its row and the note line are redrawn. A failed save says so on the note line */
static void settings_step(sketch_t *s, int dir)
{
    box_t a, n;
    sel_box(s, s->act_sel, &a);
    int rc = sks_step(s, s->act_sel - 1, dir);
    sk_cpy(s->gnote, sizeof(s->gnote), rc ? "Could not save this setting" : "");
    if (s->want_full) return sk_screen_render(s);
    note_box(s, &n);
    redraw_boxes(s, &a, &n, 0);
}

static void list_select(sketch_t *s, uint32_t now)
{
    if (s->mode == SK_PAGEMENU) {
        switch (s->act_sel) { /* the rows: Back on top, then the actions */
        case 0: to_draw(s); break;
        case 1: print_start(s); break;
        case 2: share_from(s); break;
        case 3: s->gnote[0] = 0, to_menu(s, SK_SETTINGS, 1); break; /* Back is row 0; the first setting is selected */
        case 4: sk_set_paper(s, (s->paper + 1) % PP_N, now), to_menu(s, SK_PAGEMENU, 4); break; /* Paper cycles in place */
        case 5: to_draw(s), sk_new_page(s, now); break;
        case 6: sk_to_gallery(s, now); break;
        default: /* Exit Sketch: the page is saved, the app returns as after a long press; only a failed save asks first */
            if (sk_save_page(s, now)) s->exit_req = 1;
            else s->exit_unsaved = 1, to_menu(s, SK_CONFIRM, 1);
        }
    } else if (s->mode == SK_SETTINGS) {
        if (s->act_sel == 0) menu_back(s);
        else settings_step(s, 1);
    } else if (s->mode == SK_ACTIONS) {
        if (s->act_sel == 1) share_from(s);
        else menu_back(s);
    } else if (s->mode == SK_CONFIRM) {
        if (s->act_sel == 0) return menu_back(s); /* Back, the top row */
        if (s->exit_unsaved) s->exit_req = 1;
        else s->mode = SK_BUSY, s->print_req = 1, sk_screen_render(s);
    }
}

static void menu_move(sketch_t *s, int dir)
{
    int v, n;
    if (s->mode == SK_GALLERY) v = s->sel + dir, n = gcount(s);
    else if (is_list(s)) v = s->act_sel + dir, n = s->list_n;
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
        if (gesture == APP_GESTURE_TAP) menu_back(s);
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
    for (int c = CTL_UP; c <= CTL_SETUP; c++) {
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
    else if (c == CTL_SETUP) s->setup_req = 1; /* the glue starts Print's setup and ends the app */
    else if (c == CTL_BACK || s->mode == SK_RESULT) menu_back(s);
    else if (s->mode == SK_GALLERY) gal_activate(s, now);
    else list_select(s, now);
}

/* The tablet's buttons in the menu screens: 1 and 2 (and 7, 8 in lists) move the selection, 7 and 8 turn the gallery's
   screen of tiles (in the page menu 7 and 8 close it, as they opened it), 4 selects, 5 goes back, 6 is the New page tile */
void sk_menu_key(sketch_t *s, unsigned down, uint32_t now)
{
    if (s->mode == SK_BUSY) return;
    if (s->mode == SK_SHARE) return sk_touch(s, APP_TOUCH_MIDDLE, APP_GESTURE_TAP, now);
    if (s->mode == SK_RESULT) { /* button 6 sets up the printer when that is what the screen offers */
        if (s->need_setup && (down >> 5 & 1)) s->setup_req = 1;
        else if (down & 0x18u) menu_back(s);
        return;
    }
    for (int b = 0; b < 8; b++) {
        int gal = s->mode == SK_GALLERY, pm = s->mode == SK_PAGEMENU || s->mode == SK_SETTINGS;
        if (!(down >> b & 1) || (s->mode < SK_GALLERY)) continue;
        if (b == 0 || (b == 6 && !gal && !pm)) menu_move(s, -1);
        else if (b == 1 || (b == 7 && !gal && !pm)) menu_move(s, 1);
        else if (b == 6 || b == 7) gal ? gal_page(s, b == 6 ? -1 : 1) : menu_back(s);
        else if (b == 3) ctl_press(s, CTL_SEL, now);
        else if (b == 5 && gal) sel_set(s, 0), gal_activate(s, now);
        else if (b == 4) menu_back(s);
    }
}

/* What the pen or mouse points at in the screen of the current mode: a gallery index, a list row, or -1 */
static int item_at(const sketch_t *s, int ux, int uy)
{
    if (s->mode == SK_GALLERY) {
        int slot = gal_hit(&s->L, ux, uy), idx = (s->sel / s->L.per) * s->L.per + slot;
        return slot >= 0 && idx < gcount(s) ? idx : -1;
    }
    if (is_list(s)) {
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
    else if (s->mode == SK_SETTINGS && s->ptr_item > 0) { /* the left half of the row steps back, the right half forward */
        s->act_sel = s->ptr_item;
        settings_step(s, ux < (s->lg.x - 30 + s->lg.x1) / 2 ? -1 : 1);
    } else if (s->mode != SK_BUSY) s->act_sel = s->ptr_item, list_select(s, now);
}

void sk_run_print(sketch_t *s)
{
    if (!s->pjob) {
        s->cursor = 0;
        gal_cache_free(&s->cache); /* memory for the job */
        free(s->ui.px);
        s->ui.px = 0;
        if ((s->pjob = sk_print_start(s, s->act_num))) return; /* its rows from the next turn on */
    } else {
        if (pj_job_step(s->pjob, SK_PRINT_ROWS) > 0) return;
        pj_job_free(s->pjob);
        s->pjob = 0;
    }
    s->print_req = 0;
    s->ui.px = malloc((size_t)s->fb.stride * (size_t)s->fb.h);
    if (!s->ui.px) { /* no memory for the result screen: back to the page, with the reason as a note */
        to_draw(s);
        return sk_toast(s, s->pres.pages ? "Printed" : "Not printed", s->last_push);
    }
    if (s->pres.pages && s->pres.jobs) s->printed_num = s->act_num, s->printed_rev = s->page_rev;
    s->mode = SK_RESULT;
    s->ret_mode = SK_PAGEMENU;
    s->need_setup = 0;
    sk_screen_render(s);
}

