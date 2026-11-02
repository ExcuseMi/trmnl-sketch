/* The shape tools (Line, Box, Oval): a drag from the start to the end with a rubber-band preview on the screen only;
   on lift the shape is one stroke of the log (so one undo step, mirrored, replayed exactly) drawn with the brush, tone and
   size picked. Holding the pen's barrel button or Shift constrains: lines to 0, 45 and 90 degrees, boxes to squares,
   ovals to circles. SPDX-License-Identifier: MIT */
#include "sketch.h"
#include "fmath.h"

#define SH_MIN_PX 3      /* a drag shorter than this (each axis) makes no shape */
#define SH_PRESSURE 40000 /* what a shape's points carry for the brush (pressure brushes: a firm touch) */

static int sgn(int v) { return v < 0 ? -1 : 1; }
static int iabs(int v) { return v < 0 ? -v : v; }

void sk_shape_end(int tool, int x0, int y0, int *x1, int *y1, int constrain)
{
    if (!constrain) return;
    int dx = *x1 - x0, dy = *y1 - y0, ax = iabs(dx), ay = iabs(dy);
    if (tool == TL_LINE) {
        if (ay * 12 < ax * 5) *y1 = y0;                      /* under 22.5 degrees: flat */
        else if (ax * 12 < ay * 5) *x1 = x0;                 /* over 67.5: upright */
        else {
            int m = (ax + ay) / 2;                           /* 45 degrees */
            *x1 = x0 + sgn(dx) * m, *y1 = y0 + sgn(dy) * m;
        }
    } else {
        int m = ax > ay ? ax : ay;                           /* square or circle */
        *x1 = x0 + sgn(dx) * m, *y1 = y0 + sgn(dy) * m;
    }
}

int sk_shape_pts(int tool, int x0, int y0, int x1, int y1, int16_t (*pt)[2], int cap)
{
    int n = 0;
#define ADD(X, Y) do { if (n < cap) pt[n][0] = (int16_t)(X), pt[n][1] = (int16_t)(Y); n++; } while (0)
    if (tool == TL_LINE) {
        ADD(x0, y0);
        ADD(x1, y1);
    } else if (tool == TL_RECT) {
        ADD(x0, y0);
        ADD(x1, y0);
        ADD(x1, y1);
        ADD(x0, y1);
        ADD(x0, y0);
    } else {
        float cx = (float)(x0 + x1) / 2, cy = (float)(y0 + y1) / 2, rx = (float)iabs(x1 - x0) / 2, ry = (float)iabs(y1 - y0) / 2;
        int seg = 24 + (int)(rx + ry) / 5;
        if (seg > 120) seg = 120;
        for (int i = 0; i <= seg; i++) {
            float a = 2 * FM_PI * (float)i / (float)seg;
            if (i == seg) ADD(cx + rx, cy); /* closes exactly */
            else ADD(fm_round(cx + rx * fm_cos(a)), fm_round(cy + ry * fm_sin(a)));
        }
    }
#undef ADD
    return n < cap ? n : cap;
}

static int clampi(int v, int hi) { return v < 0 ? 0 : v > hi ? hi : v; }

/* The preview goes away: the page comes back under it */
static void preview_clear(sketch_t *s)
{
    if (box_empty(&s->sh_box)) return;
    sk_fb_restore(s, &s->sh_box);
    box_clear(&s->sh_box);
}

