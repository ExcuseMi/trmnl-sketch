/* The stabiliser: smooths the pen's points while drawing (a one-euro filter: heavy at a slow, shaky pen, nearly none at a
   fast one, so it does not lag). The pen's events are the clock (one per 5 ms, the tablet reports about 200 a second), so
   a stall of the app never changes the line. Pure: no clock, no libm. SPDX-License-Identifier: MIT */
#ifndef SKSTAB_H
#define SKSTAB_H

#define SK_SMOOTH_MAX 10 /* the filter level 0 off .. 10 */
#define SK_SMOOTH_DEF 3  /* what Sketch uses (no setting, no toolbar cell) */

typedef struct {
    float x, y, rx, ry, vx, vy; /* the filtered point, the last raw one, the filtered velocity (px per event) */
    int level;
} stab_t;

/* A stroke starts at (x, y) exactly */
void stab_begin(stab_t *f, int level, int x, int y);
/* The next raw point in, the smoothed point out (level 0: the same point) */
void stab_push(stab_t *f, int x, int y, int *ox, int *oy);
/* The weight of the newest point at a speed (px per event); also what the tests read for the lag bound */
float stab_alpha(int level, float speed);
#endif
