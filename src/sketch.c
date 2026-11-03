/* SPDX-License-Identifier: MIT */
#include "sketch.h"
#include <stdlib.h>
#include <string.h>
#include "fmath.h"
#include "mirror.h"
#include "sk_cfg.h"

#define TOAST_MS 2500
#define LEGEND_MS 40000

static void mkg(const sketch_t *s, gfx_t *g) { gfx_init(g, s->fb.px, s->fb.w, s->fb.h, s->fb.stride, s->orient); }
static void mark(sketch_t *s, const box_t *b) { rs_add(&s->pend, b, s->fb.w, s->fb.h); }
static int hit(const box_t *a, const box_t *b) { return !box_empty(a) && !box_empty(b) && a->x0 <= b->x1 && b->x0 <= a->x1 && a->y0 <= b->y1 && b->y0 <= a->y1; }
void sk_cfg_num(sketch_t *s, const char *key, int v)
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
static void tb_place(sketch_t *s);
/* The page in box b goes to the screen; the strip of an open toolbar is drawn over it again if the box touched it */
static void fb_put(sketch_t *s, const box_t *b)
{
    cv_copy_box(&s->fb, &s->page, b);
    sk_paper_blit(s, b);
    mr_guide(&s->fb, s->sty.mirror, b); /* the screen-only guide of the mirror (mirror.h) */
    if (s->tb_on && hit(b, &s->tb_box)) sk_tb_restore(s, b); /* from the strip's image: no tb_draw (sktbimg.c) */
    if (s->mp_on && hit(b, &s->mp_box)) sk_mp_paint(s);
}

void sk_fb_restore(sketch_t *s, const box_t *b)
{
    fb_put(s, b);
    mark(s, b);
}
void sk_mark(sketch_t *s, const box_t *b) { mark(s, b); }

void sk_show_page(sketch_t *s)
{
    box_t b;
    whole(&b, &s->page);
    cv_copy_box(&s->fb, &s->page, &b);
    sk_paper_blit(s, &b);
    mr_guide(&s->fb, s->sty.mirror, &b);
    if (s->tb_on) sk_tb_paint(s);
    sk_mp_paint(s);
}

