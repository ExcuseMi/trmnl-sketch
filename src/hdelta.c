/* SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 */
#include "hdelta.h"
#include <stddef.h>

/* A token: the type in the top 2 bits, the count 1..63 in the low 6, or 0 there and the count 1..65535 in the next two
   bytes (high first). A repeat is followed by its byte, a literal by its bytes */
enum { HD_SKIP, HD_REP, HD_LIT };

static int clip(const canvas_t *c, const box_t *bb, int *c0, int *c1, int *y0, int *y1)
{
    box_t b = *bb;
    box_clip(&b, c->w, c->h);
    if (box_empty(&b)) return 0;
    *c0 = b.x0 >> 3, *c1 = b.x1 >> 3, *y0 = b.y0, *y1 = b.y1;
    return 1;
}

static uint32_t token(uint8_t *out, uint32_t o, int type, int n)
{
    if (n < 64) {
        if (out) out[o] = (uint8_t)(type << 6 | n);
        return o + 1;
    }
    if (out) out[o] = (uint8_t)(type << 6), out[o + 1] = (uint8_t)(n >> 8), out[o + 2] = (uint8_t)n;
    return o + 3;
}

uint32_t hd_encode(const canvas_t *a, const canvas_t *b, const box_t *bb, uint8_t *out)
{
    int c0, c1, y0, y1;
    if (!clip(a, bb, &c0, &c1, &y0, &y1)) return 0;
    uint32_t o = 0;
    for (int y = y0; y <= y1; y++) {
        const uint8_t *pa = a->px + (size_t)y * (size_t)a->stride, *pb = b->px + (size_t)y * (size_t)b->stride;
        int j = c0;
        while (j <= c1) {
            uint8_t v = pa[j] ^ pb[j];
            int k = j + 1;
            while (k <= c1 && k - j < 65535 && (uint8_t)(pa[k] ^ pb[k]) == v) k++;
            if (!v) {
                o = token(out, o, HD_SKIP, k - j);
            } else if (k - j >= 3) {
                o = token(out, o, HD_REP, k - j);
                if (out) out[o] = v;
                o++;
            } else { /* a literal up to a zero or the start of a repeat */
                k = j + 1;
                while (k <= c1 && k - j < 65535) {
                    uint8_t w = pa[k] ^ pb[k];
                    if (!w) break;
                    if (k + 2 <= c1 && (uint8_t)(pa[k + 1] ^ pb[k + 1]) == w && (uint8_t)(pa[k + 2] ^ pb[k + 2]) == w) break;
                    k++;
                }
                o = token(out, o, HD_LIT, k - j);
                for (int i = j; i < k; i++, o++)
                    if (out) out[o] = pa[i] ^ pb[i];
            }
            j = k;
        }
    }
    return o;
}

void hd_xor(canvas_t *c, const box_t *bb, const uint8_t *d, uint32_t n)
{
    int c0, c1, y0, y1;
    if (!clip(c, bb, &c0, &c1, &y0, &y1)) return;
    uint32_t i = 0;
    int y = y0, j = c0;
    uint8_t *row = c->px + (size_t)y * (size_t)c->stride;
    while (i < n && y <= y1) {
        int type = d[i] >> 6, k = d[i] & 63;
        i++;
        if (!k) k = d[i] << 8 | d[i + 1], i += 2;
        if (k > c1 - j + 1) k = c1 - j + 1; /* never past the row (a delta of another box) */
        if (type == HD_REP) {
            uint8_t v = d[i++];
            for (int e = j + k; j < e; j++) row[j] ^= v;
        } else if (type == HD_LIT) {
            for (int e = j + k; j < e; j++) row[j] ^= d[i++];
        } else {
            j += k;
        }
        if (j > c1) j = c0, y++, row += c->stride;
    }
}
