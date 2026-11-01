# Host build: tests with sanitizers, the shots tool, the baked fonts. The app ELF is built by the firmware fork's
# apps/build.sh from apps/trmnl-sketch (run `make fonts` first).
CC ?= gcc
FW ?= $(HOME)/workspace/trmnl-fw-files
PRINT ?= $(HOME)/workspace/trmnl-print/src
PY ?= $(HOME)/workspace/trmnl-fpv/.venv/bin/python
CFLAGS = -Itest -std=c11 -O1 -g -Wall -Wextra -Werror -Wshadow -Isrc -I$(PRINT) -Ihost -Igen -I$(FW)/include -fsanitize=address,undefined -fno-sanitize-recover=all
CORE = src/canvas.c src/brush.c src/toolbar.c src/tbicons.c src/penmap.c src/hist.c src/pngio.c src/pages.c src/rects.c src/skpush.c src/skcfg.c src/sksize.c src/skflash.c src/fill.c src/skfill.c src/mirror.c src/skmirror.c src/skui.c src/skpills.c src/skmenu.c src/gallery.c src/skprint.c src/skgal.c $(PRINT)/pcl.c $(PRINT)/pwg.c $(PRINT)/ipp.c $(PRINT)/imgfit.c $(PRINT)/textlay.c $(PRINT)/printer.c $(PRINT)/printjob.c src/sketch.c src/font.c src/gfx.c gen/fonts_gen.c
HOST = host/memfs.c build/tbfw.o
TESTS = $(patsubst test/%.c,build/%,$(wildcard test/test_*.c))

all: test shots
fonts: gen/fonts_gen.c
gen/fonts_gen.c: tools/bake_fonts.py fonts/sizes.txt $(wildcard fonts/*.ttf)
	@mkdir -p gen
	$(PY) tools/bake_fonts.py $@
build/tbfw.o: host/tbfw.cpp $(FW)/lib/trmnl/src/touchbar_layout.cpp
	@mkdir -p build
	g++ -std=gnu++11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -I$(FW)/include -I$(FW)/lib/trmnl/include -I$(FW)/lib/trmnl/src -c $< -o $@
build/%: test/%.c $(CORE) $(HOST) test/check.h test/util.h $(wildcard src/*.h host/*.h)
	@mkdir -p build
	$(CC) $(CFLAGS) $< $(CORE) $(HOST) -o $@ -lm -lz -lstdc++
test: $(TESTS) speed
	@for t in $(TESTS); do ./$$t || exit 1; done
build/shots: host/shots.c host/shots_fill.c host/shots_mirror.c host/png.c $(CORE) $(HOST) test/util.h $(wildcard src/*.h host/*.h)
	@mkdir -p build
	$(CC) $(CFLAGS) host/shots.c host/shots_fill.c host/shots_mirror.c host/png.c $(CORE) $(HOST) -o $@ -lz -lm -lstdc++
shots: build/shots
	@mkdir -p shots
	./build/shots shots
clean:
	rm -rf build shots gen
.PHONY: all fonts test shots clean
# Drawing speed on the X, simulated (host/speed.c: the driver's timing, the model's evidence): fails when a pen stream's
# p95 latency is over test/speed.bounds + 20 %. `make speed-table` compares the variants.
build/speed: host/speed.c $(CORE) $(HOST) test/util.h $(wildcard src/*.h host/*.h)
	@mkdir -p build
	$(CC) $(CFLAGS) host/speed.c $(CORE) $(HOST) -o $@ -lm -lz -lstdc++
speed: build/speed
	./build/speed --check test/speed.bounds
speed-table: build/speed
	./build/speed
.PHONY: speed speed-table
# The app ELF with the firmware fork's import check (every libc call must be in the loader's table); the sources are
# listed in apps/trmnl-sketch/build.flags. apps/build.sh links one path for every app (its libgcc copy is the linker fix).
elf: gen/fonts_gen.c
	@mkdir -p build/elf; d=$${TMPDIR:-/tmp}/sketch-elf; rm -rf $$d; OUT=$$d $(FW)/apps/build.sh $(CURDIR)/apps/trmnl-sketch && cp $$d/trmnl-sketch.elf build/elf/ && rm -rf $$d
	@ls -l build/elf/trmnl-sketch.elf; sha256sum build/elf/trmnl-sketch.elf
.PHONY: elf
