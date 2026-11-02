/* What the glue hands the app API's screen_refresh for one sk_flush result. On the X a partial refresh takes only the
   rows (app_api_table.cpp: bbep.partialUpdate(true, y0, y1), x and w are ignored) and the library scans the whole panel
   pp + 1 times whatever the rows (FastEPD bbepPartialUpdate: every pass writes all 1404 rows, then one neutral scan),
   so a call costs about 200 ms however small (the model: host/speed.c). All partial rectangles of one push therefore go as one
   call over the rows they span; flash halves and the whole screen go as they are. SPDX-License-Identifier: MIT */
#ifndef SKPUSH_H
#define SKPUSH_H
#include <app_api.h>
#include "rects.h"
#include "sk_cfg.h"

typedef struct {
    app_rect_t r;     /* the area (whole: unused) */
    app_refresh_t how;
    int whole;        /* screen_refresh(NULL, how): the whole screen */
} sk_call_t;

enum { SKP_ONE_CALL = 0, SKP_PER_RECT = 1 }; /* SKP_PER_RECT: one call per rectangle (before 2026-11-02; the speed run compares) */
#define SKP_MODE SKP_ONE_CALL /* what the app does (main.c), and the speed run's default */

/* n: what sk_flush returned (SK_FULL, 0 or a count of rectangles in r). Fills out (SK_MAX_RECTS at most); returns the
   number of calls */
int skp_calls(int n, const rect_t *r, int mode, int ink, sk_call_t *out);
/* ink: the firmware has APP_REFRESH_INK (api->size >= APP_API_SLOT(SKP_INK_SLOT), API 2.14): live partial rows go as
   fast ink (returns at once, 2 passes); flash halves and the whole screen stay APP_REFRESH_FULL (the clean flash) */
#define SKP_INK_SLOT 61
#endif
