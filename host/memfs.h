/* A memory file system for the host tests, with the failures the flash has. SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 */
#ifndef MEMFS_H
#define MEMFS_H
#include <stddef.h>
#include <stdint.h>
#include "pages.h"

#define MEMFS_FILES 64
typedef struct {
    char name[64];
    uint8_t *data;
    size_t n;
    int used;
} memfile_t;

typedef struct {
    memfile_t f[MEMFS_FILES];
    int fail_rename;      /* power lost between the write and the rename: the rename does not happen */
    int fail_write;       /* the disk is full: writes fail */
    int fail_open_write;  /* no file can be created */
    long removes;         /* remove() calls (nothing may be deleted but temporary files) */
    long renames_ok;
    sk_fs_t fs;
} memfs_t;

void memfs_init(memfs_t *m);
void memfs_free(memfs_t *m);
memfile_t *memfs_find(memfs_t *m, const char *name);
/* creates or replaces */
void memfs_put(memfs_t *m, const char *name, const void *data, size_t n);
#endif
