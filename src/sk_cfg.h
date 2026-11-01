/* Sketch: constants. SPDX-License-Identifier: MIT */
#ifndef SK_CFG_H
#define SK_CFG_H

#define SK_BATCH_MS 40       /* new ink is pushed to the panel at most this often */
#define SK_MAX_RECTS 6       /* rectangles per push (and pending) */
#define SK_AUTOSAVE_MS 1500  /* the page is written this long after the pen left it */
#define SK_HIST_STROKES 48   /* undo depth; older strokes are baked into the page's base image */
#define SK_HIST_POINTS 32768 /* points kept for the undo log (6 bytes each) */
#define SK_PAGES_MAX 9999
#define SK_DIR "sketch"      /* inside the app's folder; the Wi-Fi page lists its .png files */
#define SK_FILE_MAX (700 * 1024) /* the largest page file read (a 1872 x 1404 page is 329 KB raw) */
#define SK_SIZE_MIN 1  /* pen size = the diameter in px (the slider knob's too) */
#define SK_SIZE_MAX 48
#define SK_SIZE_DEF 9
#define SK_ERASE_DEF 27

#endif
