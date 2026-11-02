/* The page files: sketch/sk0001.png ... (1-bit PNG, what the Wi-Fi page lists and sends). A save writes a temporary
   file (.tmp, never listed) and renames it over the page, so a power loss leaves the old or the new file whole. A
   missing or damaged file loads as an empty page and is never deleted. SPDX-License-Identifier: MIT */
#ifndef PAGES_H
#define PAGES_H
#include <stddef.h>
#include <stdint.h>
#include "canvas.h"

/* The file system the app has (api->file_*); the host tests use a memory one that can lose power */
typedef struct {
    void *ctx;
    void *(*open)(void *ctx, const char *path, int write);
    int (*read)(void *ctx, void *f, void *buf, int n);
    int (*write)(void *ctx, void *f, const void *buf, int n);
    int (*close)(void *ctx, void *f);
    int (*rename)(void *ctx, const char *from, const char *to); /* replaces the target in one step */
    int (*remove)(void *ctx, const char *path);
    int (*mkdir)(void *ctx, const char *path);
    int (*list)(void *ctx, const char *dir, int (*cb)(const char *name, void *u), void *u); /* cb returns 0 to stop */
} sk_fs_t;

#define PG_LIST_MAX 400
enum { PG_OK = 0, PG_MISSING = -1, PG_BAD = -2, PG_IO = -3 };

typedef struct {
    const sk_fs_t *fs;
    uint16_t nums[PG_LIST_MAX]; /* the pages on disk, ascending */
    int n;
} pages_t;

void pg_init(pages_t *p, const sk_fs_t *fs);
/* Reads the folder (made if missing) */
void pg_scan(pages_t *p);
/* "sketch/sk0007.png" (suffix ".png"), "sketch/sk0007.tmp", "sketch/sk0007-bad.png", "sketch/sk0007-cleared.png" */
void pg_path(char *out, size_t cap, int num, const char *suffix);
/* Position of num in the list, or -1 */
int pg_index(const pages_t *p, int num);
/* The next free number (the highest + 1), 0 when SK_PAGES_MAX is reached */
int pg_next_num(const pages_t *p);
/* PG_OK; PG_MISSING (no file: c is left white); PG_BAD (not readable as a page: c is left white, the file untouched) */
int pg_load(pages_t *p, int num, canvas_t *c);
/* Writes c as page num (tmp, then rename) and adds it to the list. PG_OK or PG_IO */
int pg_save(pages_t *p, int num, const canvas_t *c);
/* The same under another suffix ("-cleared.png"): not a page, not listed */
int pg_save_as(pages_t *p, int num, const char *suffix, const canvas_t *c);
/* The paper of page num (paper.h): a sidecar "skNNNN.pap" ("kind spacing"); none or damaged: Blank (kind 0). Saving Blank removes the
   sidecar and the printed copy "skNNNN-paper.png" (the page with its paper, made by the caller with pg_save_as) */
int pg_paper_save(pages_t *p, int num, int kind, int sp);
void pg_paper_load(pages_t *p, int num, int *kind, int *sp);
/* Moves a page file that could not be read out of the way ("-bad.png"), so a save does not replace it. 0 or < 0 */
int pg_keep_bad(pages_t *p, int num);
/* Deletes the page file and its list entry. The file's remove result: 0 or < 0 */
int pg_remove(pages_t *p, int num);
/* Deletes every page file that decodes as entirely white (a blank page is never kept on disk); a file that does not decode
   stays. `keep`: a page number to spare (0: none). `scratch` is overwritten. Returns how many went */
int pg_purge_blank(pages_t *p, canvas_t *scratch, int keep);
#endif
