/* Sketch's settings screen (page menu, Settings): one row per setting, stepped with < and >. A change applies at once and is
   saved with the app's setting store (the X's device settings for pen_area and cleanup_s on API 2.11). SPDX-License-Identifier: MIT */
#ifndef SKSETTINGS_H
#define SKSETTINGS_H
#include "sketch.h"
#define SKS_N 6 /* settings rows (the list has Back on top, so SKS_N + 1 rows) */
/* Row i (0..SKS_N-1): its name and its value now, as shown */
void sks_row(const sketch_t *s, int i, char name[32], char val[16]);
/* Steps row i by dir (+1 or -1, wrapping at the ends), applies it and saves it. 0, or < 0 when the save failed (the value
   stays applied until Sketch closes) */
int sks_step(sketch_t *s, int i, int dir);
#endif
