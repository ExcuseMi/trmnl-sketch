# Host build: tests with sanitizers, the shots tool, the baked fonts. The app ELF is built by the firmware fork's
# apps/build.sh from apps/trmnl-sketch (run `make fonts` first).
CC ?= gcc
FW ?= $(HOME)/workspace/trmnl-fw-gb
PY ?= $(HOME)/workspace/trmnl-fpv/.venv/bin/python
CFLAGS = -std=c11 -O1 -g -Wall -Wextra -Werror -Wshadow -Isrc -Ihost -Igen -I$(FW)/include -fsanitize=address,undefined -fno-sanitize-recover=all
CORE = src/canvas.c src/penmap.c src/hist.c src/pngio.c src/pages.c src/rects.c src/skui.c src/sketch.c src/font.c src/gfx.c gen/fonts_gen.c
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
test: $(TESTS)
	@for t in $(TESTS); do ./$$t || exit 1; done
build/shots: host/shots.c host/png.c $(CORE) $(HOST) test/util.h $(wildcard src/*.h host/*.h)
	@mkdir -p build
	$(CC) $(CFLAGS) host/shots.c host/png.c $(CORE) $(HOST) -o $@ -lz -lm -lstdc++
shots: build/shots
	@mkdir -p shots
	./build/shots shots
clean:
	rm -rf build shots gen
.PHONY: all fonts test shots clean
