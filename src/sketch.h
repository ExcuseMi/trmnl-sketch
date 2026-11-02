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
#include "mirror.h"
#include "pages.h"
#include "paper.h"
#include "penmap.h"
#include "rects.h"
#include "skmenu.h"
#include "skpills.h"
#include "sk_cfg.h"
#include "sklat.h"
#include "skpress.h"
#include "skstab.h"
#include "skui.h"
#include "toolbar.h"

typedef struct {
    const sk_fs_t *fs;
    void *user;
    int (*cfg_get)(void *user, const char *key, char *buf, int cap); /* app settings; length or < 0 */
    int (*cfg_set)(void *user, const char *key, const char *val, int n);
    pj_net_t net;        /* the printer connection (post == NULL: this firmware cannot print) */
    void (*log)(void *user, int ev_med, int ev_p95, int push_med, int push_p95, int n); /* drawing latency, a line in the log every SK_LAT_LOG_MS while drawing (NULL: none) */
    int (*printer)(void *user, pj_printer_t *out); /* the default printer (pj_default_printer): 0, PJ_NO_PRINTER, APP_ENOTSUP, APP_EIO */
} sk_env_t;

enum { SK_DRAW, SK_SHARE, SK_GALLERY, SK_ACTIONS, SK_CONFIRM, SK_BUSY, SK_RESULT, SK_PAGEMENU }; /* from SK_GALLERY on: menu screens drawn from `ui` */
#define SK_DMG_MAX 32
#define SK_BLITS 40
enum { SK_NONE = 0, SK_FULL = -1, SK_NOFLASH = -2 }; /* sk_flush: nothing due / a full refresh; SK_NOFLASH: only inside (sk_flash_step) */

