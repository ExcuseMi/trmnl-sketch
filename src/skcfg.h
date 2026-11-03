/* Sketch's settings reads. On API 2.11 (api->size >= APP_API_SLOT(58)) pen_area and cleanup_s are the X's device
   settings (also on the X's phone page, Devices; Sketch's Settings screen sets them too); on older firmware they stay Sketch's own settings. SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 */
#ifndef SKCFG_H
#define SKCFG_H
#include <app_api.h>
/* True for the keys the X's device settings own on 2.11 */
int skcfg_is_device_key(const char *key);
/* The value of `key`: length, or < 0 (none) */
int skcfg_get(const app_api_t *api, const char *key, char *buf, int cap);
/* Stores `key`: the X's device setting for pen_area and cleanup_s on API 2.11, else the app's own store. 0 or an error */
int skcfg_set(const app_api_t *api, const char *key, const char *val, int n);
/* Sketch has no phone-page form (its settings are on its own screen): this declares nothing and clears one an older build left */
int skcfg_declare(const app_api_t *api);
#endif
