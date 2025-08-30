CC = gcc
CFLAGS = -I/opt/homebrew/include
LIBS = -L/opt/homebrew/lib -lraylib

all: main

main: main.c
	$(CC) main.c -o main $(CFLAGS) $(LIBS)

clean:
	rm -f main

.PHONY: build and start
start: main
	./main