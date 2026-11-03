/* The frame's points, thinned; the stamp step's scale (skframe.h). SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 */
#include "skframe.h"

int sk_thin(const sk_qpt_t *in, int n, int thin_px, int *keep)
{
    if (n <= 0) return 0;
    int k = 0, lx = in[0].x, ly = in[0].y;
    keep[k++] = 0;
    for (int i = 1; i < n - 1; i++) {
        int dx = in[i].x - lx, dy = in[i].y - ly;
        if (dx < 0) dx = -dx;
        if (dy < 0) dy = -dy;
        if (thin_px < 1 || dx >= thin_px || dy >= thin_px) keep[k++] = i, lx = in[i].x, ly = in[i].y;
    }
    if (n > 1) keep[k++] = n - 1;
    return k;
}

int sk_scale_next(int scale, uint32_t cost_ms, int *cheap)
{
    if (scale < 1) scale = 1;
    if (cost_ms > SK_FRAME_BUDGET_MS) {
        *cheap = 0;
        return scale * 2 > SK_STEP_SCALE_MAX ? SK_STEP_SCALE_MAX : scale * 2;
    }
    if (cost_ms * 3 >= SK_FRAME_BUDGET_MS) return *cheap = 0, scale;
    if (++*cheap < SK_SCALE_EASE_FRAMES || scale <= 1) return scale;
    *cheap = 0;
    return scale - 1;
}
