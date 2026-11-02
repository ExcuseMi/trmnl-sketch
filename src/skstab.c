/* SPDX-License-Identifier: MIT */
#include "skstab.h"
#include "fmath.h"

#define TE 0.005f /* seconds between events */
#define BETA 0.03f /* Hz of cutoff per px/s of speed: a fast pen leaves little lag */

static float lowpass(float fc)
{
    float a = 2 * FM_PI * fc * TE;
    return a / (1 + a);
}

float stab_alpha(int level, float speed)
{
    if (level < 1) return 1;
    if (level > SK_SMOOTH_MAX) level = SK_SMOOTH_MAX;
    float fc = 24.0f / (float)level + BETA * speed / TE;
    return lowpass(fc);
}

void stab_begin(stab_t *f, int level, int x, int y)
{
    f->x = f->rx = (float)x, f->y = f->ry = (float)y, f->vx = f->vy = 0;
    f->level = level;
}

void stab_push(stab_t *f, int x, int y, int *ox, int *oy)
{
    if (f->level < 1) return *ox = x, *oy = y, (void)0;
    float ad = lowpass(3.0f); /* the velocity estimate: slow, so a shaking hand does not read as a fast one */
    f->vx += ad * (((float)x - f->rx) - f->vx);
    f->vy += ad * (((float)y - f->ry) - f->vy);
    f->rx = (float)x, f->ry = (float)y;
    float a = stab_alpha(f->level, fm_sqrt(f->vx * f->vx + f->vy * f->vy));
    f->x += a * ((float)x - f->x);
    f->y += a * ((float)y - f->y);
    *ox = fm_round(f->x), *oy = fm_round(f->y);
}
