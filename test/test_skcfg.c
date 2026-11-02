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
static int decl_fn(const app_setting_decl_t *d, uint32_t n)
{
    (void)d;
    declares++, declared_n = (int)n;
    return 0;
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
    CHECK(declares == 1 && declared_n == 7);           /* the form on 2.11: tablet_turn, fill_gap, smooth and the two paper settings (pen_area and cleanup_s are the X's) */
    a.device_setting_get = 0;                          /* the slot is there but empty: own settings, no crash */
    CHECK(skcfg_get(&a, "cleanup_s", b, sizeof(b)) > 0 && !strcmp(b, "own-cleanup_s"));
    a.device_setting_get = dev_get;
    a.size = APP_API_SLOT(58) - 1;                     /* 2.10 */
    int dc = dev_calls;
    CHECK(skcfg_get(&a, "cleanup_s", b, sizeof(b)) > 0 && !strcmp(b, "own-cleanup_s") && dev_calls == dc);
    skcfg_declare(&a);
    CHECK(declares == 2 && declared_n == 9);           /* the own form: tablet_turn, fill_gap, paper, paper_sp, smooth, cleanup_s and pen_area */
    return CHECK_DONE("test_skcfg");
}