static void preview(sketch_t *s)
{
    int16_t pt[SK_SHAPE_PTS][2];
    int x1 = s->sh_x1, y1 = s->sh_y1;
    sk_shape_end(s->sty.tool, s->sh_x0, s->sh_y0, &x1, &y1, s->barrel || (s->mods & APP_MOD_SHIFT));
    int n = sk_shape_pts(s->sty.tool, s->sh_x0, s->sh_y0, x1, y1, pt, SK_SHAPE_PTS);
    box_t nb;
    box_clear(&nb);
    bstyle_t pv = { BR_FINELINER, SH_BLACK, 3, s->sty.mirror, 0 }; /* a thin hairline: cheap to redraw at pen speed */
    if (!box_empty(&s->sh_box)) sk_fb_restore(s, &s->sh_box);
    for (int i = 0; i < n; i++) brush_seg(&pv, 0, &s->fb, pt[i ? i - 1 : 0][0], pt[i ? i - 1 : 0][1], 3, pt[i][0], pt[i][1], 3, &nb);
    box_clip(&nb, s->fb.w, s->fb.h);
    s->sh_box = nb;
    sk_mark(s, &nb);
}

void sk_shape_cancel(sketch_t *s)
{
    if (!s->sh_on) return;
    s->sh_on = 0;
    preview_clear(s);
}

static void commit(sketch_t *s, uint32_t now)
{
    int x1 = s->sh_x1, y1 = s->sh_y1, br = s->sty.brush == BR_FILL ? s->last_ink : s->sty.brush;
    sk_shape_end(s->sty.tool, s->sh_x0, s->sh_y0, &x1, &y1, s->barrel || (s->mods & APP_MOD_SHIFT));
    preview_clear(s);
    if (iabs(x1 - s->sh_x0) < SH_MIN_PX && iabs(y1 - s->sh_y0) < SH_MIN_PX) return; /* a tap: no shape */
    int16_t pt[SK_SHAPE_PTS][2];
    int n = sk_shape_pts(s->sty.tool, s->sh_x0, s->sh_y0, x1, y1, pt, SK_SHAPE_PTS);
    bstyle_t st = s->sty;
    st.brush = (uint8_t)br, st.tool = TL_DRAW;
    if (br != s->sty.brush) st.size = s->ink_size;
    int d = brush_param(br, st.size, SH_PRESSURE);
    box_t dirty;
    box_clear(&dirty);
    hist_begin_style(&s->hist, brush_is_eraser(br) ? HK_ERASE : HK_INK, &st);
    for (int i = 0; i < n; i++) hist_add(&s->hist, pt[i][0], pt[i][1], d, &dirty);
    hist_end(&s->hist);
    sk_recent_add(s, &st);
    if (!box_empty(&dirty)) sk_page_changed(s, &dirty, now);
}

int sk_shape_pen(sketch_t *s, int tip, int inside, int fx, int fy, uint32_t now)
{
    if (s->sty.tool < TL_LINE || s->sty.tool > TL_OVAL) return 0;
    fx = clampi(fx, s->fb.w - 1), fy = clampi(fy, s->fb.h - 1);
    if (tip && !s->sh_on) {
        if (!inside || s->stroke) return 0;
        sk_cursor_hide(s);
        sk_note_hide(s);
        s->sh_on = 1, s->sh_x0 = s->sh_x1 = fx, s->sh_y0 = s->sh_y1 = fy;
        box_clear(&s->sh_box);
        return 1;
    }
    if (tip) {
        if (fx != s->sh_x1 || fy != s->sh_y1 || s->barrel != s->sh_barrel) s->sh_x1 = fx, s->sh_y1 = fy, s->sh_barrel = s->barrel, preview(s);
        return 1;
    }
    if (!s->sh_on) return 0;
    s->sh_on = 0;
    commit(s, now);
    return 0;
}

void sk_set_tool(sketch_t *s, int tool, uint32_t now)
{
    static const char *const names[] = { "", "Line", "Box", "Oval", "Text" };
    if (tool < TL_DRAW || tool > TL_TEXT) return;
    sk_shape_cancel(s);
    sk_text_end(s, now);
    s->sty.tool = (uint8_t)tool;
    if (tool == TL_TEXT && !s->kbd) s->kbd_req = 1; /* the glue enables the keyboards */
    sk_picked(s, tool == TL_TEXT ? (s->kbd ? "Text: tap where it goes" : "Connect a keyboard") : tool ? names[tool] : brush_name(s->sty.brush), now);
}
