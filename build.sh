#!/bin/sh
set -xe
ncurses=$(pkg-config --libs --cflags ncurses)
gcc main.c -Wall -Wextra $ncurses -lm -O2 -o main
