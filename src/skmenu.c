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

static void tri(gfx_t *g, float cx, float cy, float s, int dir, int col)
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
    gfx_poly(g, p, 3, col);
}

static int pill_w(const char *label, int icon)
{
    const font_face_t *f = mface(0, 34);
    return mtext_w(f, label) + (icon ? 24 + 16 : 0) + 2 * PILL_PAD_X;
}

static void pill_draw(gfx_t *g, int x, int y, const char *label, int icon, int hot)
{
    const font_face_t *f = mface(0, 34);
    int pw = pill_w(label, icon), fg = hot ? MWHT : MBLK, left = x + PILL_PAD_X;
    gfx_rrect(g, x, y, x + pw - 1, y + PILL_H - 1, PILL_H / 2, 3, hot ? MBLK : MWHT, MBLK);
    if (icon) tri(g, (float)left + 12.0f, (float)y + (float)PILL_H / 2, 24.0f, icon, fg), left += 24 + 16;
    mtext(g, f, left, y + (PILL_H + f->digit_h) / 2, label, fg);
}

void ui_pill_box(int sw, int sh, const app_touchbar_t *bar, int zone, const char *label, int icon, int *x0, int *y0, int *x1, int *y1)
{
    int pw = pill_w(label, icon), x, y;
    const app_rect_t *r = &bar->zone[zone];
    switch (bar->edge) {
    case APP_EDGE_LEFT: x = PILL_EDGE, y = r->y + r->h / 2 - PILL_H / 2; break;
    case APP_EDGE_RIGHT: x = sw - PILL_EDGE - pw, y = r->y + r->h / 2 - PILL_H / 2; break;
    case APP_EDGE_TOP: x = r->x + r->w / 2 - pw / 2, y = PILL_EDGE; break;
    default: x = r->x + r->w / 2 - pw / 2, y = sh - PILL_EDGE - PILL_H; break;
    }
    *x0 = x, *y0 = y, *x1 = x + pw - 1, *y1 = y + PILL_H - 1;
}

void ui_pill(gfx_t *g, const app_touchbar_t *bar, int zone, const char *label, int icon, int hot)
{
    if (!label) return;
    int x0, y0, x1, y1;
    ui_pill_box(g->SW, g->SH, bar, zone, label, icon, &x0, &y0, &x1, &y1);
    pill_draw(g, x0, y0, label, icon, hot);
}

void ui_menu_pills(gfx_t *g, const app_touchbar_t *bar, const char *select, int hot)
{
    ui_pill(g, bar, APP_TOUCH_LEFT, "Up", PILL_UP, hot == CTL_UP);
    ui_pill(g, bar, APP_TOUCH_MIDDLE, select, PILL_NONE, hot == CTL_SEL);
    ui_pill(g, bar, APP_TOUCH_RIGHT, "Down", PILL_DOWN, hot == CTL_DOWN);
}

/* The paging pair in the title row: [Previous] page indicator [Next], right aligned */
#define NAV_SLOT 170
#define NAV_GAP 24
void ui_nav_box(const gal_layout_t *L, int which, int *x0, int *y0, int *x1, int *y1)
{
    int wn = pill_w("Next", PILL_DOWN), wp = pill_w("Previous", PILL_UP), nx = L->ax1 - pill_w("Back", PILL_NONE) - NAV_GAP - wn, px = nx - NAV_GAP - NAV_SLOT - NAV_GAP - wp;
    *y0 = L->ay0 + 26, *y1 = *y0 + PILL_H - 1;
    if (which == CTL_NEXT) *x0 = nx, *x1 = nx + wn - 1;
    else *x0 = px, *x1 = px + wp - 1;
}

void ui_ctl_box(const gal_layout_t *L, int which, int *x0, int *y0, int *x1, int *y1)
{
    if (which == CTL_BACK) {
        int w = pill_w("Back", PILL_NONE);
        *x0 = L->ax1 - w, *x1 = L->ax1 - 1, *y0 = L->ay0 + 26, *y1 = *y0 + PILL_H - 1;
    } else {
        int w = pill_w("Set up printer", PILL_NONE);
        *x0 = L->ax0 + 40, *x1 = *x0 + w - 1, *y0 = (L->ay0 + L->ay1) / 2 + 150, *y1 = *y0 + PILL_H - 1;
    }
}

