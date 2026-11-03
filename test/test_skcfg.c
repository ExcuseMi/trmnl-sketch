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
static char set_own[16], set_dev[16];
static int dev_rc;
static int own_set(const char *k, const void *v, size_t n) { (void)v, (void)n; snprintf(set_own, sizeof(set_own), "%s", k); return 0; }
static int dev_set(const char *k, const char *v, size_t n) { (void)v, (void)n; snprintf(set_dev, sizeof(set_dev), "%s", k); return dev_rc; }
static int refuse;
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
    CHECK(declares == 1 && declared_n == 0 && !removed_declared); /* nothing is declared to the phone page: Sketch's settings are on its own screen */
    a.device_setting_get = 0;                          /* the slot is there but empty: own settings, no crash */
    CHECK(skcfg_get(&a, "cleanup_s", b, sizeof(b)) > 0 && !strcmp(b, "own-cleanup_s"));
    a.device_setting_get = dev_get;
    a.size = APP_API_SLOT(58) - 1;                     /* 2.10 */
    int dc = dev_calls;
    CHECK(skcfg_get(&a, "cleanup_s", b, sizeof(b)) > 0 && !strcmp(b, "own-cleanup_s") && dev_calls == dc);
    skcfg_declare(&a);
    CHECK(declares == 2 && declared_n == 0);
    /* writes: the X's device setting for its two keys on 2.11, Sketch's own store otherwise */
    a.setting_set = own_set, a.device_setting_set = dev_set;
    a.size = sizeof(a);
    CHECK(skcfg_set(&a, "cleanup_s", "30", 2) == 0 && !strcmp(set_dev, "cleanup_s") && !set_own[0]);
    CHECK(skcfg_set(&a, "pen_area", "keep", 4) == 0 && !strcmp(set_dev, "pen_area"));
    CHECK(skcfg_set(&a, "paper", "grid", 4) == 0 && !strcmp(set_own, "paper"));
    set_own[0] = set_dev[0] = 0;
    a.size = APP_API_SLOT(58) - 1;
    CHECK(skcfg_set(&a, "cleanup_s", "30", 2) == 0 && !strcmp(set_own, "cleanup_s") && !set_dev[0]);
    a.size = sizeof(a);
    dev_rc = APP_EIO;
    CHECK(skcfg_set(&a, "cleanup_s", "30", 2) == APP_EIO); /* a refused write is returned, never hidden */
    return CHECK_DONE("test_skcfg");
}
