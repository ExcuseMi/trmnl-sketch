# trmnl-sketch

Sketch for the TRMNL X with a USB pen tablet (XP-Pen Deco 01 V2; firmware API 2.7, `pen_enable`). Pressure sets the line width, the page autosaves as a PNG.

| Control | Does |
|---|---|
| pen | the brush picked, width or grain from pressure; the pen's button held: eraser (on the eraser: ink); hovering shows the brush's cursor at its size |
| tablet buttons 1..8 | undo, redo, eraser on/off, toolbar on/off (the same button hides it), clear, new page, 7 and 8 the page menu; no page stepping: the gallery is the way between pages |
| toolbar | a strip along the top edge (the bottom when the touch bar is on top): 16 items, each a 64 px glyph with a short label (the brushes as the tool they are: pen, paintbrush, pencil, chisel marker, nib, spray can, eraser block; shade swatches; dots of the real sizes; curved arrows; trash can), groups divided by a line; a pen tap or mouse click picks, never inks; the pick is shown inverted; a toast names it. Showing and hiding refresh only the strip (clean); the page under it comes back exactly. Drawing outside it works as usual |
| brushes | fineliner (even line, width by pressure), brush pen (swells from nothing, tapers out), pencil (page-fixed grain, lighter at a light touch), marker (broad flat square), calligraphy (45 degree nib: width depends on the direction), spray (dots, denser with pressure), eraser (square). Cursors: ring, drop, pencil tip, square, slanted bar, dotted ring, hollow square |
| shades | black, dark gray, light gray: 4 x 4 ordered dither fixed to the page (the same dots wherever and whenever drawn), only blackens; the eraser is solid |
| pills on the page | "Tools" (the toolbar on/off) and "Menu" (the page menu) in the corner away from the toolbar: a pen tap, never ink. Everything is reachable with the pen alone, and with the tablet buttons alone (tests walk both) |
| touch bar | only the middle tap (the page menu) as an extra way in; the firmware's long press leaves the app |
| page menu | for the page on screen, TRMNL list style with its framed thumbnail: Print, Send to phone, Settings, New page, Gallery, Back, Exit Sketch. Back, the menu button again and tablet 5 return to the same drawing (pixels, tool state, toolbar, undo). Exit Sketch saves and returns from the app as a long press does; only a failed save asks first |
| gallery | from the page menu: thumbnails of all pages, newest first, "New page" tile first; Up / Select / Down and Back pills (the pen taps them too); a tile opens that drawing at once; Back returns to the page you came from. A page file that does not decode is a "Damaged" tile (never deleted, never opened for drawing; its list: Send to phone, Back, so the file can be rescued); at start a damaged current page is skipped (the newest page that decodes, else a new page numbered past it) |
| Print | confirm screen, then the page file goes through the Print app's job code (`../trmnl-print/src/printjob.c`, one source). The printer and its format come only from Print's `printer.cfg` (`pj_default_printer`); none yet: "No printer yet: set one up in Print" with a "Set up printer" pill (tablet button 6) that starts Print's setup (`app_launch("trmnl-print", "setup:trmnl-sketch")`) and ends Sketch; Print starts it again with an empty argument |
| Send to phone | the files profile of the X's phone page lists `sketch/sk0001.png` ...; download them from a phone |

| Part | Where |
|---|---|
| pen width, tablet to page (fill or keep, four positions) | `src/penmap.c` |
| strokes, interpolation, discs, shades | `src/canvas.c` |
| brush table, stamps, cursor shapes | `src/brush.c` (a stroke is a log of points; the brush gives the byte of each, replay is a pure function of it) |
| toolbar layout, hit test, drawing; the glyphs | `src/toolbar.c`, `src/tbicons.c` |
| the flash and the clean-up rules | `src/skflash.c` |
| the Tools and Menu pills | `src/skpills.c` |
| the app icon (64 x 64, a pencil drawing a curve) | `tools/icon.py` writes `apps/trmnl-sketch/icon.h` |
| undo/redo/clear over a bounded stroke log and a base image | `src/hist.c` |
| page files: temp + rename, bad file moved to `-bad.png`, list | `src/pages.c`, `src/pngio.c` (1-bit PNG, no library) |
| rectangles pushed to the panel (merged, at most 6, byte aligned, every 40 ms) | `src/rects.c`, `src/sketch.c` (`sk_flush`) |
| controller (all decisions), screens | `src/sketch.c`, `src/skui.c` |
| app glue | `apps/trmnl-sketch/main.c` (+ `unity.c`, see below) |

Files in the app folder: `sketch/skNNNN.png` pages; `skNNNN-cleared.png` (the page as it was before the last clear), `skNNNN-bad.png` (a page file that could not be read, kept). Settings: `cur` (page), `size`, `brush`, `shade`; on the phone page (Settings in the page menu): `pen_area` (`fill` default: every tablet edge is a screen edge; `keep`: no stretching), `cleanup_s` (idle seconds before the whole screen is flashed clean, default 5, 0 = off).

Clean-up: every flash is `sk_flash` (`src/skflash.c`): the area inverted with a fast partial push, then as it is with a full refresh. Rule a: a toast, the start legend or the toolbar leaving the drawing page gives one whole-page flash once the tip is up. Rule b: idle for `cleanup_s` with a partial refresh since the last full one gives one whole-screen flash (page, gallery, menus). At most one blank page exists (the one on screen, never on disk; a blank file is removed at the start and when its page is left; a file that does not decode stays).

Limits: the page lives in the panel's frame, so turning the X turns the sketch with it; the tablet keeps its upright frame in every position (with `pen_area` keep the page is covered: a landscape tablet on a portrait page uses its middle part). Not in: colours, layers, shapes, text, zoom.

`make test` (host, ASan/UBSan), `make shots`, `make elf` (firmware fork's build with its import check). The linker of the toolchain aborts on some output paths for this app; `make elf` tries several and `apps/trmnl-sketch/unity.c` builds the sources as one object.
