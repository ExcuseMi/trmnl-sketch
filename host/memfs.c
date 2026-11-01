/* SPDX-License-Identifier: MIT */
#include "memfs.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    memfs_t *m;
    char name[64];
    int write;
    uint8_t *buf;
    size_t n, cap, pos;
    memfile_t *src;
} handle_t;

memfile_t *memfs_find(memfs_t *m, const char *name)
{
    for (int i = 0; i < MEMFS_FILES; i++)
        if (m->f[i].used && !strcmp(m->f[i].name, name)) return &m->f[i];
    return 0;
}

void memfs_put(memfs_t *m, const char *name, const void *data, size_t n)
{
    memfile_t *f = memfs_find(m, name);
    for (int i = 0; !f && i < MEMFS_FILES; i++)
        if (!m->f[i].used) f = &m->f[i];
    if (!f) abort();
    free(f->data);
    f->data = malloc(n ? n : 1);
    memcpy(f->data, data, n);
    f->n = n;
    f->used = 1;
    strncpy(f->name, name, sizeof(f->name) - 1);
}

static void *fs_open(void *c, const char *path, int write)
{
    memfs_t *m = c;
    handle_t *h = calloc(1, sizeof(*h));
    h->m = m;
    strncpy(h->name, path, sizeof(h->name) - 1);
    h->write = write;
    if (write) {
        if (m->fail_open_write) return free(h), (void *)0;
        return h;
    }
    h->src = memfs_find(m, path);
    if (!h->src) return free(h), (void *)0;
    return h;
}

static int fs_read(void *c, void *f, void *buf, int n)
{
    (void)c;
    handle_t *h = f;
    if (h->write || !h->src) return -1;
    size_t left = h->src->n - h->pos;
    size_t k = (size_t)n < left ? (size_t)n : left;
    memcpy(buf, h->src->data + h->pos, k);
    h->pos += k;
    return (int)k;
}

static int fs_write(void *c, void *f, const void *buf, int n)
{
    memfs_t *m = c;
    handle_t *h = f;
    if (m->fail_write) return -1;
    if (h->n + (size_t)n > h->cap) {
        h->cap = (h->n + (size_t)n) * 2;
        h->buf = realloc(h->buf, h->cap);
    }
    memcpy(h->buf + h->n, buf, (size_t)n);
    h->n += (size_t)n;
    return n;
}

static int fs_close(void *c, void *f)
{
    (void)c;
    handle_t *h = f;
    if (h->write) memfs_put(h->m, h->name, h->buf ? h->buf : (uint8_t *)"", h->n); /* a closed file is whole; a lost one never closes */
    free(h->buf);
    free(h);
    return 0;
}

static int fs_rename(void *c, const char *a, const char *b)
{
    memfs_t *m = c;
    memfile_t *f = memfs_find(m, a);
    if (m->fail_rename || !f) return -1;
    memfile_t *t = memfs_find(m, b);
    if (t) free(t->data), t->used = 0, t->data = 0;
    strncpy(f->name, b, sizeof(f->name) - 1);
    m->renames_ok++;
    return 0;
}

static int fs_remove(void *c, const char *p)
{
    memfs_t *m = c;
    memfile_t *f = memfs_find(m, p);
    m->removes++;
    if (!f) return -1;
    free(f->data);
    f->data = 0;
    f->used = 0;
    return 0;
}

static int fs_mkdir(void *c, const char *p) { return (void)c, (void)p, 0; }

static int fs_list(void *c, const char *dir, int (*cb)(const char *, void *), void *u)
{
    memfs_t *m = c;
    size_t dl = strlen(dir);
    for (int i = 0; i < MEMFS_FILES; i++) {
        if (!m->f[i].used || strncmp(m->f[i].name, dir, dl) || m->f[i].name[dl] != '/') continue;
        if (!cb(m->f[i].name + dl + 1, u)) break;
    }
    return 0;
}

void memfs_init(memfs_t *m)
{
    memset(m, 0, sizeof(*m));
    m->fs = (sk_fs_t){ m, fs_open, fs_read, fs_write, fs_close, fs_rename, fs_remove, fs_mkdir, fs_list };
}

void memfs_free(memfs_t *m)
{
    for (int i = 0; i < MEMFS_FILES; i++)
        if (m->f[i].used) free(m->f[i].data);
}
