CC = gcc
CFLAGS = -I/opt/homebrew/include
LIBS = -L/opt/homebrew/lib -lraylib \
       -framework OpenGL -framework Cocoa -framework IOKit -framework CoreAudio -framework CoreVideo

all: main

main: main.c
	$(CC) main.c -o main $(CFLAGS) $(LIBS)

clean:
	rm -f main