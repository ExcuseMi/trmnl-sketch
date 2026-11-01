/* SPDX-License-Identifier: MIT */
#include "skmenu.h"
#include <string.h>
#include "font.h"

enum { MBLK = GFX_BLACK, MWHT = GFX_WHITE };

#define PILL_PAD_X 40
#define PILL_H 84
#define PILL_EDGE 40 /* from the screen's edge, as hints.h HINTS_MARGIN */

static const font_face_t *mface(int bold, int size) { return font_get(bold ? FONT_BOLD : FONT_MEDIUM, size); }
static int mtext_w(const font_face_t *f, const char *s) { return (int)((font_width64(f, s, 0) + 32) >> 6); }
static void mtext(gfx_t *g, const font_face_t *f, int x, int y, const char *s, int c) { gfx_text(g, f, (int32_t)x * 64, y, s, 0, c); }

static void tri(gfx_t *g, float cx, float cy, float s, int dir)
{
    float p[6];
    if (dir == PILL_UP) {
        float t[6] = { cx, cy - s / 2, cx - s / 2, cy + s / 2, cx + s / 2, cy + s / 2 };
        memcpy(p, t, sizeof(p));
    } else if (dir == PILL_DOWN) {
        float t[6] = { cx - s / 2, cy - s / 2, cx + s / 2, cy - s / 2, cx, cy + s / 2 };
        memcpy(p, t, sizeof(p));
    } else {
        float t[6] = { cx - s / 4, cy - s / 2, cx - s / 4, cy + s / 2, cx + s / 2 - s / 4, cy };
        memcpy(p, t, sizeof(p));
    }
    gfx_poly(g, p, 3, MBLK);
}

void ui_pill(gfx_t *g, const app_touchbar_t *bar, int zone, const char *label, int icon)
{
    if (!label) return;
    const font_face_t *f = mface(0, 34);
    int tw = mtext_w(f, label), ti = icon ? 24 : 0, content = tw + (ti ? ti + 16 : 0), pw = content + 2 * PILL_PAD_X;
    const app_rect_t *r = &bar->zone[zone];
    int x, y;
    switch (bar->edge) {
    case APP_EDGE_LEFT: x = PILL_EDGE, y = r->y + r->h / 2 - PILL_H / 2; break;
    case APP_EDGE_RIGHT: x = g->SW - PILL_EDGE - pw, y = r->y + r->h / 2 - PILL_H / 2; break;
    case APP_EDGE_TOP: x = r->x + r->w / 2 - pw / 2, y = PILL_EDGE; break;
    default: x = r->x + r->w / 2 - pw / 2, y = g->SH - PILL_EDGE - PILL_H; break;
    }
    gfx_rrect(g, x, y, x + pw - 1, y + PILL_H - 1, PILL_H / 2, 3, MWHT, MBLK);
    int left = x + PILL_PAD_X;
    if (ti) tri(g, (float)left + (float)ti / 2, (float)y + (float)PILL_H / 2, (float)ti, icon), left += ti + 16;
    mtext(g, f, left, y + (PILL_H + f->digit_h) / 2, label, MBLK);
}

void ui_menu_pills(gfx_t *g, const app_touchbar_t *bar, const char *select)
{
    ui_pill(g, bar, APP_TOUCH_LEFT, "Up", PILL_UP);
    ui_pill(g, bar, APP_TOUCH_MIDDLE, select, PILL_NONE);
    ui_pill(g, bar, APP_TOUCH_RIGHT, "Down", PILL_DOWN);
}

void ui_list(gfx_t *g, const gal_layout_t *L, const char *title, const char *const *items, int n, int sel, list_geom_t *geo)
{
    const font_face_t *big = mface(1, 64);
    mtext(g, big, L->ax0, L->ay0 + 110, title, MBLK);
    int x = L->ax0 + (L->ax1 - L->ax0) / 3, mid = (L->ay0 + L->ay1) / 2, w = 0;
    for (int i = 0; i < n; i++)
        if (mtext_w(big, items[i]) > w) w = mtext_w(big, items[i]);
    geo->x = x, geo->y0 = mid - (n - 1) * LIST_PITCH / 2, geo->x1 = x + w + 30;
    for (int i = 0; i < n; i++) {
        int y = geo->y0 + i * LIST_PITCH;
        if (i == sel) gfx_rrect(g, x - 30, y - 74, geo->x1, y + 28, 14, 0, MBLK, MBLK); /* the selected row is inverted */
        mtext(g, big, x, y, items[i], i == sel ? MWHT : MBLK);
    }
}

