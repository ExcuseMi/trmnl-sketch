/* SPDX-License-Identifier: MIT */
#include "pages.h"
#include <stdlib.h>
#include <string.h>
#include "paper.h"
#include "pngio.h"
#include "sk_cfg.h"

void pg_init(pages_t *p, const sk_fs_t *fs)
{
    p->fs = fs;
    p->n = 0;
}

void pg_path(char *out, size_t cap, int num, const char *suffix)
{
    char d[5] = { (char)('0' + num / 1000 % 10), (char)('0' + num / 100 % 10), (char)('0' + num / 10 % 10), (char)('0' + num % 10), 0 };
    const char *parts[5] = { SK_DIR "/sk", d, suffix };
    size_t n = 0;
    for (int i = 0; i < 3; i++)
        for (const char *s = parts[i]; *s && n + 1 < cap; s++) out[n++] = *s;
    out[n] = 0;
}

static int name_num(const char *s) /* "sk0007.png" -> 7, else 0 */
{
    if (strlen(s) != 10 || s[0] != 's' || s[1] != 'k' || strcmp(s + 6, ".png")) return 0;
    int v = 0;
    for (int i = 2; i < 6; i++) {
        if (s[i] < '0' || s[i] > '9') return 0;
        v = v * 10 + (s[i] - '0');
    }
    return v;
}

static void add_num(pages_t *p, int num)
{
    int i = 0;
    while (i < p->n && p->nums[i] < num) i++;
    if (i < p->n && p->nums[i] == num) return;
    if (p->n >= PG_LIST_MAX) return;
    memmove(p->nums + i + 1, p->nums + i, (size_t)(p->n - i) * sizeof(p->nums[0]));
    p->nums[i] = (uint16_t)num;
    p->n++;
}

static int on_name(const char *name, void *u)
{
    int v = name_num(name);
    if (v > 0) add_num(u, v);
    return 1;
}

void pg_scan(pages_t *p)
{
    p->n = 0;
    p->fs->mkdir(p->fs->ctx, SK_DIR); /* fails when it exists: fine */
    p->fs->list(p->fs->ctx, SK_DIR, on_name, p);
}

int pg_index(const pages_t *p, int num)
{
    for (int i = 0; i < p->n; i++)
        if (p->nums[i] == num) return i;
    return -1;
}

int pg_next_num(const pages_t *p)
{
    int top = p->n ? p->nums[p->n - 1] : 0;
    return top >= SK_PAGES_MAX ? 0 : top + 1;
}

/* Page num's file into buf (cap bytes): PG_OK with *got, PG_MISSING, or PG_BAD when it is larger than cap or unreadable */
static int read_page(pages_t *p, int num, uint8_t *buf, size_t cap, size_t *got)
{
    char path[48];
    pg_path(path, sizeof(path), num, ".png");
    void *f = p->fs->open(p->fs->ctx, path, 0);
    if (!f) return PG_MISSING;
    int bad = 0;
    *got = 0;
    for (;;) {
        int k = p->fs->read(p->fs->ctx, f, buf + *got, (int)(cap - *got));
        if (k < 0) bad = 1;
        if (k <= 0) break;
        *got += (size_t)k;
        if (*got >= cap) {
            bad = 1; /* larger than the caller takes */
            break;
        }
    }
    p->fs->close(p->fs->ctx, f);
    return bad ? PG_BAD : PG_OK;
}

int pg_load(pages_t *p, int num, canvas_t *c)
{
    uint8_t *buf = malloc(SK_FILE_MAX);
    if (!buf) return PG_IO;
    size_t got = 0;
    int rc = read_page(p, num, buf, SK_FILE_MAX, &got);
    if (rc == PG_OK && png_decode(buf, got, c->px, c->w, c->h, c->stride)) rc = PG_BAD;
    free(buf);
    if (rc != PG_OK) cv_fill(c, 1);
    return rc;
}

/* Writes n bytes to dst through a temp file and a rename; PG_OK or PG_IO */
static int write_file(pages_t *p, int num, const char *dst, const uint8_t *buf, long n)
{
    char tmp[48];
    pg_path(tmp, sizeof(tmp), num, ".tmp");
    void *f = p->fs->open(p->fs->ctx, tmp, 1);
    int ok = f != 0;
    if (f) {
        for (long off = 0; ok && off < n;) {
            int k = (int)(n - off > 4096 ? 4096 : n - off);
            ok = p->fs->write(p->fs->ctx, f, buf + off, k) == k;
            off += k;
        }
        ok = p->fs->close(p->fs->ctx, f) >= 0 && ok;
    }
    if (!ok || p->fs->rename(p->fs->ctx, tmp, dst) < 0) {
        p->fs->remove(p->fs->ctx, tmp);
        return PG_IO;
    }
    return PG_OK;
}

