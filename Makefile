CC = gcc
CFLAGS = -I/opt/homebrew/include
LIBS = -L/opt/homebrew/lib -lraylib

all: main

main: main.c input_config.c
	$(CC) main.c input_config.c -o main $(CFLAGS) $(LIBS)

clean:
	rm -f main

.PHONY: build and start
start: main
	./main
