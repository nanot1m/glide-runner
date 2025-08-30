CC = gcc
CFLAGS = -I/opt/homebrew/include -std=c99 -Wall -Wextra -Wno-unused-parameter
LIBS = -L/opt/homebrew/lib -lraylib

SRCS = app.c game.c level.c ui.c audio.c render.c editor.c menu.c input_config.c
OBJS = $(SRCS:.c=.o)

all: main

main: $(OBJS)
	$(CC) $(OBJS) -o $@ $(CFLAGS) $(LIBS)

clean:
	rm -f main $(OBJS)

.PHONY: build start clean
start: main
	./main
