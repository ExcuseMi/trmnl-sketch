/* The Sketch app without the firmware: pen, tablet buttons and touch bar in; pixels in the framebuffer and the
   rectangles to push out. The same code runs on the X (apps/trmnl-sketch/main.c) and in the host tests.
   SPDX-License-Identifier: MIT */
#ifndef SKETCH_H
#define SKETCH_H
#include <stdint.h>
#include <app_api.h>
#include "canvas.h"
#include "gfx.h"
#include "hist.h"
#include "pages.h"
#include "penmap.h"
#include "rects.h"
#include "skui.h"

typedef struct {
    const sk_fs_t *fs;
    void *user;
    int (*cfg_get)(void *user, const char *key, char *buf, int cap); /* app settings; length or < 0 */
    int (*cfg_set)(void *user, const char *key, const char *val, int n);
} sk_env_t;

enum { SK_DRAW, SK_MENU, SK_SHARE };
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
    int size, eraser;    /* pen size 0..2; eraser toggled on */
    int tablet;          /* a tablet is connected (pen_info) */
    int stroke, barrel;  /* a stroke is open; a pen button is held */
    int stroke_erase;    /* the open stroke erases */
    int hover_valid, hover_x, hover_y; /* the pen hovers over the page there */
    uint32_t um_w, um_h; /* the tablet's area, micrometres (0: unknown) */
    unsigned pressure;   /* smoothed */
    int cursor, cx, cy, cr; /* the hover cursor is drawn at fb (cx, cy) with radius cr */
    box_t note;          /* the corner note (legend or toast) on screen, when note_on */
    int note_on, legend;
    uint32_t note_until;
    int dirty, bad_pending, save_failed;
    uint32_t save_at;
    rset_t pend;
    uint32_t last_push;
    int want_full, share_req;
    share_view_t share, share_shown;
    int share_rc;
    uint32_t pushed_px;  /* statistics: pixels and rectangles handed out by sk_flush */
    uint32_t pushed_rects;
} sketch_t;

/* fb: the panel's framebuffer (w x h, stride). Returns 0, or APP_ENOMEM */
int sk_init(sketch_t *s, const sk_env_t *env, uint8_t *fb, int w, int h, int stride, int orient, const app_touchbar_t *bar);
void sk_free(sketch_t *s);
void sk_set_tablet(sketch_t *s, uint32_t width_um, uint32_t height_um, int connected);
void sk_set_orient(sketch_t *s, int orient, const app_touchbar_t *bar);

void sk_pen(sketch_t *s, unsigned flags, unsigned x, unsigned y, unsigned pressure, uint32_t now);
void sk_pen_key(sketch_t *s, unsigned keys, unsigned changed, uint32_t now);
void sk_touch(sketch_t *s, int zone, int gesture, uint32_t now);
/* The Wi-Fi page's start result and status (SK_SHARE mode), about once a second */
void sk_share_update(sketch_t *s, int start_rc, const app_webshare_status_t *st);
/* Autosave and notes timing */
void sk_tick(sketch_t *s, uint32_t now);
/* The pages saved now (the app ends or is told to) */
void sk_save_now(sketch_t *s);
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
#endif
