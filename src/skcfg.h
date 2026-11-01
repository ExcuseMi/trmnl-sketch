/* Sketch's settings reads. On API 2.11 (api->size >= APP_API_SLOT(58)) pen_area and cleanup_s are the X's device
   settings (set on its phone page, Devices); on older firmware they stay Sketch's own settings. SPDX-License-Identifier: MIT */
#ifndef SKCFG_H
#define SKCFG_H
#include <app_api.h>
/* True for the keys the X's device settings own on 2.11 */
int skcfg_is_device_key(const char *key);
/* The value of `key`: length, or < 0 (none) */
int skcfg_get(const app_api_t *api, const char *key, char *buf, int cap);
/* Sketch's own settings form: pen_area and cleanup_s before API 2.11, none after (nothing else is left in it) */
void skcfg_declare(const app_api_t *api);
#endif
