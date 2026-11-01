/* The menu screens in the look of the X's own menus (hint pills over the touch bar's zones, a list with an arrow,
   inverted selection), drawn in the user's frame into a 1-bit canvas that the gray screen is made from.
   SPDX-License-Identifier: MIT */
#ifndef SKMENU_H
#define SKMENU_H
#include <app_api.h>
#include "gallery.h"
#include "gfx.h"

enum { PILL_NONE, PILL_UP, PILL_DOWN };

/* One hint pill beside the zone `zone` of the touch bar: outlined, rounded, its label (and a triangle) inside */
void ui_pill(gfx_t *g, const app_touchbar_t *bar, int zone, const char *label, int icon);
/* The three pills of a menu: Up, Select (label given), Down */
void ui_menu_pills(gfx_t *g, const app_touchbar_t *bar, const char *select);
/* A list as the firmware's menus: a title, the items (bold) with an arrow before the selected one. The text column
   starts a third into the area `L`. Returns the x where the text starts and the first item's baseline. */
void ui_list(gfx_t *g, const gal_layout_t *L, const char *title, const char *const *items, int n, int sel, int *text_x, int *first_y);
/* The gallery: tile 0 is "New page", the others pages. nums[slot] is the page number (0 = the new-page tile) for the
   `count` tiles of this screen; sel the selected slot (-1 none); cur the page on the flash (marked). Draws the frames and
   captions; the thumbnails are blitted over the inside of each tile (gallery.h). */
void ui_gallery(gfx_t *g, const gal_layout_t *L, const int *nums, int count, int sel, int cur, int page, int pages, const char *note);
/* A message screen: a title and up to three lines in the middle of the area */
void ui_message(gfx_t *g, const gal_layout_t *L, const char *title, const char *l1, const char *l2, const char *l3);
/* One line of text at the foot of the area */
void ui_footer(gfx_t *g, const gal_layout_t *L, const char *line);
#endif
