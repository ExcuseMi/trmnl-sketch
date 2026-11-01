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

/* The default printer, from the Print app's file (the only source of address and format). 0 with s->prn set; else the
   result screen's words are set (and need_setup when Print has to be opened to set one up) and -1 */
int sk_print_check(sketch_t *s)
{
    s->need_setup = 0;
    if (!s->env.net.post) return fail(s, "Printing is not available on this firmware."), -1;
    int rc = s->env.printer ? s->env.printer(s->env.user, &s->prn) : PJ_NO_PRINTER;
    if (rc == 0) return 0;
    if (rc == PJ_NO_PRINTER) return fail(s, "No printer yet: set one up in Print"), s->need_setup = 1, -1;
    if (rc == APP_ENOTSUP) return fail(s, "This firmware cannot read the Print app's printer."), -1;
    return fail(s, "The printer file could not be read."), -1;
}

int sk_print_page(sketch_t *s, int num)
{
    size_t n = 0;
    uint8_t *file = read_file(s, num, &n);
    if (!file) return fail(s, "The page could not be read."), -1;
    pj_net_t net = s->env.net;
    net.format = s->prn.format;
    int rc = pj_print_png(&net, &s->prn, file, n, &s->pres);
    free(file);
    return rc;
}