/* The inputs of a strip paint: the same key, the same pixels (sktbimg.c) */
typedef struct {
    bstyle_t sty, rc[SK_RECENT];
    int hot, rc_n, smooth, orient;
    box_t box;
} tb_key_t;

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
    uint8_t ink_size, era_size; /* the size of the ink brushes and of the eraser (sty.size is the one of the brush picked) */
    int tb_drag;         /* a press on a slider is still down (1 size, 2 tone): the knob follows the pen, no ink */
    bstyle_t stk;        /* the open stroke's style */
    int tb_hot, tb_ptr;  /* the toolbar item the pen is over (-1 none); the arrow pointer is drawn over the strip */
    box_t tb_ptr_box;
    int tb_on, tb_hold;  /* the toolbar is up; a pen press that began on it is still down */
    tb_layout_t tb;
    box_t tb_box;        /* the strip in the framebuffer, whole bytes */
    box_t tb_px;         /* the strip in the framebuffer, exact pixels (tb_box without the byte rounding) */
    tb_key_t tb_key;     /* what the strip image was painted from (sktbimg.c) */
    uint8_t *tb_img;     /* the painted strip, tb_box's bytes (NULL: none, paint again) */
    size_t tb_img_cap;
    int tb_img_ok;
    uint32_t tb_paints;  /* statistics: whole-strip paints (tb_draw); a pen move over the strip must not cost one */
    box_t flash;         /* to flash (sk_flash), empty: none */
    box_t fl_box;        /* the flash under way: its inverted half was pushed, the clean half is next */
    int fl_phase;
    int tip;             /* the pen's tip is down */
    int ovl_gone;        /* a note or the toolbar left the page: one whole-page flash when the tip is up */
    int ghost;           /* a partial refresh was pushed since the last whole-screen flash */
    int cleanup_s;       /* setting cleanup_s: idle seconds before the whole screen is flashed clean (0: off) */
    uint32_t last_act;   /* the last pen, button or touch event */
    int lx, ly, px, py, lp; /* the open stroke's last two points and the byte of the last, for the tail */
    int tablet;          /* a tablet is connected (pen_info) */
    int stroke, barrel;  /* a stroke is open; a pen button is held */
    stab_t stab;         /* the stabiliser of the open stroke (skstab.h) */
    lat_t lat_ev, lat_push; /* event to push issued, push duration (sklat.h) */
    int ink_pend, lat_new, dbg; /* ink waits to be pushed; samples since the last log line; the debug toast is on */
    uint32_t ink_t, lat_next, k8_down; /* when that ink began; when the next log line is due; when tablet button 8 went down */
    int use_tilt, tilt;  /* setting use_tilt (default on); the tilt code of the pen's last event (sk_pen_tilt) */
    int pcurve;          /* setting pressure_curve: PC_SOFT, PC_NORMAL (default), PC_FIRM (skpress.h) */
    int smooth;          /* the stabiliser 0..10 (setting smooth, default 3; the toolbar's Smooth cell) */
    int rx, ry, stroke_pts; /* the pen's own last point of the open stroke, and how many points it has had */
    int sh_on, sh_x0, sh_y0, sh_x1, sh_y1, sh_barrel; /* a shape is being dragged (skshape.c): start, end, constrain was on at the last preview */
    int paper, paper_sp, paper_def, paper_sp_def; /* the page on screen's paper (paper.h) and the settings' default for new pages */
    box_t sh_box;        /* the rubber band on the screen (framebuffer only, never in the page) */
    int tx_on, tx_x, tx_y, tx_n, tx_cls, tx_down, tx_moved, tx_dx, tx_dy; /* the text being typed (sktext.c): origin, length, size class, the tap */
    uint16_t tx_cp[SK_TEXT_MAX];
    box_t tx_box;        /* its drawing on the screen (framebuffer only until it ends) */
    uint32_t kbd;        /* APP_KBD_* bits connected now */
    int kbd_req;         /* the Text tool wants the glue to enable the keyboards (keyboard_enable) */
    bstyle_t rc[SK_RECENT]; /* the last tone + brush + size combinations drawn with, most recent first (skrecent.c) */
    int rc_n;
    unsigned mods;       /* keyboard modifiers held (APP_MOD_*) */
    int stroke_erase;    /* the open stroke erases */
    int hover_valid, hover_x, hover_y; /* the pen hovers over the page there */
    uint32_t um_w, um_h; /* the tablet's area, micrometres (0: unknown) */
    int turn;            /* setting tablet_turn (auto): the tablet is used turned in portrait (penmap.h) */
    int fill;            /* setting pen_area: the tablet's whole area on the whole page (else the aspect is kept) */
    unsigned pressure;   /* smoothed */
    int bk_gap;          /* setting fill_gap: gaps up to this many px close for the bucket (0..6) */
    int bk_down, bk_moved, bk_x, bk_y; /* a bucket tap: the tip is down (where), and moved too far to be a tap */
    uint32_t bk_ask_until; /* "Fill the whole page?" was asked: a tap before this confirms */
    int cursor, cx, cy, cr, cb, csz; /* the hover cursor is drawn at fb (cx, cy) with half size cr, for brush cb */
    box_t note;          /* the corner note (legend or toast) on screen, when note_on */
    int note_on, legend;
    uint32_t note_until;
    int dirty, bad_pending, save_failed;
    uint32_t save_at;
    rset_t pend;
    uint32_t last_push;
    int batch_ms;        /* new ink waits this long after the last push (SK_BATCH_MS; the speed run tries others) */
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
    pj_printer_t prn;    /* the default printer for the job on screen */
    int need_setup;      /* the result screen offers "Set up printer" */
    int setup_req, exit_req; /* the glue ends the app: after launching Print's setup / for "Exit Sketch" */
    int ret_mode;        /* the screen a share or result screen returns to (SK_PAGEMENU, SK_ACTIONS) */
    int exit_unsaved;    /* the confirm screen is "Exit without saving?" (the save failed), not the print one */
    int tb_back;         /* the toolbar was up when the page menu opened */
    uint16_t dmg[SK_DMG_MAX]; /* pages found damaged in the gallery (file does not decode) */
    int dmg_n;
    pills_t pl;          /* the Tools and Menu pills on the drawing page (overlay, like the toolbar) */
    box_t pl_box;
    int pl_hold;         /* a pen press that began on a pill (1 Tools, 2 Menu) is still down */
    int pl_on;           /* the pills are shown (skplvis.c: only while the pen hovers near them) */
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
/* Reads the settings the phone page can change (pen_area) again; the app calls it when that page closes */
void sk_cfg_reload(sketch_t *s);

