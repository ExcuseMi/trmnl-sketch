/* Sketch's settings reads with and without the X's device settings (API 2.11). SPDX-License-Identifier: MIT */
#include "check.h"
#include <stdio.h>
#include <string.h>
#include "skcfg.h"

static int own_calls, dev_calls, declares, declared_n;
static int own_get(const char *k, void *b, size_t cap)
{
    own_calls++;
    return snprintf(b, cap, "own-%s", k);
}
static int dev_get(const char *k, char *b, size_t cap)
{
    dev_calls++;
    if (!strcmp(k, "cleanup_s")) return snprintf(b, cap, "30");
    if (!strcmp(k, "pen_area")) return snprintf(b, cap, "keep");
    return APP_ENOENT;
}
static int refuse, logged;
static void log_fn(const char *fmt, ...) { (void)fmt; logged++; }
static int removed_declared; /* a setting of a removed feature in the form */
static int decl_fn(const app_setting_decl_t *d, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++)
        if (!strcmp(d[i].key, "smooth") || !strcmp(d[i].key, "pressure_curve") || !strcmp(d[i].key, "use_tilt")) removed_declared++;
    declares++, declared_n = (int)n;
    return refuse;
}

int main(void)
{
    app_api_t a;
    char b[16];
    memset(&a, 0, sizeof(a));
    a.setting_get = own_get, a.device_setting_get = dev_get, a.settings_declare = decl_fn;
    a.size = sizeof(a);                                /* 2.11 */
    CHECK(skcfg_get(&a, "cleanup_s", b, sizeof(b)) == 2 && !strcmp(b, "30") && own_calls == 0);
    CHECK(skcfg_get(&a, "pen_area", b, sizeof(b)) == 4 && !strcmp(b, "keep"));
    CHECK(skcfg_get(&a, "size", b, sizeof(b)) > 0 && !strcmp(b, "own-size") && own_calls == 1);   /* Sketch's own state */
    skcfg_declare(&a);
    CHECK(declares == 1 && declared_n == 4 && !removed_declared); /* the form on 2.11: tablet_turn, fill_gap and the two paper settings (pen_area and cleanup_s are the X's); no stabiliser, pressure or tilt rows */
    a.device_setting_get = 0;                          /* the slot is there but empty: own settings, no crash */
    CHECK(skcfg_get(&a, "cleanup_s", b, sizeof(b)) > 0 && !strcmp(b, "own-cleanup_s"));
    a.device_setting_get = dev_get;
    a.size = APP_API_SLOT(58) - 1;                     /* 2.10 */
    int dc = dev_calls;
    CHECK(skcfg_get(&a, "cleanup_s", b, sizeof(b)) > 0 && !strcmp(b, "own-cleanup_s") && dev_calls == dc);
    skcfg_declare(&a);
    CHECK(declares == 2 && declared_n == 6 && !removed_declared); /* the own form: tablet_turn, fill_gap, paper, paper_sp, cleanup_s and pen_area */
    /* a refused form is returned and logged, never ignored (old firmware refused labels over 48 bytes) */
    refuse = APP_ENOENT;
    a.log = log_fn;
    a.size = sizeof(a);
    CHECK(skcfg_declare(&a) == APP_ENOENT && logged == 1);
    refuse = 0;
    CHECK(skcfg_declare(&a) == 0 && logged == 1);
    return CHECK_DONE("test_skcfg");
}
