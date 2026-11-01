/* The Sketch app without the firmware: pen, tablet buttons and touch bar in; pixels in the framebuffer and the
   rectangles to push out. The same code runs on the X (apps/trmnl-sketch/main.c) and in the host tests.
   SPDX-License-Identifier: MIT */
#ifndef SKETCH_H
#define SKETCH_H
#include <stdint.h>
#include <app_api.h>
#include "brush.h"
#include "canvas.h"
#include "gallery.h"
#include "printjob.h"
#include "gfx.h"
#include "hist.h"
#include "pages.h"
#include "penmap.h"
#include "rects.h"
#include "skmenu.h"
#include "skui.h"
#include "toolbar.h"

typedef struct {
    const sk_fs_t *fs;
    void *user;
    int (*cfg_get)(void *user, const char *key, char *buf, int cap); /* app settings; length or < 0 */
    int (*cfg_set)(void *user, const char *key, const char *val, int n);
    pj_net_t net;        /* the printer connection (post == NULL: this firmware cannot print) */
} sk_env_t;

enum { SK_DRAW, SK_SHARE, SK_GALLERY, SK_ACTIONS, SK_CONFIRM, SK_BUSY, SK_RESULT }; /* from SK_GALLERY on: menu screens drawn from `ui` */
#define SK_PRINTER_DEFAULT "192.0.2.40"
#define SK_BLITS 40
enum { SK_NONE = 0, SK_FULL = -1 }; /* sk_flush: nothing due / a full refresh */

typedef struct {
    sk_env_t env;
    canvas_t fb, page;
    hist_t hist;
    pages_t pg;
    penmap_t map;
    app_touchbar_t bar;
    int orient, mode;
    int cur;             /* page number on screen */
    bstyle_t sty;        /* the brush, shade and size picked (brush may be the eraser) */
    int last_ink;        /* the brush before the eraser was picked */
    bstyle_t stk;        /* the open stroke's style */
    int tb_hot, tb_ptr;  /* the toolbar item the pen is over (-1 none); the arrow pointer is drawn over the strip */
    box_t tb_ptr_box;
    int tb_on, tb_hold;  /* the toolbar is up; a pen press that began on it is still down */
    tb_layout_t tb;
    box_t tb_box;        /* the strip in the framebuffer, whole bytes */
    box_t flash;         /* to refresh clean (the strip toggled), empty: none */
    int lx, ly, px, py, lp; /* the open stroke's last two points and the byte of the last, for the tail */
    int tablet;          /* a tablet is connected (pen_info) */
    int stroke, barrel;  /* a stroke is open; a pen button is held */
    int stroke_erase;    /* the open stroke erases */
    int hover_valid, hover_x, hover_y; /* the pen hovers over the page there */
    uint32_t um_w, um_h; /* the tablet's area, micrometres (0: unknown) */
    unsigned pressure;   /* smoothed */
    int cursor, cx, cy, cr, cb; /* the hover cursor is drawn at fb (cx, cy) with half size cr, for brush cb */
    box_t note;          /* the corner note (legend or toast) on screen, when note_on */
    int note_on, legend;
    uint32_t note_until;
    int dirty, bad_pending, save_failed;
    uint32_t save_at;
    rset_t pend;
    uint32_t last_push;
    int want_full, share_req, share_kind; /* share_req: 1 start the files page, 2 the settings page, -1 stop; share_kind: which one is on screen */
    share_view_t share, share_shown;
    int share_rc;
    /* gallery and menus (1-bit like the page, so the pointer and the highlight need only partial refreshes) */
    canvas_t ui;         /* the clean screen of a menu (chrome and thumbnails); px NULL outside them */
    box_t cur_box;       /* the menus' arrow pointer on screen (cursor set), in the framebuffer */
    gal_layout_t L;
    gal_cache_t cache;
    int sel, act_sel;    /* selected gallery tile (0 new page, 1 newest page ...), selected list item */
    int act_num;         /* the page the action list and Print are about */
    list_geom_t lg;      /* the list as drawn, for the pen */
    int list_n;
    char gnote[64];
    int print_req;
    pj_result_t pres;
    struct { int num, x, y; } blit[SK_BLITS]; /* thumbnails to lay over `ui` */
    int blit_n;
    int hot, ptr_ctl;    /* the control (CTL_*) the pen hovers over / a press began on */
    int ptr_down, ptr_moved, ptr_item, ptr_x, ptr_y; /* a pen or mouse press on a menu */
    uint32_t pushed_px;  /* statistics: pixels and rectangles handed out by sk_flush */
    uint32_t pushed_rects;
} sketch_t;

