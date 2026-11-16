CC := clang

CFLAGS := -g -O0 -Wall -Wextra -I.
LFLAGS := -lm

NCURSES := $(shell pkg-config --libs --cflags ncurses)

fields: libfields.so
	$(CC) main.c $(CFLAGS) $(LFLAGS) $(NCURSES) -o main

libfields.so:
	$(CC) fields.c $(CFLAGS) $(LFLAGS) -Wl,--shared -Wl,--no-pie -o libfields.so
