/* The paper under the ink: Blank, Grid, Lines, Dots. Light 1-bit patterns (dotted lines, small dots) that are not part of
   the drawing: a per-page attribute, drawn under the ink on the screen, in the thumbnail and in the printed copy, never in
   the page bitmap (so the eraser cannot remove it and the bucket sees white). SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 */
#ifndef PAPER_H
#define PAPER_H
#include "canvas.h"

enum { PP_BLANK, PP_GRID, PP_LINES, PP_DOTS, PP_N };
#define PP_SP_MIN 16
#define PP_SP_MAX 200
#define PP_SP_DEF 48

const char *pp_name(int kind);
int pp_from_name(const char *s, int n); /* "grid" ... (case blind); PP_BLANK when unknown */
int pp_spacing(int sp);                 /* clamped to PP_SP_MIN..PP_SP_MAX (0 or bad: PP_SP_DEF) */
int pp_black(int kind, int sp, int x, int y); /* the paper is black at this page pixel */
/* Makes the paper's black pixels black in the whole bytes covering box b (clipped), over what is there */
void pp_overlay(canvas_t *c, const box_t *b, int kind, int sp);
#endif
