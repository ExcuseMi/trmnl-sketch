/* SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 */
#include "skpush.h"
#include "sketch.h"

int skp_calls(int n, const rect_t *r, int mode, int ink, sk_call_t *out)
{
    if (n == SK_FULL) {
        out[0] = (sk_call_t){ { 0, 0, 0, 0 }, APP_REFRESH_FULL, 1 };
        return 1;
    }
    int k = 0, band = -1;
    for (int i = 0; i < n && i < SK_MAX_RECTS; i++) {
        app_rect_t a = { r[i].x, r[i].y, r[i].w, r[i].h };
        if (r[i].flash || mode == SKP_PER_RECT || band < 0) { /* a clean area, or the first partial one */
            if (!r[i].flash && mode != SKP_PER_RECT) band = k;
            out[k++] = (sk_call_t){ a, r[i].flash ? APP_REFRESH_FULL : ink ? APP_REFRESH_INK : APP_REFRESH_PARTIAL, 0 };
            continue;
        }
        app_rect_t *b = &out[band].r; /* the partial band grows to span this one too */
        int x0 = b->x < a.x ? b->x : a.x, y0 = b->y < a.y ? b->y : a.y;
        int x1 = b->x + b->w > a.x + a.w ? b->x + b->w : a.x + a.w, y1 = b->y + b->h > a.y + a.h ? b->y + b->h : a.y + a.h;
        *b = (app_rect_t){ (int16_t)x0, (int16_t)y0, (int16_t)(x1 - x0), (int16_t)(y1 - y0) };
    }
    return k;
}
