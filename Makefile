CC = gcc

# raylib submodule paths
RAYLIB_DIR ?= raylib
RAYLIB_SRC := $(RAYLIB_DIR)/src
RAYLIB_LIB := $(RAYLIB_SRC)/libraylib.a

# Host (native) build settings (use vendored raylib)
CFLAGS = -I$(RAYLIB_SRC) -std=c99 -Wall -Wextra -Wno-unused-parameter -O2 -DNDEBUG

# Platform-specific system libs for raylib
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    # Link both Metal and OpenGL frameworks to support either backend
    PLATFORM_LIBS = -framework Cocoa -framework IOKit -framework CoreVideo -framework CoreAudio -framework AudioToolbox -framework Metal -framework MetalKit -framework OpenGL
else
    PLATFORM_LIBS = -lm -lpthread -ldl -lrt -lX11 -lXrandr -lXi -lXxf86vm -lXinerama -lXcursor
endif

LIBS = $(RAYLIB_LIB) $(PLATFORM_LIBS)

# Web (WASM) build settings — requires Emscripten and a raylib web build
EMCC ?= emcc
# Override these from the CLI to point at your raylib build for web
# Example: make web RAYLIB_WEB_LIB_DIR=raylib/src RAYLIB_INC=raylib/src
RAYLIB_INC ?= $(RAYLIB_SRC)
RAYLIB_WEB_LIB_DIR ?= $(RAYLIB_SRC)

WEB_CFLAGS = -I$(RAYLIB_INC) -DPLATFORM_WEB -std=gnu99 -Wall -Wextra -Wno-unused-parameter -Os -s USE_GLFW=3
WEB_LDFLAGS = -s WASM=1 -s MIN_WEBGL_VERSION=2 -s MAX_WEBGL_VERSION=2 -s USE_WEBGL2=1 -s ALLOW_MEMORY_GROWTH=1 -s STACK_SIZE=262144 -s USE_GLFW=3 -s ASYNCIFY -s EXPORTED_RUNTIME_METHODS=['requestFullscreen']
RAYLIB_WEB_LIB := $(RAYLIB_SRC)/libraylib.web.a
WEB_LIBS = $(RAYLIB_WEB_LIB)
WEB_OUTPUT_DIR = web
WEB_SHELL ?= web_shell.html

SRCS = app.c game.c level.c ui.c audio.c render.c editor.c menu.c input_config.c fps_meter.c settings.c autotiler.c physics.c player.c enemy.c
OBJS = $(SRCS:.c=.o)

all: main

main: $(OBJS) $(RAYLIB_LIB)
	$(CC) $(OBJS) -o $@ $(CFLAGS) $(LIBS)

# Rebuild objects when config.h changes (simple dep tracking)
$(OBJS): config.h

# Build vendored raylib for desktop
$(RAYLIB_LIB):
	$(MAKE) -C $(RAYLIB_SRC) clean
	$(MAKE) -C $(RAYLIB_SRC) PLATFORM=PLATFORM_DESKTOP

# WebAssembly build (outputs web/index.html + .wasm + .js)
WEB_SRCS = $(SRCS)
WEB_OBJS = $(WEB_SRCS:.c=.web.o)

# Ensure web objects also rebuild on config.h changes
$(WEB_OBJS): config.h

%.web.o: %.c
	$(EMCC) $(WEB_CFLAGS) -c $< -o $@

web: $(WEB_OBJS)
	$(MAKE) -C $(RAYLIB_SRC) clean
	$(MAKE) -C $(RAYLIB_SRC) PLATFORM=PLATFORM_WEB
	@mkdir -p $(WEB_OUTPUT_DIR)
	$(EMCC) $(WEB_OBJS) -o $(WEB_OUTPUT_DIR)/index.html $(WEB_LDFLAGS) $(WEB_LIBS) --shell-file $(WEB_SHELL) \
		--preload-file assets@assets --preload-file levels@levels --preload-file config@config
	zip -r web.zip web

format:
	git ls-files '*.c' '*.h' | xargs -n 25 clang-format -i

clean:
	rm -f main $(OBJS) $(WEB_OBJS)
	rm -rf $(WEB_OUTPUT_DIR)

start: main
	./main

.PHONY: deploy-web
deploy-web: web
	# Publish the built web/ folder to GitHub Pages using gh-pages
	touch package.json && npx gh-pages --dist web && rm package.json