void ui_list_row(const list_geom_t *geo, int i, int *x0, int *y0, int *x1, int *y1)
{
    *x0 = geo->x - 30, *x1 = geo->x1, *y0 = geo->y0 + i * LIST_PITCH - 74, *y1 = geo->y0 + i * LIST_PITCH + 28;
}

void ui_gallery(gfx_t *g, const gal_layout_t *L, const int *nums, int count, int sel, int cur, int page, int pages, const char *note)
{
    const font_face_t *big = mface(1, 64), *cap = mface(0, 34), *capb = mface(1, 34);
    char t[40] = "Sketches";
    mtext(g, big, L->ax0, L->ay0 + 110, t, MBLK);
    if (pages > 1) {
        char pg[24];
        int n = 0, v[2] = { page + 1, pages };
        for (int k = 0; k < 2; k++) {
            char d[8];
            int m = 0, x = v[k];
            do d[m++] = (char)('0' + x % 10), x /= 10; while (x);
            while (m) pg[n++] = d[--m];
            if (!k) pg[n++] = '/';
        }
        pg[n] = 0;
        mtext(g, cap, L->ax1 - mtext_w(cap, pg), L->ay0 + 110, pg, MBLK);
    }
    for (int i = 0; i < count; i++) {
        int x, y;
        gal_tile_xy(L, i, &x, &y);
        int on = i == sel, w = on ? 8 : 2;
        gfx_rrect(g, x - 10, y - 10, x + L->tw + 9, y + L->th + 9, 12, w, MWHT, MBLK);
        if (nums[i] == 0) {
            int cx = x + L->tw / 2, cy = y + L->th / 2 - 30;
            gfx_fill_rect(g, (float)cx - 40, (float)cy - 6, (float)cx + 40, (float)cy + 6, MBLK);
            gfx_fill_rect(g, (float)cx - 6, (float)cy - 40, (float)cx + 6, (float)cy + 40, MBLK);
            mtext(g, capb, x + (L->tw - mtext_w(capb, "New page")) / 2, y + L->th / 2 + 70, "New page", MBLK);
            continue;
        }
        char num[16];
        int n = 0, m = 0, v = nums[i];
        char d[8];
        do d[m++] = (char)('0' + v % 10), v /= 10; while (v);
        while (m) num[n++] = d[--m];
        num[n] = 0;
        const font_face_t *f = on ? capb : cap;
        int ty = y + L->th + 44;
        mtext(g, f, x, ty, num, MBLK);
        if (nums[i] == cur) {
            int w2 = mtext_w(capb, "open") + 28;
            int px = x + L->tw - w2;
            gfx_rrect(g, px, ty - 34, x + L->tw - 1, ty + 10, 22, 0, MBLK, MBLK);
            mtext(g, capb, px + 14, ty, "open", MWHT);
        }
    }
    if (note && note[0]) mtext(g, cap, L->ax0, L->ay1 - 4, note, MBLK);
}

void ui_message(gfx_t *g, const gal_layout_t *L, const char *title, const char *l1, const char *l2, const char *l3)
{
    const font_face_t *big = mface(1, 64), *f = mface(0, 46);
    int y = (L->ay0 + L->ay1) / 2 - 90;
    mtext(g, big, L->ax0 + 40, y, title, MBLK);
    const char *ls[3] = { l1, l2, l3 };
    for (int i = 0; i < 3; i++)
        if (ls[i] && ls[i][0]) mtext(g, f, L->ax0 + 40, y + 90 + i * 64, ls[i], MBLK);
}

void ui_footer(gfx_t *g, const gal_layout_t *L, const char *line)
{
    mtext(g, mface(0, 34), L->ax0, L->ay1 - 4, line, MBLK);
}
