/* SPDX-License-Identifier: MIT */
#include "skcfg.h"
#include <string.h>

/* Not a device setting of the X (its keys are fixed): always Sketch's own */
static const app_setting_decl_t decl_turn = { "tablet_turn", "Tablet in portrait (auto: used turned a quarter, long side up and down; off: as it lies)", "auto|off", 0, 0, APP_SETTING_CHOICE };
static const app_setting_decl_t decl[] = {
    { "cleanup_s", "Clean the screen after this many idle seconds (0: never)", 0, 0, 600, APP_SETTING_NUMBER },
    { "pen_area", "Tablet area on the page (fill: every tablet edge is a screen edge; keep: no stretching)", "fill|keep", 0, 0, APP_SETTING_CHOICE },
};

static int has_dev(const app_api_t *api) { return api->size >= APP_API_SLOT(58) && api->device_setting_get; }

int skcfg_is_device_key(const char *key) { return !strcmp(key, "pen_area") || !strcmp(key, "cleanup_s"); }

int skcfg_get(const app_api_t *api, const char *key, char *buf, int cap)
{
    if (cap <= 0) return -1;
    if (has_dev(api) && skcfg_is_device_key(key)) {
        int n = api->device_setting_get(key, buf, (size_t)cap);
        return n < 0 ? -1 : n;
    }
    return api->setting_get(key, buf, (size_t)cap);
}

void skcfg_declare(const app_api_t *api)
{
    if (api->size < APP_API_SLOT(47)) return;
    app_setting_decl_t all[3];
    uint32_t n = 0;
    all[n++] = decl_turn;
    if (!has_dev(api)) all[n++] = decl[0], all[n++] = decl[1];
    api->settings_declare(all, n);
}
