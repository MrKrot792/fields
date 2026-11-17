#include <stdlib.h>
#include <string.h>

#include "fields.h"

#define WORLD_SIZE 256

void world_alloc(world_t* w) {
  w->back  = calloc(WORLD_SIZE, sizeof(Cell));
  w->front = calloc(WORLD_SIZE, sizeof(Cell));
  w->size  = WORLD_SIZE;
}

void world_init(world_t* w) {
  for (size_t i = 0; i < w->size; i++) {
    w->back[i] = cell_make(0);
  }
  
  w->back[WORLD_SIZE/2] = cell_make(1);
  memcpy(w->front, w->back, WORLD_SIZE * sizeof(Cell));
}

void world_free(world_t* w) {
  free(w->back);
  free(w->front);
}
