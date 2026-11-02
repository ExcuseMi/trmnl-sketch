/* The stroke's points of one frame, thinned and stamped in one go (owner 2026-11-02: "draw whatever happened in the last
   moment, consistently"). The pen's reports come at 200 a second; the app takes all of them at once every SK_FRAME_MS,
   keeps the points the stroke needs (a point every thin_px along it, the first and the last always), stamps that
   polyline and marks it for one push. When a frame's stamping overran its budget the soft Brush's stamp step grows
   (brush_step_scale) so the next frame fits, instead of the app falling behind the pen (fw-lag28 pulse lines: the app at
   60 to 84 % of its core, thousands of reports dropped in the firmware's queue, 10 s to see a change).
   SPDX-License-Identifier: MIT */
#ifndef SKFRAME_H
#define SKFRAME_H
#include <stdint.h>

#define SK_FRAME_MS 30         /* the frame: reports are taken for this long after the first one, then stamped */
#define SK_FRAME_BUDGET_MS 12  /* stamping per frame on the X above this grows the Brush's step, under a third shrinks it */
#define SK_STEP_SCALE_MAX 8
#define SK_QUEUE_MAX 128       /* points a frame holds; a fuller one is stamped at once (640 ms of reports) */
#define SK_SCALE_EASE_FRAMES 32 /* cheap frames in a row (about a second) before the scale comes down one step */
#define SK_BAKE_PTS 8          /* points of an old stroke baked into the history's base per frame (hist.h) */
#define SK_BAKE_IDLE_PTS 32    /* and per idle tick, the pen up */

typedef struct { int16_t x, y; int p, tilt; } sk_qpt_t;

typedef struct {
    sk_qpt_t q[SK_QUEUE_MAX];
    int n;
    int scale;      /* the Brush's stamp step multiplier for the next stroke, 1..SK_STEP_SCALE_MAX (a stroke keeps its own) */
    int cheap;      /* frames in a row under a third of the budget */
    uint32_t cost_ms; /* the last frame's stamping time */
} sk_frame_t;

/* The indices of `in` to keep: the first, then every point at least thin_px (Chebyshev) from the last kept one, and
   the last. Returns the count; thin_px < 1 keeps every point. */
int sk_thin(const sk_qpt_t *in, int n, int thin_px, int *keep);

/* The scale after a frame that took cost_ms: doubled over the budget (at once: the next stroke must keep up), one
   step down after SK_SCALE_EASE_FRAMES cheap frames in a row (`cheap` counts them) */
int sk_scale_next(int scale, uint32_t cost_ms, int *cheap);

#endif
