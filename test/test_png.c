/* The page file: exact round trip, a valid PNG for any reader (zlib checks the stream), small, and every damaged or
   foreign file refused without touching the page. SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 */
#include "check.h"
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include "canvas.h"
#include "pngio.h"

static void sketch_page(canvas_t *c, int seed)
{
    cv_fill(c, 1);
    srand((unsigned)seed);
    for (int s = 0; s < 30; s++) {
        int x = rand() % c->w, y = rand() % c->h;
        for (int i = 0; i < 40; i++) {
            int nx = x + rand() % 61 - 30, ny = y + rand() % 61 - 30;
            cv_seg(c, x, y, 1 + rand() % 9, nx, ny, 1 + rand() % 9, rand() % 5 ? 0 : 1, 0);
            x = nx, y = ny;
        }
    }
}

static void roundtrip(int w, int h, int seed)
{
    int stride = (w + 7) / 8 + 3; /* a stride wider than the row, as a framebuffer may have */
    uint8_t *a = calloc((size_t)stride, (size_t)h), *b = calloc((size_t)stride, (size_t)h);
    canvas_t c = { a, w, h, stride };
    sketch_page(&c, seed);
    size_t cap = png_max_size(w, h);
    uint8_t *f = malloc(cap);
    long n = png_encode(a, w, h, stride, f, cap);
    CHECK(n > 0 && (size_t)n <= cap);
    memset(b, 0x55, (size_t)stride * (size_t)h);
    CHECK(png_decode(f, (size_t)n, b, w, h, stride) == 0);
    int same = 1;
    for (int y = 0; y < h; y++)
        if (memcmp(a + y * stride, b + y * stride, (size_t)(w + 7) / 8)) same = 0;
    CHECK(same);
    free(a), free(b), free(f);
}

static void test_roundtrip(void)
{
    roundtrip(1872, 1404, 1);
    roundtrip(100, 37, 2);  /* a width that is not a multiple of 8 */
    roundtrip(8, 1, 3);
    roundtrip(1, 1, 4);
    roundtrip(257 * 8, 3, 5); /* rows longer than a 255-byte run */
}

/* The stream is real deflate: zlib inflates it to the filtered rows, and the chunk checksums are right */
static void test_valid_png(void)
{
    int w = 1872, h = 1404, stride = w / 8;
    uint8_t *px = malloc((size_t)stride * h);
    canvas_t c = { px, w, h, stride };
    sketch_page(&c, 9);
    size_t cap = png_max_size(w, h);
    uint8_t *f = malloc(cap);
    long n = png_encode(px, w, h, stride, f, cap);
    CHECK(n > 0 && !memcmp(f, "\x89PNG\r\n\x1a\n", 8));
    uint32_t len = (uint32_t)f[33] << 24 | (uint32_t)f[34] << 16 | (uint32_t)f[35] << 8 | f[36];
    CHECK(!memcmp(f + 37, "IDAT", 4));
    uLong crc = crc32(0, f + 37, 4 + len);
    const uint8_t *cp = f + 41 + len;
    CHECK(crc == ((uLong)cp[0] << 24 | (uLong)cp[1] << 16 | (uLong)cp[2] << 8 | cp[3]));
    uLongf rawn = (uLongf)(stride + 1) * (uLongf)h;
    uint8_t *raw = malloc(rawn);
    uLongf got = rawn;
    CHECK(uncompress(raw, &got, f + 41, len) == Z_OK && got == rawn); /* zlib checks the adler32 too */
    for (int y = 0; y < h; y++) {
        const uint8_t *r = raw + (size_t)y * (stride + 1);
        CHECK(r[0] == 2);
        for (int x = 0; x < stride; x++) {
            uint8_t up = y ? px[(y - 1) * stride + x] : 0;
            if (r[1 + x] != (uint8_t)(px[y * stride + x] - up)) {
                CHECK(0);
                y = h;
                break;
            }
        }
    }
    CHECK(!memcmp(f + n - 12, "\0\0\0\0IEND\xae\x42\x60\x82", 12));
    free(px), free(f), free(raw);
}

static void test_small(void)
{
    int w = 1872, h = 1404, stride = w / 8;
    uint8_t *px = malloc((size_t)stride * h);
    canvas_t c = { px, w, h, stride };
    cv_fill(&c, 1);
    size_t cap = png_max_size(w, h);
    uint8_t *f = malloc(cap);
    long blank = png_encode(px, w, h, stride, f, cap);
    CHECK(blank > 0 && blank < 4096); /* a white page is a few hundred bytes */
    sketch_page(&c, 3);
    long inked = png_encode(px, w, h, stride, f, cap);
    CHECK(inked > blank && inked < 100000); /* a sketch is tens of KB, not the 329 KB of the raw page */
    CHECK(png_encode(px, w, h, stride, f, 100) < 0); /* too small an output: an error, no overrun */
    free(px), free(f);
}

static void test_refused(void)
{
    int w = 200, h = 100, stride = 25;
    uint8_t px[25 * 100], out[25 * 100], keep[25 * 100];
    canvas_t c = { px, w, h, stride };
    sketch_page(&c, 11);
    size_t cap = png_max_size(w, h);
    uint8_t *f = malloc(cap), *g = malloc(cap);
    long n = png_encode(px, w, h, stride, f, cap);
    CHECK(n > 0);
    memset(keep, 0x77, sizeof(keep));
    /* every single byte flipped: refused or (rarely, in an ignored place) the same page, never a crash and never a
       changed page without success */
    int refused = 0;
    for (long i = 0; i < n; i++) {
        memcpy(g, f, (size_t)n);
        g[i] ^= 0x10;
        memcpy(out, keep, sizeof(out));
        int rc = png_decode(g, (size_t)n, out, w, h, stride);
        if (rc) {
            refused++;
            CHECK(!memcmp(out, keep, sizeof(out))); /* the page is untouched on failure */
        } else {
            CHECK(!memcmp(out, px, sizeof(px)));
        }
    }
    CHECK(refused > n * 9 / 10);
    for (long cut = 0; cut < n; cut += 7) { /* truncated */
        memcpy(out, keep, sizeof(out));
        CHECK(png_decode(f, (size_t)cut, out, w, h, stride) != 0);
        CHECK(!memcmp(out, keep, sizeof(out)));
    }
    CHECK(png_decode(f, (size_t)n, out, w + 8, h, stride + 1) != 0); /* a page of another size */
    CHECK(png_decode(f, (size_t)n, out, w, h + 1, stride) != 0);
    uint8_t junk[300];
    srand(3);
    for (int i = 0; i < 300; i++) junk[i] = (uint8_t)rand();
    CHECK(png_decode(junk, sizeof(junk), out, w, h, stride) != 0);
    CHECK(png_decode(f, 0, out, w, h, stride) != 0);
    free(f), free(g);
}

int main(void)
{
    test_roundtrip();
    test_valid_png();
    test_small();
    test_refused();
    return CHECK_DONE("test_png");
}