/* Brush, shade and size (the toolbar and the settings call these; each toasts its name) */
void sk_set_brush(sketch_t *s, int brush, uint32_t now);
void sk_set_shade(sketch_t *s, int shade, uint32_t now);
void sk_set_size(sketch_t *s, int size, uint32_t now);
/* The stabiliser 0..10 (the toolbar's Smooth cell cycles 0, 3, 6, 10; the setting `smooth` is its default) */
void sk_set_smooth(sketch_t *s, int level, uint32_t now);
/* The symmetry (skmirror.c): off, left-right, top-bottom, 4-way, kaleidoscope; the guide is on the screen only */
void sk_set_mirror(sketch_t *s, int mode, uint32_t now);
/* The size slider (sksize.c): the knob follows the pen at user x `ux`; only the slider's cells are pushed */
void sk_slider_drag(sketch_t *s, int ux);
/* sketch.c's own, shared with sksize.c: a setting's number, a toast with the strip repainted, the strip repainted in the framebuffer */
void sk_cfg_num(sketch_t *s, const char *key, int v);
void sk_picked(sketch_t *s, const char *text, uint32_t now);
void sk_tb_paint(sketch_t *s);                    /* the whole strip in the framebuffer (drawn only when its inputs changed: sktbimg.c) */
void sk_tb_restore(sketch_t *s, const box_t *b);  /* the strip's pixels back inside b (an arrow or the page laid over them) */
void sk_tb_img_free(sketch_t *s);
/* The pills follow the pen (skplvis.c): shown while it hovers near them, hidden while it draws or hovers elsewhere;
   out of range: as they were. tip, range, inside: the pen event; ux, uy: the user frame */
void sk_pills_follow(sketch_t *s, int tip, int range, int inside, int ux, int uy);
void sk_pills_show(sketch_t *s, int on);
void sk_pl_paint(sketch_t *s);
/* Shows the toolbar, or hides it again (the same tablet button): only the strip is refreshed, clean; the page under it is
   kept and comes back as it was */
void sk_toolbar(sketch_t *s, uint32_t now);
/* The tilt of the pen event that sk_pen takes next (degrees, as the tablet reports; 0 0 for a mouse or a tablet that does not tell) */
void sk_pen_tilt(sketch_t *s, int tilt_x, int tilt_y);
int sk_tilt_code(const penmap_t *m, int tilt_x, int tilt_y); /* the same, as the byte a point carries */
void sk_pen(sketch_t *s, unsigned flags, unsigned x, unsigned y, unsigned pressure, uint32_t now);
void sk_pen_key(sketch_t *s, unsigned keys, unsigned changed, uint32_t now);
void sk_touch(sketch_t *s, int zone, int gesture, uint32_t now);
/* The Wi-Fi page's start result and status (SK_SHARE mode), about once a second */
void sk_share_update(sketch_t *s, int start_rc, const app_webshare_status_t *st);
/* Autosave and notes timing */
void sk_tick(sketch_t *s, uint32_t now);
/* Latency (sklatency.c): sk_lat_push is what the glue reports after a push call (its duration in ms); a long press of tablet button 8 toggles the debug toast */
void sk_lat_push(sketch_t *s, uint32_t ms);
void sk_lat_ink(sketch_t *s, uint32_t now);
void sk_lat_issued(sketch_t *s, uint32_t now);
void sk_lat_tick(sketch_t *s, uint32_t now);
void sk_lat_debug(sketch_t *s, uint32_t now);
/* The pages saved now (the app ends or is told to) */
void sk_save_now(sketch_t *s);
/* After sk_flush showed the "Printing" screen (print_req): sends the page, sets the result screen */
void sk_run_print(sketch_t *s);
/* Sends page `num`'s saved file to the printer; 0 or < 0 with s->pres.msg set (skprint.c) */
int sk_print_page(sketch_t *s, int num);
/* The shape tools (skshape.c). sk_shape_end constrains the end point; sk_shape_pts gives the outline as a polyline (the
   points of the stroke); sk_shape_pen takes the pen events of a shape tool (1: taken); the preview is on the screen only */
