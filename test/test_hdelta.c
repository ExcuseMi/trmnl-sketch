/* Undo by deltas (hdelta.h, hist.h): the codec gives back the exact bytes; a long random history of strokes, clears,
   undos, redos and bakes gives the page as drawn at every point, with no pool, a pool too small for most deltas (the
   oldest give theirs up: drawn again) and one with room, presses left stale (lazy) and settled once.
   SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 */
#include "check.h"
#include <stdlib.h>
#include <string.h>
#include "hdelta.h"
#include "hist.h"

#define W 320
#define H 240
#define S (W / 8)
#define BYTES (S * H)
#define OPS 400

static uint8_t pa[BYTES], pb[BYTES], pc[BYTES], enc[2 * BYTES];

static void test_codec(void)
{
    canvas_t a = { pa, W, H, S }, b = { pb, W, H, S }, c = { pc, W, H, S };
    srand(7);
    for (int round = 0; round < 40; round++) {
        for (int i = 0; i < BYTES; i++) { /* zeros, long repeats (over 63 bytes), short ones, noise */
            int y = i / S, x = i % S, kind = (y / 7 + round) % 4;
            pa[i] = (uint8_t)rand();
            pb[i] = kind == 0 ? pa[i] : kind == 1 ? (uint8_t)(pa[i] ^ (y & 1 ? 0xaa : 0xff)) : kind == 2 ? (uint8_t)(pa[i] ^ (x / 2 % 3 ? 0 : 0x41)) : (uint8_t)rand();
        }
        box_t bb = { rand() % W - 20, rand() % H - 20, rand() % W + 30, rand() % H + 30 }; /* may stick out of the page */
        if (round == 0) bb = (box_t){ 0, 0, W - 1, H - 1 };
        if (bb.x0 > bb.x1 || bb.y0 > bb.y1) continue;
        uint32_t n = hd_encode(&a, &b, &bb, 0);
        CHECK(n <= sizeof(enc) && hd_encode(&a, &b, &bb, enc) == n);
        memcpy(pc, pb, BYTES);
        hd_xor(&c, &bb, enc, n);
        box_t k = bb;
        box_clip(&k, W, H);
        int bad = 0;
        for (int y = 0; y < H; y++)
            for (int x = 0; x < S; x++) {
                int in = y >= k.y0 && y <= k.y1 && x >= k.x0 / 8 && x <= k.x1 / 8;
                bad += pc[y * S + x] != (in ? pa[y * S + x] : pb[y * S + x]);
            }
        CHECK(!bad);
        hd_xor(&c, &bb, enc, n); /* the same delta again: back (redo after undo) */
        CHECK(!memcmp(pc, pb, BYTES));
    }
    memcpy(pb, pa, BYTES); /* nothing changed: a skip token a row */
    CHECK(hd_encode(&a, &b, &(box_t){ 0, 0, W - 1, H - 1 }, 0) == H);
}

static uint8_t page_px[BYTES], base_px[BYTES], shadow_px[BYTES], pool[96 * 1024], ref[OPS + 1][BYTES];

static void scribble(hist_t *h, int seed)
{
    srand((unsigned)seed);
    int kind = seed % 9 == 0 ? HK_ERASE : HK_INK;
    bstyle_t st = { (uint8_t)(seed % 3 == 0 ? BR_BRUSH : BR_PEN), (uint8_t)(seed % 4 == 1 ? SH_GREY : SH_BLACK), (uint8_t)(6 + seed % 30), 0, 0 };
    hist_begin_style(h, kind, &st);
    int x = rand() % W, y = rand() % H, n = 5 + rand() % 40;
    for (int i = 0; i < n; i++) {
        x += rand() % 41 - 20, y += rand() % 31 - 15;
        x = x < 0 ? 0 : x >= W ? W - 1 : x;
        y = y < 0 ? 0 : y >= H ? H - 1 : y;
        hist_add(h, x, y, kind == HK_ERASE ? 15 : brush_param(st.brush, st.size, 20000 + (unsigned)rand() % 40000), 0);
    }
    hist_end(h);
}

/* the page after each press is the page as it was drawn at that point of the history */
static void test_history(uint32_t cap, int lazy)
{
    static hpt_t pts[1500];
    static hstroke_t st[20];
    canvas_t page = { page_px, W, H, S };
    hist_t h;
    cv_fill(&page, 1);
    hist_init(&h, &page, base_px, pts, 1500, st, 20);
    if (cap) hist_set_deltas(&h, shadow_px, pool, cap);
    h.lazy = lazy;
    int pos = 0, top = 0, bad = 0, settles = 0;
    memcpy(ref[0], page_px, BYTES);
    srand(11 + cap);
    for (int op = 0; op < OPS; op++) {
        int r = rand() % 10;
        box_t b;
        if (r < 4) { /* a stroke ends the redo list */
            scribble(&h, op);
            memcpy(ref[++pos], page_px, BYTES), top = pos;
        } else if (r == 4 && op % 3 == 0) {
            hist_clear(&h, &b);
            memcpy(ref[++pos], page_px, BYTES), top = pos;
        } else if (r < 8) { /* a row of undos in one frame, then one settle */
            for (int k = rand() % 4; k >= 0; k--) {
                int could = hist_can_undo(&h);
                CHECK(hist_undo(&h, &b) == could);
                pos -= could;
            }
        } else if (r < 9) {
            for (int k = rand() % 4; k >= 0; k--) {
                int could = hist_can_redo(&h);
                CHECK(hist_redo(&h, &b) == could && pos + could <= top);
                pos += could;
            }
        } else {
            hist_bake_step(&h, 6);
        }
        if (hist_settle(&h, &b)) settles++;
        CHECK(!h.stale);
        bad += memcmp(page_px, ref[pos], BYTES) != 0;
        if (h.shadow.px && !h.open) bad += memcmp(shadow_px, page_px, BYTES) != 0; /* the deltas are taken against it */
    }
    fprintf(stderr, "  pool %u lazy %d: %d pages not as drawn, %d settles, %u bytes of deltas\n", cap, lazy, bad, settles, hist_delta_bytes(&h));
    CHECK(!bad);
    if (cap >= sizeof(pool) && lazy) CHECK(settles == 0); /* with room every press is a delta */
    if (!cap && lazy) CHECK(settles > 0);
}

int main(void)
{
    test_codec();
    static const uint32_t caps[] = { 0, 300, 4096, sizeof(pool) };
    for (int i = 0; i < 4; i++)
        for (int lazy = 0; lazy < 2; lazy++) test_history(caps[i], lazy);
    return CHECK_DONE("test_hdelta");
}