void ui_back_corner(gfx_t *g)
{
    pill_draw(g, g->SW - 60 - pill_w("Back", PILL_NONE), 40, "Back", PILL_NONE, 0);
}

void ui_button(gfx_t *g, const gal_layout_t *L, int which, int hot)
{
    int x0, y0, x1, y1;
    ui_ctl_box(L, which, &x0, &y0, &x1, &y1);
    pill_draw(g, x0, y0, which == CTL_BACK ? "Back" : "Set up printer", PILL_NONE, hot);
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

void ui_settings(gfx_t *g, const gal_layout_t *L, const char *title, const char *const *names, const char *const *vals, int n, int sel, list_geom_t *geo)
{
    const font_face_t *big = mface(1, 64);
    mtext(g, big, L->ax0, L->ay0 + 110, title, MBLK);
    int x = L->ax0 + 60, mid = (L->ay0 + L->ay1) / 2, vw = mtext_w(big, "Lines") + 140, vx = L->ax1 - 70 - vw;
    geo->x = x, geo->y0 = mid - (n - 1) * LIST_PITCH / 2, geo->x1 = L->ax1 - 40;
    for (int i = 0; i < n; i++) {
        int y = geo->y0 + i * LIST_PITCH, c = i == sel ? MWHT : MBLK;
        if (i == sel) gfx_rrect(g, x - 30, y - 74, geo->x1, y + 28, 14, 0, MBLK, MBLK);
        mtext(g, big, x, y, names[i], c);
        if (!vals[i]) continue;
        mtext(g, big, vx, y, "<", c);
        mtext(g, big, vx + vw - mtext_w(big, ">"), y, ">", c);
        mtext(g, big, vx + vw / 2 - mtext_w(big, vals[i]) / 2, y, vals[i], c);
    }
}

void ui_list_row(const list_geom_t *geo, int i, int *x0, int *y0, int *x1, int *y1)
{
    *x0 = geo->x - 30, *x1 = geo->x1, *y0 = geo->y0 + i * LIST_PITCH - 74, *y1 = geo->y0 + i * LIST_PITCH + 28;
}

void ui_gallery(gfx_t *g, const gal_layout_t *L, const int *nums, const uint8_t *dmg, int count, int sel, int cur, int page, int pages, const char *note, int hot)
{
    const font_face_t *big = mface(1, 64), *cap = mface(0, 34), *capb = mface(1, 34);
    char t[40] = "Sketches";
    mtext(g, big, L->ax0, L->ay0 + 110, t, MBLK);
    mtext(g, cap, L->ax0, L->ay0 + 148, "Buttons 1/2 move, 7/8 screens, 4 open, 6 new page, 5 close", MBLK);
    if (pages > 1) {
        char pg[24];
        int n = 0, v[2] = { page + 1, pages }, a0, b0, a1, b1;
        for (int k = 0; k < 2; k++) {
            char d[8];
            int m = 0, x = v[k];
            do d[m++] = (char)('0' + x % 10), x /= 10; while (x);
            while (m) pg[n++] = d[--m];
            if (!k) pg[n++] = ' ', pg[n++] = '/', pg[n++] = ' ';
        }
        pg[n] = 0;
        ui_nav_box(L, CTL_PREV, &a0, &b0, &a1, &b1);
        int nx0, ny0, nx1, ny1;
        ui_nav_box(L, CTL_NEXT, &nx0, &ny0, &nx1, &ny1);
        mtext(g, cap, (a1 + nx0) / 2 - mtext_w(cap, pg) / 2, b0 + (PILL_H + cap->digit_h) / 2, pg, MBLK);
        if (page > 0) pill_draw(g, a0, b0, "Previous", PILL_UP, hot == CTL_PREV);
        if (page + 1 < pages) pill_draw(g, nx0, ny0, "Next", PILL_DOWN, hot == CTL_NEXT);
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
        if (dmg && dmg[i]) {
            const font_face_t *df = mface(1, 46);
            mtext(g, df, x + (L->tw - mtext_w(df, "Damaged")) / 2, y + L->th / 2 + 14, "Damaged", MBLK);
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
