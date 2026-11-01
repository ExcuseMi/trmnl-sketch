/* SPDX-License-Identifier: MIT */
#include "pages.h"
#include <stdlib.h>
#include <string.h>
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

int pg_load(pages_t *p, int num, canvas_t *c)
{
    char path[48];
    pg_path(path, sizeof(path), num, ".png");
    void *f = p->fs->open(p->fs->ctx, path, 0);
    if (!f) return cv_fill(c, 1), PG_MISSING;
    uint8_t *buf = malloc(SK_FILE_MAX);
    if (!buf) return p->fs->close(p->fs->ctx, f), PG_IO;
    size_t got = 0;
    int bad = 0;
    for (;;) {
        int k = p->fs->read(p->fs->ctx, f, buf + got, (int)(SK_FILE_MAX - got));
        if (k < 0) bad = 1;
        if (k <= 0) break;
        got += (size_t)k;
        if (got >= SK_FILE_MAX) {
            bad = 1; /* larger than any page we write */
            break;
        }
    }
    p->fs->close(p->fs->ctx, f);
    int rc = bad ? -1 : png_decode(buf, got, c->px, c->w, c->h, c->stride);
    free(buf);
    if (rc) {
        cv_fill(c, 1);
        return PG_BAD;
    }
    return PG_OK;
}

int pg_save_as(pages_t *p, int num, const char *suffix, const canvas_t *c)
{
    char tmp[48], dst[48];
    pg_path(tmp, sizeof(tmp), num, ".tmp");
    pg_path(dst, sizeof(dst), num, suffix);
    size_t cap = png_max_size(c->w, c->h);
    uint8_t *buf = malloc(cap);
    if (!buf) return PG_IO;
    long n = png_encode(c->px, c->w, c->h, c->stride, buf, cap);
    if (n < 0) return free(buf), PG_IO;
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
    free(buf);
    if (!ok || p->fs->rename(p->fs->ctx, tmp, dst) < 0) {
        p->fs->remove(p->fs->ctx, tmp);
        return PG_IO;
    }
    return PG_OK;
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
    return p->fs->remove(p->fs->ctx, path);
}

int pg_purge_blank(pages_t *p, canvas_t *scratch, int keep)
{
    int gone = 0;
    for (int i = p->n - 1; i >= 0; i--) {
        int num = p->nums[i];
        if (num == keep || pg_load(p, num, scratch) != PG_OK) continue; /* a file that does not decode stays, whatever it is */
        int white = 1, rb = (scratch->w + 7) / 8;
        for (int y = 0; white && y < scratch->h; y++)
            for (int x = 0; x < rb; x++)
                if (scratch->px[y * scratch->stride + x] != 0xff) {
                    white = 0;
                    break;
                }
        if (white && pg_remove(p, num) >= 0) gone++;
    }
    return gone;
}
