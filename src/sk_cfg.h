/* Sketch: constants. SPDX-License-Identifier: MIT */
#ifndef SK_CFG_H
#define SK_CFG_H

#define SK_BATCH_MS 16       /* new ink is pushed to the panel at most this often (host/speed.c: 16 beats 40 by 4 to 11 ms p95, the panel is the bottleneck) */
#define SK_LONGPRESS_MS 800 /* tablet button 8 held this long: the hidden debug toast */
#define SK_MAX_RECTS 6       /* rectangles per push (and pending) */
#define SK_AUTOSAVE_MS 1500  /* the page is written this long after the pen left it */
#define SK_HIST_STROKES 48   /* undo depth; older strokes are baked into the page's base image */
#define SK_HIST_POINTS 32768 /* points kept for the undo log (6 bytes each) */
#define SK_HIST_DELTA_BYTES (1024 * 1024) /* the undo steps' changed pixels (hist.h, PSRAM; plus a 329 KB shadow page) */
#define SK_HIST_DELTA_MIN (64 * 1024)     /* the pool is halved down to this when memory is short, then left out */
#define SK_PAGES_MAX 9999
#define SK_DIR "sketch"      /* inside the app's folder; the Wi-Fi page lists its .png files */
#define SK_FILE_MAX (700 * 1024) /* the largest page file read (a 1872 x 1404 page is 329 KB raw) */
#define SK_SIZE_MIN 1  /* pen size = the diameter in px */
#define SK_SIZE_MAX 48
#define SK_MIN_SLEEP_MS 10 /* sk_wait_ms: the shortest sleep after a due time that pushed nothing */
#define SK_PRINT_ROWS 384 /* printer rows (600 dpi) a loop turn makes of a print job: about 120 ms on the X (make actions, no turn over 300 ms) */
#define SK_GHOST_SCREENS 12 /* the drawing page's idle clean-up only after fast ink over this many screens' worth of pixels (owner 2026-11-03, test_tbflash) */

#endif
