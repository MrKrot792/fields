CC := clang

CFLAGS := -g -O0 -Wall -Wextra -I.
LFLAGS := -lm

NCURSES := $(shell pkg-config --libs --cflags ncursesw)

fields: main.c libfields.so
	$(CC) main.c $(CFLAGS) $(LFLAGS) $(NCURSES) -o fields

libfields.so: fields.c
	$(CC) fields.c $(CFLAGS) $(LFLAGS) -shared -fPIC -o libfields.so
