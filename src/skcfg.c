/* SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 */
#include "skcfg.h"
#include <string.h>

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

int skcfg_set(const app_api_t *api, const char *key, const char *val, int n)
{
    if (n < 0) return -1;
    if (has_dev(api) && api->device_setting_set && skcfg_is_device_key(key)) return api->device_setting_set(key, val, (size_t)n);
    return api->setting_set(key, val, (size_t)n);
}

/* Sketch's settings are on its own screen: nothing is declared to the phone page. An older build's form is cleared. */
int skcfg_declare(const app_api_t *api)
{
    if (api->size < APP_API_SLOT(47) || !api->settings_declare) return 0;
    return api->settings_declare(0, 0);
}
