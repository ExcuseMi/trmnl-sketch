/* SPDX-License-Identifier: MIT */
#include "pngio.h"
#include <stdlib.h>
#include <string.h>

static uint32_t crc_tab[256];
static int crc_ready;

static uint32_t crc32(uint32_t c, const uint8_t *p, size_t n)
{
    if (!crc_ready) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t v = i;
            for (int k = 0; k < 8; k++) v = v & 1 ? 0xedb88320u ^ (v >> 1) : v >> 1;
            crc_tab[i] = v;
        }
        crc_ready = 1;
    }
    c = ~c;
    while (n--) c = crc_tab[(c ^ *p++) & 0xff] ^ (c >> 8);
    return ~c;
}

static void be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24), p[1] = (uint8_t)(v >> 16), p[2] = (uint8_t)(v >> 8), p[3] = (uint8_t)v;
}
static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }

static const uint8_t SIG[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n' };
static const uint16_t len_base[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
static const uint8_t len_extra[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
static const uint16_t dist_base[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
static const uint8_t dist_extra[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };

size_t png_max_size(int w, int h)
{
    size_t row = (size_t)(w + 7) / 8 + 1;
    return row * (size_t)h * 9 / 8 + 256;
}

/* ---- writer ---- */

typedef struct {
    uint8_t *out;
    size_t pos, cap;
    uint32_t acc;
    int nb, bad;
    int last, run; /* the byte run being collected */
    uint32_t a, b; /* adler32 */
    int rowlen, same_rows; /* a filtered row, and how many in a row repeat the one before */
    const uint8_t *prev;   /* the row they repeat */
} enc_t;

static void put_byte(enc_t *e, uint8_t v)
{
    if (e->pos >= e->cap) e->bad = 1;
    else e->out[e->pos++] = v;
}
static void put_bits(enc_t *e, uint32_t v, int n)
{
    e->acc |= v << e->nb;
    e->nb += n;
    while (e->nb >= 8) {
        put_byte(e, (uint8_t)e->acc);
        e->acc >>= 8;
        e->nb -= 8;
    }
}
static void put_code(enc_t *e, uint32_t code, int len) /* Huffman codes go most significant bit first */
{
    uint32_t r = 0;
    for (int i = 0; i < len; i++) r |= ((code >> i) & 1u) << (len - 1 - i);
    put_bits(e, r, len);
}
static void put_lit(enc_t *e, int v)
{
    if (v < 144) put_code(e, 0x30u + (uint32_t)v, 8);
    else put_code(e, 0x190u + (uint32_t)(v - 144), 9);
}
static void put_copy(enc_t *e, int len, int dist) /* a copy of `len` (3..258) bytes from `dist` back (1..32768) */
{
    int s = 28;
    while (len_base[s] > len) s--;
    uint32_t sym = 257u + (uint32_t)s;
    if (sym < 280) put_code(e, sym - 256, 7);
    else put_code(e, 0xc0u + (sym - 280), 8);
    if (len_extra[s]) put_bits(e, (uint32_t)(len - len_base[s]), len_extra[s]);
    int dc = 29;
    while (dist_base[dc] > dist) dc--;
    put_code(e, (uint32_t)dc, 5);
    if (dist_extra[dc]) put_bits(e, (uint32_t)(dist - dist_base[dc]), dist_extra[dc]);
}

static void flush_run(enc_t *e)
{
    if (e->run <= 0) return;
    put_lit(e, e->last);
    int rest = e->run - 1;
    while (rest >= 3) {
        int l = rest > 258 ? 258 : rest;
        put_copy(e, l, 1);
        rest -= l;
    }
    while (rest-- > 0) put_lit(e, e->last);
    e->run = 0;
}

/* The rows that repeated the one before: copies of up to 258 bytes from one row back (flush_run first) */
static void flush_rows(enc_t *e)
{
    if (e->same_rows == 0) return;
    flush_run(e);
    long left = (long)e->same_rows * e->rowlen;
    if (left < 3) { /* one tiny row: literals */
        for (long i = 0; i < left; i++) put_lit(e, e->prev[i]);
    }
    while (left >= 3) {
        int l = left > 258 ? 258 : (int)left;
        if (left > 258 && left - 258 < 3) l = (int)left - 3; /* never leave a tail a copy cannot take */
        put_copy(e, l, e->rowlen);
        left -= l;
    }
    e->same_rows = 0;
}

static void feed(enc_t *e, uint8_t v)
{
    e->a = (e->a + v) % 65521u;
    e->b = (e->b + e->a) % 65521u;
    if (e->run > 0 && v == e->last) {
        e->run++;
        return;
    }
    flush_run(e);
    e->last = v;
    e->run = 1;
}

long png_encode(const uint8_t *px, int w, int h, int stride, uint8_t *out, size_t cap)
{
    if (w < 1 || h < 1 || w > 65535 || h > 65535) return -1;
    int rb = (w + 7) / 8;
    if (stride < rb || cap < 64) return -1;
    enc_t e = { out, 0, cap, 0, 0, 0, 0, 0, 1, 0, rb + 1, 0, 0 };
    for (int i = 0; i < 8; i++) put_byte(&e, SIG[i]);
    uint8_t ih[25] = { 0, 0, 0, 13, 'I', 'H', 'D', 'R' };
    be32(ih + 8, (uint32_t)w), be32(ih + 12, (uint32_t)h);
    ih[16] = 1; /* 1 bit, gray, deflate, adaptive filtering, no interlace */
    be32(ih + 21, crc32(0, ih + 4, 17));
    for (int i = 0; i < 25; i++) put_byte(&e, ih[i]);
    size_t idat = e.pos;
    for (int i = 0; i < 8; i++) put_byte(&e, 0); /* length and type, filled in below */
    size_t data0 = e.pos;
    put_byte(&e, 0x78), put_byte(&e, 0x01);
    put_bits(&e, 1, 1); /* the last block */
    put_bits(&e, 1, 2); /* fixed Huffman */
    size_t rl = (size_t)rb + 1; /* a filtered row */
    uint8_t *row = malloc(rl), *prev = malloc(rl);
    if (!row || !prev) return free(row), free(prev), -1;
    e.prev = prev;
    for (int y = 0; y < h && !e.bad; y++) {
        row[0] = 2; /* filter Up */
        for (int x = 0; x < rb; x++) row[1 + x] = (uint8_t)(px[y * stride + x] - (y ? px[(y - 1) * stride + x] : 0));
        if (y && !memcmp(row, prev, rl)) { /* the same row again: a copy from one row back */
            for (int i = 0; i <= rb; i++) {
                e.a = (e.a + row[i]) % 65521u;
                e.b = (e.b + e.a) % 65521u;
            }
            e.same_rows++;
            continue;
        }
        flush_rows(&e);
        for (int i = 0; i <= rb; i++) feed(&e, row[i]);
        memcpy(prev, row, rl);
    }
    flush_rows(&e);
    free(row), free(prev);
    flush_run(&e);
    put_code(&e, 0, 7); /* end of block */
    if (e.nb) put_bits(&e, 0, 8 - e.nb);
    uint32_t ad = e.b << 16 | e.a;
    for (int i = 3; i >= 0; i--) put_byte(&e, (uint8_t)(ad >> (8 * i)));
    if (e.bad) return -1;
    be32(out + idat, (uint32_t)(e.pos - data0));
    memcpy(out + idat + 4, "IDAT", 4);
    uint32_t c = crc32(0, out + idat + 4, e.pos - idat - 4);
    for (int i = 3; i >= 0; i--) put_byte(&e, (uint8_t)(c >> (8 * i)));
    static const uint8_t end[12] = { 0, 0, 0, 0, 'I', 'E', 'N', 'D', 0xae, 0x42, 0x60, 0x82 };
    for (int i = 0; i < 12; i++) put_byte(&e, end[i]);
    return e.bad ? -1 : (long)e.pos;
}

/* ---- reader ---- */

typedef struct {
    const uint8_t *in;
    size_t n, pos;
    uint32_t bitbuf;
    int nb, bad, ghost; /* bits in bitbuf; of them, ghost zero bits past the end of the input (taking one is an error) */
    uint8_t *raw; /* the inflated stream (filter byte and row, h times): matches copy from it */
    uint32_t total, cap;
} dec_t;

/* The bits come a byte at a time into a 32-bit buffer and the fixed codes are looked up 9 bits at once (owner 2026-11-03:
   a bit per call made opening a page and every gallery thumbnail slow) */
static void refill(dec_t *d)
{
    while (d->nb <= 24) {
        uint32_t b = 0;
        if (d->pos < d->n) b = d->in[d->pos++];
        else d->ghost += 8;
        d->bitbuf |= b << d->nb;
        d->nb += 8;
    }
}
static uint32_t get_bits(dec_t *d, int n) /* least significant first, n <= 16 */
{
    if (d->nb < n) refill(d);
    uint32_t v = d->bitbuf & ((1u << n) - 1);
    d->bitbuf >>= n;
    d->nb -= n;
    if (d->nb < d->ghost) d->bad = 1;
    return v;
}

/* The fixed literal/length code: the next 9 bits (as they come) to the symbol | its length << 9 */
static uint16_t fix_tab[512];
static void fix_init(void)
{
    if (fix_tab[0]) return;
    for (int sym = 0; sym < 288; sym++) {
        uint32_t code = sym < 144 ? 0x30u + (uint32_t)sym : sym < 256 ? 0x190u + (uint32_t)(sym - 144) : sym < 280 ? (uint32_t)(sym - 256) : 0xc0u + (uint32_t)(sym - 280);
        int len = sym < 144 ? 8 : sym < 256 ? 9 : sym < 280 ? 7 : 8;
        uint32_t r = 0;
        for (int i = 0; i < len; i++) r |= ((code >> i) & 1u) << (len - 1 - i);
        for (uint32_t k = r; k < 512; k += 1u << len) fix_tab[k] = (uint16_t)(sym | len << 9);
    }
}

static void emit(dec_t *d, uint8_t b)
{
    if (d->total >= d->cap) d->bad = 1;
    else d->raw[d->total++] = b;
}

static int inflate_block(dec_t *d, int fixed)
{
    if (!fixed) { /* stored */
        get_bits(d, d->nb & 7); /* to the byte boundary; whole bytes still in the buffer go back to the input */
        d->pos -= (size_t)((d->nb - d->ghost) / 8);
        d->nb = d->ghost = 0, d->bitbuf = 0;
        if (d->pos + 4 > d->n) return -1;
        unsigned len = d->in[d->pos] | d->in[d->pos + 1] << 8, nlen = d->in[d->pos + 2] | d->in[d->pos + 3] << 8;
        d->pos += 4;
        if ((len ^ 0xffffu) != nlen || d->pos + len > d->n) return -1;
        if (len > d->cap - d->total) return -1;
        memcpy(d->raw + d->total, d->in + d->pos, len);
        d->total += len, d->pos += len;
        return d->bad ? -1 : 0;
    }
    for (;;) {
        if (d->nb < 9) refill(d);
        int e = fix_tab[d->bitbuf & 511], sym = e & 511;
        get_bits(d, e >> 9);
        if (d->bad) return -1;
        if (sym < 256) emit(d, (uint8_t)sym);
        else if (sym == 256) return 0;
        else {
            int s = sym - 257;
            if (s > 28) return -1;
            int len = len_base[s] + (int)get_bits(d, len_extra[s]);
            uint32_t dc = get_bits(d, 5), rv = 0; /* a 5-bit code, most significant bit first */
            for (int i = 0; i < 5; i++) rv |= ((dc >> i) & 1u) << (4 - i);
            dc = rv;
            if (dc > 29) return -1;
            uint32_t dist = dist_base[dc] + get_bits(d, dist_extra[dc]);
            if (dist > d->total || dist > 32768 || (uint32_t)len > d->cap - d->total) return -1;
            uint8_t *o = d->raw + d->total;
            for (int i = 0; i < len; i++) o[i] = o[i - (int)dist]; /* byte by byte: a match may overlap itself */
            d->total += (uint32_t)len;
        }
        if (d->bad) return -1;
    }
}

int png_decode(const uint8_t *in, size_t n, uint8_t *px, int w, int h, int stride)
{
    if (w < 1 || h < 1 || w > 65535 || h > 65535) return -1;
    int rb = (w + 7) / 8;
    if (n < 8 + 25 || memcmp(in, SIG, 8) || stride < rb) return -1;
    const uint8_t *ih = in + 8;
    if (rd32(ih) != 13 || memcmp(ih + 4, "IHDR", 4) || rd32(ih + 8) != (uint32_t)w || rd32(ih + 12) != (uint32_t)h ||
        ih[16] != 1 || ih[17] != 0 || ih[18] != 0 || ih[19] != 0 || ih[20] != 0 || rd32(ih + 21) != crc32(0, ih + 4, 17))
        return -1;
    size_t p = 8 + 25;
    const uint8_t *idat = 0;
    size_t idat_n = 0;
    int ended = 0;
    while (p + 12 <= n) {
        uint32_t len = rd32(in + p);
        if (len > n || p + 12 + len > n) return -1;
        const uint8_t *type = in + p + 4, *data = in + p + 8;
        if (!memcmp(type, "IDAT", 4)) {
            if (idat) return -1; /* one chunk only */
            if (rd32(data + len) != crc32(0, type, 4 + len)) return -1;
            idat = data, idat_n = len;
        } else if (!memcmp(type, "IEND", 4)) {
            ended = 1;
            break;
        }
        p += 12 + len;
    }
    if (!idat || !ended || idat_n < 6) return -1;
    if ((idat[0] & 0x0f) != 8 || ((idat[0] << 8) | idat[1]) % 31 || (idat[1] & 0x20)) return -1;
    uint32_t cap = (uint32_t)(rb + 1) * (uint32_t)h; /* the stream is the rows with their filter bytes, nothing more */
    uint8_t *raw = malloc(cap);
    if (!raw) return -1;
    fix_init();
    dec_t d = { idat, idat_n - 4, 2, 0, 0, 0, 0, raw, 0, cap };
    int rc = 0, last = 0;
    while (!last && rc == 0) {
        last = (int)get_bits(&d, 1);
        uint32_t t = get_bits(&d, 2);
        rc = d.bad || t > 1 ? -1 : inflate_block(&d, (int)t);
    }
    if (rc == 0 && (d.bad || d.total != cap)) rc = -1;
    for (int y = 0; rc == 0 && y < h; y++) /* the filters (none, sub, up) checked, then undone into px: px is only written for a good file */
        if (raw[(size_t)y * (size_t)(rb + 1)] > 2) rc = -1;
    for (int y = 0; rc == 0 && y < h; y++) {
        const uint8_t *r = raw + (size_t)y * (size_t)(rb + 1);
        uint8_t *o = px + (size_t)y * (size_t)stride, *up = y ? o - stride : 0;
        if (r[0] == 0 || (r[0] == 2 && !y)) memcpy(o, r + 1, (size_t)rb);
        else if (r[0] == 2)
            for (int i = 0; i < rb; i++) o[i] = (uint8_t)(r[1 + i] + up[i]);
        else
            for (int i = 0; i < rb; i++) o[i] = (uint8_t)(r[1 + i] + (i ? o[i - 1] : 0));
    }
    free(raw);
    return rc;
}
