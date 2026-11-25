CC := clang

CFLAGS := -g -O0 -Wall -Wextra -I.
LFLAGS := -lm

fields: main.c libfields.so
	$(CC) main.c $(CFLAGS) $(LFLAGS) -o fields

libfields.so: fields.c
	$(CC) fields.c $(CFLAGS) $(LFLAGS) -shared -fPIC -o libfields.so
