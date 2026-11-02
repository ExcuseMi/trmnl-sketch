/* SPDX-License-Identifier: MIT */
#include "textdraw.h"
#include "font.h"

static const font_face_t *face(int cls) { return cls == 0 ? font_get(FONT_MEDIUM, 34) : font_get(FONT_BOLD, 64); }
static int scale(int cls) { return cls == 2 ? 2 : 1; }

int tx_class(int pen_size) { return pen_size <= 16 ? 0 : pen_size <= 32 ? 1 : 2; }
int tx_line_h(int cls)
{
    const font_face_t *f = face(cls);
    return (f->ascent + f->descent + f->size / 8) * scale(cls);
}

void tx_draw(canvas_t *c, int colour, int shade, int x, int y, int cls, const uint16_t *cps, int n, box_t *bb, int *ex, int *ey)
{
    if (shade == SH_WHITE) colour = 1; /* white ink */
    const font_face_t *f = face(cls);
    int sc = scale(cls), lh = tx_line_h(cls), top = y;
    int32_t pen = 0; /* 1/64 px along the line, unscaled */
    box_clear(bb);
    for (int i = 0; i < n; i++) {
        if (cps[i] == 10) {
            top += lh, pen = 0;
            continue;
        }
        const font_glyph_t *q = font_glyph(f, cps[i]);
        if (!q) continue;
        int gx = x + sc * (int)((pen + 32) >> 6) + sc * q->xoff, gy = top + sc * f->ascent + sc * q->yoff;
        if (q->w && q->h) box_add(bb, gx, gy), box_add(bb, gx + sc * q->w - 1, gy + sc * q->h - 1);
        if (c) {
            const uint8_t *d = f->data + q->off;
            int wide = q->w > 255;
            for (int row = 0; row < q->h;) {
                int rep = *d++, nr = *d++;
                const uint8_t *runs = d;
                for (int r = 0; r < rep; r++) {
                    const uint8_t *p = runs;
                    for (int k = 0; k < nr; k++) {
                        int rx, rl;
                        if (wide) rx = p[0] | p[1] << 8, rl = p[2] | p[3] << 8, p += 4;
                        else rx = p[0], rl = p[1], p += 2;
                        for (int sy = 0; sy < sc; sy++) cv_span_s(c, gy + (row + r) * sc + sy, gx + rx * sc, gx + (rx + rl) * sc - 1, colour, shade);
                    }
                }
                d = runs + nr * (wide ? 4 : 2);
                row += rep;
            }
        }
        pen += (int32_t)q->adv64;
    }
    if (ex) *ex = x + sc * (int)((pen + 32) >> 6);
    if (ey) *ey = top;
}
