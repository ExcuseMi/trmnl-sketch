# trmnl-sketch

Sketch for the TRMNL X with a USB pen tablet (XP-Pen Deco 01 V2; firmware API 2.7, `pen_enable`). Pressure sets the line width, the page autosaves as a PNG.

| Control | Does |
|---|---|
| pen | the brush picked, width or grain from pressure; the pen's button held: eraser (on the eraser: ink); hovering shows the brush's cursor at its size |
| tablet buttons 1..8 | undo, redo, eraser on/off, toolbar on/off (the same button hides it), clear, new page, previous, next |
| toolbar | a strip along the top edge (the bottom when the touch bar is on top): 7 brushes, 3 shades, 3 sizes, undo, redo, clear; a pen tap or mouse click picks, never inks; the pick is shown inverted; a toast names it. Showing and hiding refresh only the strip (clean); the page under it comes back exactly. Drawing outside it works as usual |
| brushes | fineliner (even line, width by pressure), brush pen (swells from nothing, tapers out), pencil (page-fixed grain, lighter at a light touch), marker (broad flat square), calligraphy (45 degree nib: width depends on the direction), spray (dots, denser with pressure), eraser (square). Cursors: ring, drop, pencil tip, square, slanted bar, dotted ring, hollow square |
| shades | black, dark gray, light gray: 4 x 4 ordered dither fixed to the page (the same dots wherever and whenever drawn), only blackens; the eraser is solid |
| touch bar | tap left undo, tap right redo, tap middle the gallery; swipe left/right next/previous page; hold middle clear |
| gallery | thumbnails (16 grays) of all pages, newest first, "New page" tile first; touch bar like the X's menus (Up / Select / Down pills); pen or mouse tap opens a tile (no hover highlight: gray screens only refresh in full); a drawing's list: Open, Print, Send to phone (the files page), Settings (the phone's settings form: the printer's address), Back; the drawing's preview is framed like a tile |
| Print | confirm screen, then the page file goes through the Print app's job code (`../trmnl-print/src/printjob.c`, one source); printer address is Sketch's own `printer` setting (settings are per app), default 192.0.2.40 |
| Send to phone | the files profile of the X's phone page lists `sketch/sk0001.png` ...; download them from a phone |

| Part | Where |
|---|---|
| pen width, tablet to page (aspect kept, page covered, four positions) | `src/penmap.c` |
| strokes, interpolation, discs, shades | `src/canvas.c` |
| brush table, stamps, cursor shapes | `src/brush.c` (a stroke is a log of points; the brush gives the byte of each, replay is a pure function of it) |
| toolbar layout, hit test, drawing | `src/toolbar.c` |
| undo/redo/clear over a bounded stroke log and a base image | `src/hist.c` |
| page files: temp + rename, bad file moved to `-bad.png`, list | `src/pages.c`, `src/pngio.c` (1-bit PNG, no library) |
| rectangles pushed to the panel (merged, at most 6, byte aligned, every 40 ms) | `src/rects.c`, `src/sketch.c` (`sk_flush`) |
| controller (all decisions), screens | `src/sketch.c`, `src/skui.c` |
| app glue | `apps/trmnl-sketch/main.c` (+ `unity.c`, see below) |

Files in the app folder: `sketch/skNNNN.png` pages; `skNNNN-cleared.png` (the page as it was before the last clear), `skNNNN-bad.png` (a page file that could not be read, kept). Settings: `cur` (page), `size`, `brush`, `shade`, `printer` (edited on the phone: Settings in a drawing's list).

Limits: the page lives in the panel's frame, so turning the X turns the sketch with it; the tablet keeps its upright frame in every position and the page is covered (a landscape tablet on a portrait page uses its middle part). Not in: colours, layers, shapes, text, zoom.

`make test` (host, ASan/UBSan), `make shots`, `make elf` (firmware fork's build with its import check). The linker of the toolchain aborts on some output paths for this app; `make elf` tries several and `apps/trmnl-sketch/unity.c` builds the sources as one object.
