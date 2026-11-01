# trmnl-sketch

Sketch for the TRMNL X with a USB pen tablet (XP-Pen Deco 01 V2; firmware API 2.7, `pen_enable`). Pressure sets the line width, the page autosaves as a PNG.

| Control | Does |
|---|---|
| pen | line width from pressure (thin, medium, thick sizes); the pen's button held: eraser; hovering shows a small ring |
| tablet buttons 1..8 | undo, redo, eraser on/off, size, clear, new page, previous, next |
| touch bar | tap left undo, tap right redo, tap middle the gallery; swipe left/right next/previous page; hold middle clear |
| gallery | thumbnails (16 grays) of all pages, newest first, "New page" tile first; touch bar like the X's menus (Up / Select / Down pills); pen or mouse tap opens a tile (no hover highlight: gray screens only refresh in full); a drawing's list: Open, Print, Send to phone, Back |
| Print | confirm screen, then the page file goes through the Print app's job code (`../trmnl-print/src/printjob.c`, one source); printer address is Sketch's own `printer` setting (settings are per app), default 192.0.2.40 |
| Send to phone | the files profile of the X's phone page lists `sketch/sk0001.png` ...; download them from a phone |

| Part | Where |
|---|---|
| pen width, tablet to page (aspect kept, page covered, four positions) | `src/penmap.c` |
| strokes, interpolation, discs | `src/canvas.c` |
| undo/redo/clear over a bounded stroke log and a base image | `src/hist.c` |
| page files: temp + rename, bad file moved to `-bad.png`, list | `src/pages.c`, `src/pngio.c` (1-bit PNG, no library) |
| rectangles pushed to the panel (merged, at most 6, byte aligned, every 40 ms) | `src/rects.c`, `src/sketch.c` (`sk_flush`) |
| controller (all decisions), screens | `src/sketch.c`, `src/skui.c` |
| app glue | `apps/trmnl-sketch/main.c` (+ `unity.c`, see below) |

Files in the app folder: `sketch/skNNNN.png` pages; `skNNNN-cleared.png` (the page as it was before the last clear), `skNNNN-bad.png` (a page file that could not be read, kept). Settings: `cur` (page), `size`.

Limits: the page lives in the panel's frame, so turning the X turns the sketch with it; the tablet keeps its upright frame in every position and the page is covered (a landscape tablet on a portrait page uses its middle part). Not in: colours, layers, shapes, text, zoom.

`make test` (host, ASan/UBSan), `make shots`, `make elf` (firmware fork's build with its import check). The linker of the toolchain aborts on some output paths for this app; `make elf` tries several and `apps/trmnl-sketch/unity.c` builds the sources as one object.