void sk_resume_draw(sketch_t *s) /* back from a menu screen: the toolbar as it was, the page */
{
    if (s->tb_back) tb_place(s), s->tb_on = 1, s->tb_hot = -1;
    s->tb_back = 0;
    sk_show_page(s);
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
    ui_fb_box(&g, s->tb.cx[i], s->tb.cyi[i], s->tb.cx[i] + s->tb.cell - 1, s->tb.cyi[i] + s->tb.ch - 1, b);
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
    sk_cursor_hide(s); /* the arrow's old box: the strip comes back from its image */
    if (item != old) sk_tb_paint(s); /* the frame moved: the strip is drawn again, never for a move inside one item */
    box_t b;
    if (item != old)
        for (int k = 0; k < 2; k++) {
            int i = k ? item : old;
            if (i >= 0) tb_item_box(s, i, &b), mark(s, &b);
        }
    ui_arrow(&s->fb, s->orient, ux, uy, &b);
    s->cur_draws++;
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
        sk_tb_paint(s);
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

/* The size the brush `br` draws with: the size button picked, as that brush measures it */
static int size_for(const sketch_t *s, int br) { return brush_size(br, s->size_cls); }

static void cursor_show(sketch_t *s, int fx, int fy)
{
    int br = eff_brush(s), sz = size_for(s, br), r = brush_cursor_r(br, sz);
    box_t want = { fx - r - BR_CUR_OUT, fy - r - BR_CUR_OUT, fx + r + BR_CUR_OUT, fy + r + BR_CUR_OUT };
    if (s->cursor && s->cr == r && s->cb == br && s->csz == sz && (fx - s->cx) * (fx - s->cx) + (fy - s->cy) * (fy - s->cy) < 9) return;
    sk_cursor_hide(s);
    if (s->note_on && hit(&want, &s->note)) return; /* never over a note: restoring the page would eat it */
    if ((s->tb_on && hit(&want, &s->tb_box)) || (s->mp_on && hit(&want, &s->mp_box))) return; /* nor over the toolbar or the mirror picker */
    box_t b;
    ui_cursor(&s->fb, fx, fy, br, sz, &b);
    s->cur_draws++;
    mark(s, &b);
    s->cursor = 1, s->cx = fx, s->cy = fy, s->cr = r, s->cb = br, s->csz = sz;
}

void sk_note_hide(sketch_t *s)
{
    if (!s->note_on) return;
    fb_put(s, &s->note);
    mark(s, &s->note);
    if (!s->legend && s->mode == SK_DRAW) s->traces++;
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
void sk_page_changed(sketch_t *s, const box_t *b, uint32_t now)
{
    sk_cursor_hide(s);
    if (s->note_on && hit(b, &s->note)) sk_note_hide(s);
    fb_put(s, b);
    mark(s, b);
    s->traces++;
    s->dirty = 1;
    s->save_at = now + SK_AUTOSAVE_MS;
}

/* ---- actions ---- */

/* An undo or redo that had to draw the page again (no delta: hist.h) waits for this, once for a row of presses */
void sk_settle(sketch_t *s, uint32_t now)
{
    box_t b;
    if (hist_settle(&s->hist, &b)) sk_page_changed(s, &b, now);
}

void sk_undo(sketch_t *s, uint32_t now)
{
    box_t b;
    s->stroke = 0;
    if (hist_undo(&s->hist, &b)) {
        if (!box_empty(&b)) sk_page_changed(s, &b, now);
    }
    else sk_toast(s, "Nothing to undo", now);
}

void sk_redo(sketch_t *s, uint32_t now)
{
    box_t b;
    s->stroke = 0;
    if (hist_redo(&s->hist, &b)) {
        if (!box_empty(&b)) sk_page_changed(s, &b, now);
    }
    else sk_toast(s, "Nothing to redo", now);
}

void sk_clear(sketch_t *s, uint32_t now)
{
    hist_end(&s->hist);
    s->stroke = 0;
    if (sk_is_blank(&s->page)) return sk_toast(s, "The page is empty", now);
    if (s->dirty) sk_do_save(s, now);
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
    if (s->tb_on) sk_tb_paint(s), mark(s, &s->tb_box);
    sk_toast(s, s->sty.brush == BR_ERASER ? "Eraser on" : "Eraser off", now);
}

/* ---- brush, shade, size and the toolbar ---- */

void sk_picked(sketch_t *s, const char *text, uint32_t now)
{
    if (s->tb_on) sk_tb_paint(s), mark(s, &s->tb_box);
    sk_toast(s, text, now);
}

void sk_set_size(sketch_t *s, int cls, uint32_t now)
{
    if (cls < 0 || cls >= BR_SIZES) return;
    s->size_cls = cls;
    s->sty.size = (uint8_t)brush_size(s->sty.brush, cls);
    sk_cfg_num(s, "size_cls", cls);
    sk_picked(s, cls == 0 ? "Small" : cls == 1 ? "Medium" : "Big", now);
    ui_cursor_warm(s->sty.brush, size_for(s, s->sty.brush));
}

void sk_set_brush(sketch_t *s, int brush, uint32_t now)
{
    if (brush < 0 || brush >= BR_N) return;
    if (brush != BR_ERASER) s->last_ink = brush;
    s->sty.brush = (uint8_t)brush;
    s->sty.size = (uint8_t)brush_size(brush, s->size_cls);
    sk_picked(s, brush_name(brush), now);
    ui_cursor_warm(brush, size_for(s, brush));
}

void sk_set_shade(sketch_t *s, int shade, uint32_t now)
{
    if (tb_item_of_shade(shade) < 0) return;
    s->sty.shade = (uint8_t)shade;
    sk_cfg_num(s, "colour", shade);
    sk_picked(s, shade_name(shade), now);
}

static void tb_place(sketch_t *s) /* the strip for the screen in the user's frame, and its box in the framebuffer */
{
    gfx_t g;
    box_t b;
    mkg(s, &g);
    tb_layout(&s->tb, s->orient, g.SW, g.SH);
    ui_fb_box(&g, s->tb.x0, s->tb.y0, s->tb.x1, s->tb.y1, &b);
    box_clip(&b, s->fb.w, s->fb.h);
    s->tb_px = b;
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
    sk_mp_close(s);
    if (s->tb_on) { /* the page under the strip comes back as it was */
        s->tb_on = s->tb_hold = 0;
        s->tb_hot = -1;
        fb_put(s, &s->tb_box); /* (the guide comes back with the page) */
        mark(s, &s->tb_box); /* fast ink only: the drawing page never flashes (skflash.c) */
        return;
    }
    sk_note_hide(s);
    hist_end(&s->hist);
    s->stroke = 0;
    tb_place(s);
    s->tb_on = 1;
    sk_tb_paint(s);
    mark(s, &s->tb_box);
}

static void tb_press(sketch_t *s, int item, uint32_t now)
{
    if (item < 0) return;
    if (item < TI_BLACK) sk_set_brush(s, item - TI_BRUSH, now);
    else if (item < TI_SMALL) sk_set_shade(s, tb_shade_of(item), now);
    else if (item < TI_MIRROR) sk_set_size(s, item - TI_SMALL, now);
    else sk_mp_open(s); /* the five modes in a row: one tap picks (skmirror.c) */
}

/* ---- input ---- */

/* A point of the open stroke: drawn on the page, then on the screen */
/* A point of the open stroke: queued for the frame (skframe.h); a full queue is stamped now */
static void stroke_add(sketch_t *s, int fx, int fy, int p, uint32_t now)
{
    sk_frame_t *f = &s->frame;
    if (f->n >= SK_QUEUE_MAX) sk_frame(s, now);
    f->q[f->n++] = (sk_qpt_t){ (int16_t)fx, (int16_t)fy, p, s->tilt };
    sk_lat_ink(s, now); /* the latency counts from the report, not from the frame that stamps it */
    s->px = s->lx, s->py = s->ly, s->lx = fx, s->ly = fy, s->lp = p;
    s->dirty = 1;
    s->save_at = now + SK_AUTOSAVE_MS;
}

static void hover_apply(sketch_t *s);

static void frame_ink(sketch_t *s)
{
    sk_frame_t *f = &s->frame;
    if (!f->n) return;
    int brush = s->stk.brush == BR_BRUSH, step = brush ? brush_stamp_step(BR_BRUSH, s->stk.size) : 1;
    int scale = brush && s->stk.scale > 1 ? s->stk.scale : 1, keep[SK_QUEUE_MAX];
    int k = sk_thin(f->q, f->n, brush ? step * scale / 2 : 1, keep);
    /* the stroke keeps its step: undo and redo replay it exactly; sk_frame_cost sets the next stroke's */
    box_t dirty;
    box_clear(&dirty);
    for (int i = 0; i < k; i++) {
        const sk_qpt_t *q = &f->q[keep[i]];
        hist_add_t(&s->hist, q->x, q->y, q->p, q->tilt, &dirty);
    }
    f->n = 0;
    if (!box_empty(&dirty)) {
        fb_put(s, &dirty);
        mark(s, &dirty);
    }
}

void sk_frame(sketch_t *s, uint32_t now)
{
    sk_settle(s, now); /* the latency was noted when the point was queued */
    frame_ink(s);
    if (s->hv_pend) hover_apply(s);
    sk_gal_step(s, 0);
}

void sk_frame_cost(sketch_t *s, uint32_t ms)
{
    s->frame.cost_ms = ms;
    s->frame.scale = sk_scale_next(s->frame.scale, ms, &s->frame.cheap);
    hist_bake_step(&s->hist, SK_BAKE_PTS); /* an old stroke's slice into the base, every frame (bounded work) */
}

/* The stabiliser trails the pen a little: at the end of a stroke the line goes on to where the pen really ended */
static void stroke_tail(sketch_t *s, uint32_t now)
{
    if (s->stab.level && (s->lx != s->rx || s->ly != s->ry)) stroke_add(s, s->rx, s->ry, s->lp, now);
    /* The brush pen's end tapers out along the last movement */
    if (!brush_tail(s->stk.brush) || (s->lx == s->px && s->ly == s->py)) return;
    float dx = (float)(s->lx - s->px), dy = (float)(s->ly - s->py), len = fm_sqrt(dx * dx + dy * dy);
    int p = s->lp, x = s->lx, y = s->ly;
    for (int k = 1; k <= 3; k++)
        stroke_add(s, x + fm_round(dx / len * 4.0f * (float)k), y + fm_round(dy / len * 4.0f * (float)k), p * (3 - k) / 3, now);
}

void sk_pen(sketch_t *s, unsigned flags, unsigned x, unsigned y, unsigned pressure, uint32_t now)
{
    s->barrel = (flags & (APP_PEN_BARREL | APP_PEN_BARREL2)) != 0;
    s->tip = (flags & APP_PEN_TIP) != 0;
    s->last_act = now;
    s->hv_pend = 0;
    sk_settle(s, now);
    int fx, fy, inside = pm_map(&s->map, x, y, &fx, &fy);
    if (s->mode != SK_DRAW) return sk_menu_pen(s, flags, fx, fy, inside, now);
    int tip = (flags & APP_PEN_TIP) != 0, range = (flags & (APP_PEN_IN_RANGE | APP_PEN_TIP)) != 0;
    int on_bar = 0, ux, uy;
    gal_from_fb(s->orient, s->fb.w, s->fb.h, fx, fy, &ux, &uy);
    if (s->tb_on && inside) on_bar = tb_in(&s->tb, ux, uy);
    if ((s->mp_on || s->mp_hold) && sk_mp_pen(s, tip, range, inside, ux, uy, now)) return;
    if (!tip) s->tb_hold = 0;
    if (tip && on_bar) { /* a press on the strip picks, never inks; a stroke dragged onto it just ends */
        int was = s->stroke || s->bk_down;
        s->bk_down = 0;
            if (was && s->stroke) stroke_tail(s, now), sk_frame(s, now), hist_end(&s->hist), s->stroke = 0;
        else if (was) sk_frame(s, now), hist_end(&s->hist), s->stroke = 0;
        if (!s->tb_hold) {
            s->tb_hold = 1;
            sk_cursor_hide(s);
            if (!was) tb_press(s, tb_hit(&s->tb, ux, uy), now);
        }
        return;
    }
    if (tip && s->tb_hold) return; /* dragged off the strip: no ink until the pen lifts */
    if (sk_fill_pen(s, tip, inside, fx, fy, now)) return;
    if (tip && inside) {
        if (!s->stroke) {
            int br = eff_brush(s);
            sk_cursor_hide(s);
            sk_note_hide(s);
            s->stk = s->sty;
            s->stk.brush = (uint8_t)br;
            s->stk.size = (uint8_t)size_for(s, br);
            s->stk.scale = 1;
            if (br == BR_BRUSH) { /* the frame's step for this stroke, never over the radius (the stamps overlap) */
                int step = brush_stamp_step(BR_BRUSH, s->stk.size), smax = (s->stk.size / 2) / step, sc = s->frame.scale;
                if (smax < 1) smax = 1;
                s->stk.scale = (uint8_t)(sc < 1 ? 1 : sc > smax ? smax : sc);
            }
            s->stroke_erase = brush_is_eraser(br);
            hist_begin_style(&s->hist, s->stroke_erase ? HK_ERASE : HK_INK, &s->stk);
            s->stroke = 1;
            s->lx = s->px = s->rx = fx, s->ly = s->py = s->ry = fy;
            stab_begin(&s->stab, s->smooth, fx, fy);
            s->stroke_pts = 0;
            s->pressure = brush_tail(br) ? pressure / 4 : pressure; /* the brush pen swells from nothing */
        } else {
            s->pressure = (s->pressure + pressure) / 2;
        }
        int sx = fx, sy = fy;
        if (s->stroke_pts++) stab_push(&s->stab, fx, fy, &sx, &sy); /* the first point is exact */
        s->rx = fx, s->ry = fy;
        stroke_add(s, sx, sy, brush_param(s->stk.brush, s->stk.size, s->pressure), now);
        return;
    }
    if (s->stroke) {
        stroke_tail(s, now);
        sk_frame(s, now); /* the stroke's last points, before its history closes */
        hist_end(&s->hist);
        s->stroke = 0;
        s->traces++;
        s->save_at = now + SK_AUTOSAVE_MS;
    }
    /* A hover report only notes where the pen is; sk_frame draws the cursor (or the arrow) once, at the last one */
    s->hv_pend = 1;
    s->hv_ux = ux, s->hv_uy = uy, s->hover_x = fx, s->hover_y = fy;
    if (s->tb_on && range && !tip && inside && on_bar) s->hv_kind = HV_ARROW;
    else s->hv_kind = range && !tip && inside && !on_bar ? HV_CURSOR : HV_NONE;
}

/* The hover of the last report of the frame: the arrow over the strip, the brush's cursor on the page, or nothing */
static void hover_apply(sketch_t *s)
{
    s->hv_pend = 0;
    if (s->hv_kind == HV_ARROW) {
        s->hover_valid = 0;
        tb_pointer(s, s->hv_ux, s->hv_uy);
        return;
    }
    tb_pointer_off(s);
    s->hover_valid = s->hv_kind == HV_CURSOR;
    if (s->hover_valid) cursor_show(s, s->hover_x, s->hover_y);
    else sk_cursor_hide(s);
}

void sk_pen_key(sketch_t *s, unsigned keys, unsigned changed, uint32_t now)
{
    unsigned down = keys & changed;
    s->last_act = now;
    if ((down & ~3u) || (changed >> 7 & 1)) sk_settle(s, now); /* only undo and redo may leave the page stale */
    if (s->mode != SK_DRAW) {
        if (down) sk_menu_key(s, down, now);
        return;
    }
    if (down) sk_mp_close(s); /* any tablet button closes the mirror picker unchanged */
    if (changed >> 7 & 1) { /* button 8: the menu when let go, the debug toast after a long press */
        if (down >> 7 & 1) s->k8_down = now | 1;
        else if (s->k8_down) {
            int held = (int32_t)(now - s->k8_down) >= SK_LONGPRESS_MS;
            s->k8_down = 0;
            if (held) sk_lat_debug(s, now);
            else sk_to_pagemenu(s, now);
        }
    }
    for (int b = 0; b < 7; b++) {
        if (!(down >> b & 1)) continue;
        switch (b) {
        case 0: sk_undo(s, now); break;
        case 1: sk_redo(s, now); break;
        case 2: toggle_eraser(s, now); break;
        case 3: sk_toolbar(s, now); break;
        case 4: sk_clear(s, now); break;
        case 5: sk_new_page(s, now); break;
        default: sk_to_pagemenu(s, now); break; /* 7: the menu of this page (8 does the same when let go; no page stepping: the gallery is the way between pages) */
        }
    }
}

void sk_touch(sketch_t *s, int zone, int gesture, uint32_t now)
{
    s->last_act = now;
    sk_settle(s, now);
    if (s->mode == SK_SHARE) { /* any touch ends the phone page */
        s->share_req = -1;
        if (s->share_kind == 2) sk_cfg_reload(s); /* the settings form was open */
        s->mode = s->ret_mode;
        sk_screen_render(s);
        return;
    }
    if (s->mode >= SK_GALLERY) return sk_menu_touch(s, zone, gesture, now);
    if (gesture == APP_GESTURE_TAP && zone == APP_TOUCH_MIDDLE) sk_to_pagemenu(s, now); /* the one extra way in; nothing else on the bar */
}

void sk_share_update(sketch_t *s, int start_rc, const app_webshare_status_t *st)
{
    if (s->mode != SK_SHARE) return;
    share_view_t v;
    share_view(&v, start_rc, st);
    v.settings = s->share_kind == 2;
    if (v.settings && v.up) sk_cpy(v.note, sizeof(v.note), "Save the form on your phone, then tap Back.");
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
    sk_settle(s, now);
    if (s->bk_plan.mask && (int32_t)(now - s->bk_ask_until) >= 0) sk_fill_forget(s); /* the question ran out */
    if (s->note_on && !s->legend && (int32_t)(now - s->note_until) >= 0) {
        sk_note_hide(s);
        if (s->hover_valid) cursor_show(s, s->hover_x, s->hover_y);
    }
    if (s->legend && (int32_t)(now - s->note_until) >= 0) sk_note_hide(s);
    if (s->dirty && !s->stroke && (int32_t)(now - s->save_at) >= 0) sk_do_save(s, now);
    if (!s->stroke) hist_bake_step(&s->hist, SK_BAKE_IDLE_PTS); /* the pen is up: old strokes into the base, a slice a tick */
    sk_lat_tick(s, now);
}

int sk_flush(sketch_t *s, uint32_t now, rect_t *out)
{
    sk_frame(s, now); /* the frame's points before the push (main.c frames first; a direct caller gets the same) */
    int n = sk_flash_step(s, now, out);
    if (n != SK_NOFLASH) return n;
    if (!s->pend.n || (uint32_t)(now - s->last_push) < (uint32_t)s->batch_ms) return 0;
    n = rs_take(&s->pend, out);
    sk_lat_issued(s, now);
    for (int i = 0; i < n; i++) s->pushed_px += (uint32_t)(out[i].w * out[i].h);
    s->pushed_rects += (uint32_t)n;
    s->last_push = now;
    s->ghost = 1; /* a partial refresh left its traces: the idle clean-up has something to do */
    return n;
}

uint32_t sk_next_due(const sketch_t *s, uint32_t now)
{
    uint32_t due = 1000000;
    if (s->want_full || !box_empty(&s->flash) || s->fl_phase || (s->gal_todo && s->mode == SK_GALLERY) || s->print_req) return 0;
    uint32_t cd = sk_cleanup_due(s, now);
    if (cd < due) due = cd;
    if (s->pend.n) {
        int32_t d = (int32_t)(s->last_push + (uint32_t)s->batch_ms - now);
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

uint32_t sk_wait_ms(sketch_t *s, uint32_t now)
{
    uint32_t w = sk_next_due(s, now);
    if (w) return s->wz_zero = 0, w;
    if (s->wz_zero && s->wz_push == s->last_push) return SK_MIN_SLEEP_MS; /* due, but nothing was pushed since: do not spin */
    s->wz_zero = 1, s->wz_push = s->last_push;
    return 0;
}

/* ---- set up ---- */

void sk_set_tablet(sketch_t *s, uint32_t width_um, uint32_t height_um, int connected)
{
    s->um_w = width_um, s->um_h = height_um, s->tablet = connected;
    pm_init(&s->map, s->orient, s->fb.w, s->fb.h, width_um, height_um, s->fill, s->turn);
}

void sk_cfg_reload(sketch_t *s)
{
    char b[8];
    int n = s->env.cfg_get ? s->env.cfg_get(s->env.user, "pen_area", b, 7) : -1;
    s->fill = !(n == 4 && !memcmp(b, "keep", 4)); /* fill unless it says keep */
    s->turn = !(s->env.cfg_get && (n = s->env.cfg_get(s->env.user, "tablet_turn", b, 7)) == 3 && !memcmp(b, "off", 3)); /* auto unless off */
    s->bk_gap = cfg_int(s, "fill_gap", 3);
    if (s->bk_gap > 6) s->bk_gap = 6;
    n = s->env.cfg_get ? s->env.cfg_get(s->env.user, "paper", b, 7) : -1;
    s->paper_def = n > 0 ? pp_from_name(b, n) : PP_BLANK;
    s->paper_sp_def = pp_spacing(cfg_int(s, "paper_sp", 0));
    s->cleanup_s = cfg_int(s, "cleanup_s", 5);
    if (s->cleanup_s > 600) s->cleanup_s = 600;
    pm_init(&s->map, s->orient, s->fb.w, s->fb.h, s->um_w, s->um_h, s->fill, s->turn);
}

void sk_set_orient(sketch_t *s, int orient, const app_touchbar_t *bar)
{
    s->orient = orient;
    s->bar = *bar;
    pm_init(&s->map, orient, s->fb.w, s->fb.h, s->um_w, s->um_h, s->fill, s->turn);
    s->hover_valid = 0;
    s->mp_on = s->mp_hold = 0; /* the picker is placed again when it is opened (the page is drawn whole below) */
    if (s->mode == SK_DRAW) {
        sk_cursor_hide(s);
        sk_note_hide(s);
        if (s->tb_on) tb_place(s);
        sk_show_page(s); /* nothing but the page (and the toolbar, if it is up) is left on the screen */
    } else if (s->mode >= SK_GALLERY) {
        if (s->tb_back) tb_place(s);
        sk_screen_render(s);
    } else {
        memset(&s->share_shown, 0xff, sizeof(s->share_shown));
    }
    s->want_full = 1;
}

/* The saved colour. Settings may come from the earlier toolbar: its tone slider ("tone": 0 black, 1..8 lighter, 9 white) and its three shades
   ("shade") become Black, Grey or White; an unknown value is Black. Never an error */
static int colour_cfg(sketch_t *s)
{
    int c = cfg_int(s, "colour", -1), t = cfg_int(s, "tone", -1);
    if (c >= 0) return tb_item_of_shade(c) >= 0 ? c : SH_BLACK;
    if (t < 0) t = cfg_int(s, "shade", 0) ? SH_GREY : SH_BLACK;
    return t == SH_BLACK ? SH_BLACK : t == SH_WHITE ? SH_WHITE : SH_GREY;
}

/* Undo by deltas (hist.h): a shadow page and the pool, halved down to SK_HIST_DELTA_MIN when memory is short; none at
   all: every undo draws the page again (slow, never wrong) */
static void deltas_alloc(hist_t *h, size_t bytes)
{
    uint8_t *shadow = malloc(bytes), *pool = 0;
    uint32_t cap = SK_HIST_DELTA_BYTES;
    while (shadow && !(pool = malloc(cap)) && cap / 2 >= SK_HIST_DELTA_MIN) cap /= 2;
    if (!pool) return free(shadow);
    hist_set_deltas(h, shadow, pool, cap);
}

int sk_init(sketch_t *s, const sk_env_t *env, uint8_t *fb, int w, int h, int stride, int orient, const app_touchbar_t *bar)
{
    memset(s, 0, sizeof(*s));
    s->tb_hot = -1, s->batch_ms = SK_BATCH_MS, s->smooth = SK_SMOOTH_DEF;
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
    deltas_alloc(&s->hist, bytes);
    s->hist.lazy = 1; /* a row of presses without deltas: one rebuild in the frame (sk_settle) */
    pg_init(&s->pg, env->fs);
    pg_scan(&s->pg);
    pg_purge_blank(&s->pg, &s->page, 0); /* blank pages left by older builds or by a clear: gone (files that do not decode stay) */
    cv_fill(&s->page, 1);
    s->size_cls = cfg_int(s, "size_cls", 1);
    if (s->size_cls >= BR_SIZES) s->size_cls = 1;
    s->sty.shade = (uint8_t)colour_cfg(s);
    s->sty.brush = BR_BRUSH; /* owner 2026-11-02: Sketch always opens with the best tool, the Brush */
    s->sty.size = (uint8_t)brush_size(s->sty.brush, s->size_cls);
    s->last_ink = s->sty.brush;
    box_clear(&s->flash);
    s->orient = orient;
    s->bar = *bar;
    sk_cfg_reload(s);
    int want = cfg_int(s, "cur", 0);
    int num = pg_index(&s->pg, want) >= 0 ? want : s->pg.n ? s->pg.nums[s->pg.n - 1] : 1, rc = PG_MISSING;
    if (s->pg.n && (rc = pg_load(&s->pg, num, &s->page)) == PG_BAD) { /* a damaged page never opens for drawing: the newest one that decodes, else a new page */
        int i = s->pg.n - 1;
        while (i >= 0 && (rc = pg_load(&s->pg, s->pg.nums[i], &s->page)) != PG_OK) i--;
        num = i >= 0 ? s->pg.nums[i] : pg_next_num(&s->pg);
        if (i < 0) num = num ? num : 1;
    }
    s->cur = num;
    if (s->pg.n && pg_index(&s->pg, num) >= 0) sk_opened(s, num, rc, 0); /* decoded once (it was twice); none: the blank one is written with its first ink */
    else cv_fill(&s->page, 1), sk_paper_new(s);
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
    sk_fill_forget(s);
    pj_job_free(s->pjob); /* the app left while printing: the job is given up, nothing sent */
    s->pjob = 0;
    sk_tb_img_free(s);
    free(s->ui.px);
    gal_cache_free(&s->cache);
    free(s->page.px);
    free(s->hist.base.px);
    free(s->hist.pts);
    free(s->hist.st);
    free(s->hist.shadow.px);
    free(s->hist.pool);
    s->page.px = 0;
}
