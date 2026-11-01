/* Printing a saved page from inside Sketch: the page file goes through the Print app's own job code (one source,
   ../trmnl-print/src/printjob.c: PWG raster, IPP, error words). SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include <string.h>
#include "sk_cfg.h"
#include "sketch.h"

static uint8_t *read_file(sketch_t *s, int num, size_t *n)
{
    char path[48];
    pg_path(path, sizeof(path), num, ".png");
    const sk_fs_t *fs = s->env.fs;
    void *f = fs->open(fs->ctx, path, 0);
    if (!f) return 0;
    uint8_t *buf = malloc(SK_FILE_MAX);
    size_t got = 0;
    int bad = !buf;
    while (!bad) {
        int k = fs->read(fs->ctx, f, buf + got, (int)(SK_FILE_MAX - got));
        if (k < 0 || (k > 0 && (got += (size_t)k) >= SK_FILE_MAX)) bad = 1;
        if (k <= 0) break;
    }
    fs->close(fs->ctx, f);
    if (bad || !got) return free(buf), (uint8_t *)0;
    *n = got;
    return buf;
}

static void fail(sketch_t *s, const char *msg)
{
    memset(&s->pres, 0, sizeof(s->pres));
    sk_cpy(s->pres.msg, sizeof(s->pres.msg), msg);
}

int sk_print_page(sketch_t *s, int num)
{
    size_t n = 0;
    char printer[80];
    int len = s->env.cfg_get ? s->env.cfg_get(s->env.user, "printer", printer, (int)sizeof(printer) - 1) : -1;
    if (len <= 0) len = (int)strlen(SK_PRINTER_DEFAULT), memcpy(printer, SK_PRINTER_DEFAULT, (size_t)len);
    printer[len] = 0;
    pj_printer_t pr;
    if (pj_printer_parse(printer, &pr) < 0) return fail(s, "The printer address is not valid. Set it in Settings."), -1;
    uint8_t *file = read_file(s, num, &n);
    if (!file) return fail(s, "The page could not be read."), -1;
    int rc = pj_print_png(&s->env.net, &pr, file, n, &s->pres);
    free(file);
    return rc;
}