/* fb: the panel's framebuffer (w x h, stride). Returns 0, or APP_ENOMEM */
int sk_init(sketch_t *s, const sk_env_t *env, uint8_t *fb, int w, int h, int stride, int orient, const app_touchbar_t *bar);
void sk_free(sketch_t *s);
void sk_set_tablet(sketch_t *s, uint32_t width_um, uint32_t height_um, int connected);
void sk_set_orient(sketch_t *s, int orient, const app_touchbar_t *bar);

/* Brush, shade and size (the toolbar and the settings call these; each toasts its name) */
void sk_set_brush(sketch_t *s, int brush, uint32_t now);
void sk_set_shade(sketch_t *s, int shade, uint32_t now);
void sk_set_size(sketch_t *s, int size, uint32_t now);
/* Shows the toolbar, or hides it again (the same tablet button): only the strip is refreshed, clean; the page under it is
   kept and comes back as it was */
void sk_toolbar(sketch_t *s, uint32_t now);
void sk_pen(sketch_t *s, unsigned flags, unsigned x, unsigned y, unsigned pressure, uint32_t now);
void sk_pen_key(sketch_t *s, unsigned keys, unsigned changed, uint32_t now);
void sk_touch(sketch_t *s, int zone, int gesture, uint32_t now);
/* The Wi-Fi page's start result and status (SK_SHARE mode), about once a second */
void sk_share_update(sketch_t *s, int start_rc, const app_webshare_status_t *st);
/* Autosave and notes timing */
void sk_tick(sketch_t *s, uint32_t now);
/* The pages saved now (the app ends or is told to) */
void sk_save_now(sketch_t *s);
/* After sk_flush showed the "Printing" screen (print_req): sends the page, sets the result screen */
void sk_run_print(sketch_t *s);
/* Sends page `num`'s saved file to the printer; 0 or < 0 with s->pres.msg set (skprint.c) */
int sk_print_page(sketch_t *s, int num);
void sk_cpy(char *d, size_t cap, const char *s); /* bounded copy */
/* What to push: SK_FULL (the whole screen, clean), else the number of rectangles in out (SK_MAX_RECTS at most) */
int sk_flush(sketch_t *s, uint32_t now, rect_t *out);
/* Milliseconds until sk_tick or sk_flush has something to do (idle: a large number) */
uint32_t sk_next_due(const sketch_t *s, uint32_t now);
/* Actions, also what the buttons and the touch bar call */
void sk_undo(sketch_t *s, uint32_t now);
void sk_redo(sketch_t *s, uint32_t now);
void sk_clear(sketch_t *s, uint32_t now);
void sk_goto_page(sketch_t *s, int index, uint32_t now); /* index into the page list */
void sk_new_page(sketch_t *s, uint32_t now);
/* Internal, between sketch.c (drawing, pages, saving) and skgal.c (gallery and menus) */
void sk_cursor_hide(sketch_t *s);
void sk_note_hide(sketch_t *s);
void sk_toast(sketch_t *s, const char *text, uint32_t now);
int sk_leave_page(sketch_t *s, uint32_t now);
void sk_make_page(sketch_t *s, uint32_t now);
void sk_open_page(sketch_t *s, int num, uint32_t now);
int sk_is_blank(const canvas_t *c);
void sk_show_page(sketch_t *s);
void sk_screen_render(sketch_t *s); /* draws the menu screen of the mode into `ui` and asks for a full refresh */
void sk_menu_cursor(sketch_t *s); /* draws the pointer on the framebuffer at the hover place, when a menu shows one */
void sk_to_gallery(sketch_t *s, uint32_t now);
void sk_menu_touch(sketch_t *s, int zone, int gesture, uint32_t now);
void sk_menu_key(sketch_t *s, unsigned down, uint32_t now); /* tablet buttons (bit 0 is button 1) in a menu screen */
void sk_menu_pen(sketch_t *s, unsigned flags, int fx, int fy, int inside, uint32_t now);
#endif