#define SK_SHAPE_PTS 140
void sk_shape_end(int tool, int x0, int y0, int *x1, int *y1, int constrain);
int sk_shape_pts(int tool, int x0, int y0, int x1, int y1, int16_t (*pt)[2], int cap);
int sk_shape_pen(sketch_t *s, int tip, int inside, int fx, int fy, uint32_t now);
void sk_shape_cancel(sketch_t *s);
void sk_set_tool(sketch_t *s, int tool, uint32_t now);
void sk_fb_restore(sketch_t *s, const box_t *b); /* the page (and strip, pills) back on the screen in b, and pushed */
void sk_mark(sketch_t *s, const box_t *b);       /* b is to be pushed */
/* The Text tool (sktext.c). sk_key: a key event of a keyboard (down 0: released); sk_keyboard: the keyboards connected now */
int sk_text_pen(sketch_t *s, int tip, int inside, int fx, int fy, uint32_t now);
void sk_text_end(sketch_t *s, uint32_t now);
void sk_keyboard(sketch_t *s, uint32_t bits, uint32_t now);
void sk_key(sketch_t *s, int down, unsigned key, unsigned mods, unsigned hid, const char *utf8, uint32_t now);
/* The Recent combinations (skrecent.c): sk_recent_add when a stroke ends (the eraser and the bucket never enter), sk_recent_pick restores slot i */
void sk_recent_load(sketch_t *s);
void sk_recent_add(sketch_t *s, const bstyle_t *st);
void sk_recent_pick(sketch_t *s, int slot, uint32_t now);
/* The paper (skpaper.c) */
void sk_paper_new(sketch_t *s);               /* the default for a new page */
void sk_paper_open(sketch_t *s, int num);     /* the paper of page num (Blank for a page saved before papers existed) */
void sk_paper_store(sketch_t *s);             /* beside the saved page: sidecar and printed copy */
void sk_paper_blit(sketch_t *s, const box_t *b); /* the paper over the framebuffer box (just copied from the page) */
void sk_set_paper(sketch_t *s, int kind, uint32_t now);
/* The bucket (skfill.c): the pen with the Fill tool, true when it took the event; a tap at (x, y) fills */
int sk_fill_pen(sketch_t *s, int tip, int inside, int fx, int fy, uint32_t now);
void sk_fill_at(sketch_t *s, int x, int y, uint32_t now);
void sk_page_changed(sketch_t *s, const box_t *b, uint32_t now); /* a change of the page in box b reaches the screen */
void sk_cpy(char *d, size_t cap, const char *s); /* bounded copy */
/* What to push: SK_FULL (the whole screen, clean), else the number of rectangles in out (SK_MAX_RECTS at most) */
int sk_flush(sketch_t *s, uint32_t now, rect_t *out);
/* The one way to clean the glass: the area is pushed inverted (partially, fast), then as it is with a full refresh.
   sk_flush hands out the two halves in two calls (sk_flash_busy after the first: call again at once, before any input
   is handled: the inverted area is in the framebuffer until then). Page, gallery and menu switches (want_full), the
   toolbar strip and the clean-up rules all use it (skflash.c) */
void sk_flash(sketch_t *s, const box_t *area);
int sk_flash_busy(const sketch_t *s);
int sk_flash_step(sketch_t *s, uint32_t now, rect_t *out); /* inside sk_flush */
uint32_t sk_cleanup_due(const sketch_t *s, uint32_t now);
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
void sk_to_gallery(sketch_t *s, uint32_t now); /* from the page menu */
void sk_to_pagemenu(sketch_t *s, uint32_t now); /* from the drawing: the menu of the current page */
int sk_do_save(sketch_t *s, uint32_t now); /* writes the page; 0 or the pages.h error (skpage.c) */
int sk_save_page(sketch_t *s, uint32_t now); /* writes the page on screen if changed; 0 when it could not */
void sk_resume_draw(sketch_t *s); /* the toolbar and pills as they were, for the page coming back */
int sk_print_check(sketch_t *s); /* the default printer into s->prn: 0, or -1 with the result words set */
void sk_menu_touch(sketch_t *s, int zone, int gesture, uint32_t now);
void sk_menu_key(sketch_t *s, unsigned down, uint32_t now); /* tablet buttons (bit 0 is button 1) in a menu screen */
void sk_menu_pen(sketch_t *s, unsigned flags, int fx, int fy, int inside, uint32_t now);
#endif