int pg_save_as(pages_t *p, int num, const char *suffix, const canvas_t *c)
{
    char dst[48];
    pg_path(dst, sizeof(dst), num, suffix);
    size_t cap = png_max_size(c->w, c->h);
    uint8_t *buf = malloc(cap);
    if (!buf) return PG_IO;
    long n = png_encode(c->px, c->w, c->h, c->stride, buf, cap);
    int rc = n < 0 ? PG_IO : write_file(p, num, dst, buf, n);
    free(buf);
    return rc;
}

int pg_paper_save(pages_t *p, int num, int kind, int sp)
{
    char dst[48], t[16];
    pg_path(dst, sizeof(dst), num, ".pap");
    if (kind == 0) { /* blank paper: no sidecar, no printed copy */
        char pc[48];
        void *f = p->fs->open(p->fs->ctx, dst, 0); /* only touch the files when there is something to remove */
        if (!f) return PG_OK;
        p->fs->close(p->fs->ctx, f);
        pg_path(pc, sizeof(pc), num, "-paper.png");
        p->fs->remove(p->fs->ctx, pc);
        p->fs->remove(p->fs->ctx, dst);
        return PG_OK;
    }
    int n = 0, v[2] = { kind, sp };
    for (int k = 0; k < 2; k++) {
        char d[8];
        int m = 0, x = v[k];
        do d[m++] = (char)('0' + x % 10), x /= 10; while (x);
        while (m) t[n++] = d[--m];
        t[n++] = k ? '\n' : ' ';
    }
    return write_file(p, num, dst, (const uint8_t *)t, n);
}

void pg_paper_load(pages_t *p, int num, int *kind, int *sp)
{
    char path[48], b[16];
    *kind = 0, *sp = 0;
    pg_path(path, sizeof(path), num, ".pap");
    void *f = p->fs->open(p->fs->ctx, path, 0);
    if (!f) return; /* an old page: Blank */
    int n = p->fs->read(p->fs->ctx, f, b, 15);
    p->fs->close(p->fs->ctx, f);
    int v[2] = { 0, 0 }, k = 0;
    for (int i = 0; i < n && k < 2; i++) {
        if (b[i] >= '0' && b[i] <= '9' && v[k] < 100000) v[k] = v[k] * 10 + b[i] - '0';
        else if (b[i] == ' ' || b[i] == '\n') k++;
        else return; /* damaged: Blank */
    }
    if (v[0] > 0 && v[0] < PP_N) *kind = v[0], *sp = pp_spacing(v[1]);
}

int pg_save(pages_t *p, int num, const canvas_t *c)
{
    int rc = pg_save_as(p, num, ".png", c);
    if (rc == PG_OK) add_num(p, num);
    return rc;
}

int pg_keep_bad(pages_t *p, int num)
{
    char a[48], b[48];
    pg_path(a, sizeof(a), num, ".png");
    pg_path(b, sizeof(b), num, "-bad.png");
    return p->fs->rename(p->fs->ctx, a, b);
}

int pg_remove(pages_t *p, int num)
{
    char path[48];
    pg_path(path, sizeof(path), num, ".png");
    int i = pg_index(p, num);
    if (i >= 0) {
        for (int k = i; k + 1 < p->n; k++) p->nums[k] = p->nums[k + 1];
        p->n--;
    }
    char side[48];
    pg_path(side, sizeof(side), num, ".pap");
    p->fs->remove(p->fs->ctx, side);
    pg_path(side, sizeof(side), num, "-paper.png");
    p->fs->remove(p->fs->ctx, side);
    return p->fs->remove(p->fs->ctx, path);
}

int pg_purge_blank(pages_t *p, canvas_t *scratch, int keep)
{
    int gone = 0;
    uint8_t *buf = malloc(PG_BLANK_MAX + 1);
    if (!buf) return 0;
    for (int i = p->n - 1; i >= 0; i--) {
        int num = p->nums[i];
        size_t got = 0;
        /* a page with ink is a larger file: only a small one is decoded (owner 2026-11-03: the start decoded every page, 5 s with 40) */
        if (num == keep || read_page(p, num, buf, PG_BLANK_MAX + 1, &got) != PG_OK) continue;
        if (png_decode(buf, got, scratch->px, scratch->w, scratch->h, scratch->stride)) continue; /* a file that does not decode stays, whatever it is */
        int white = 1, rb = (scratch->w + 7) / 8;
        for (int y = 0; white && y < scratch->h; y++)
            for (int x = 0; x < rb; x++)
                if (scratch->px[y * scratch->stride + x] != 0xff) {
                    white = 0;
                    break;
                }
        if (white && pg_remove(p, num) >= 0) gone++;
    }
    free(buf);
    return gone;
}
