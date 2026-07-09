#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <ncurses.h>

#define WORLD_SIZE 128

typedef struct {
  float velocity;
  float position;
} Cell;

Cell cell_get(const Cell* cells, int32_t i) {
  if (i < 0 || i > WORLD_SIZE - 1) return (Cell){0};
  return cells[i];
}

void cell_set(Cell cell, Cell* cells, int32_t i) {
  if (i < 0 || i > WORLD_SIZE - 1) return;
  cells[i] = cell;
}

void world_simulate(const Cell *world, Cell *output, float dt) {
  const float wave_speed = 1.0f;
  const float damping = 0.02f;

  for (size_t i = 0; i < WORLD_SIZE; ++i) {
    size_t left  = (i + WORLD_SIZE - 1) % WORLD_SIZE;
    size_t right = (i + 1) % WORLD_SIZE;

    float center = world[i].position;
    float laplacian =
      world[left].position - 2.0f * center + world[right].position;

    float acceleration = wave_speed * wave_speed * laplacian;

    output[i].velocity =
      (world[i].velocity + acceleration * dt) * (1.0f - damping * dt);

    output[i].position =
      center + output[i].velocity * dt;
  }
}

void world_draw(Cell* world, int32_t cam, float max) {
  for (size_t x = 0; x < (size_t)COLS; x++) {
    for (size_t y = 0; y < (size_t)LINES; y++) {
      
      float threshold = -((float)y / (float)LINES * 2.0 - 1.0);
      threshold *= max;

      if (world[x + cam].position >= threshold && threshold > 0)
	mvaddch(y, x, '#');

      if (world[x + cam].position <= threshold && threshold < 0)
	mvaddch(y, x, '#');
      
      if (y == (size_t)LINES/2)
	mvaddch(y, x, '$');
    }
  }
}

int main(void) {
  Cell* world_front = malloc(sizeof(Cell) * WORLD_SIZE);
  Cell* world_back  = malloc(sizeof(Cell) * WORLD_SIZE);
  
  world_back[WORLD_SIZE/2] = (Cell){0.0, 1.0};
  memcpy(world_front, world_back, sizeof(Cell) * WORLD_SIZE);

  int32_t offset = WORLD_SIZE/2;
  float scope = 1.0;

  bool running = true;
  bool events_present = true;

  initscr();
  noecho();

  offset -= COLS/2;

  while (running) {
    world_simulate(world_back, world_front, 0.2);
    erase();
    world_draw(world_front, offset, scope);
    refresh();
    
    Cell* t = world_back;
    world_back = world_front;
    world_front = t;

    while (events_present) {
      int ch = getch();

      switch (ch) {
      case 'h':
	offset -= 1;
	break;
      case 'l':
	offset += 1;
	break;
      case 'j':
	scope *= 0.8;
	break;
      case 'k':
	scope *= 1.2;
	break;
      case 'q':
	running = false;
	break;
      default:
	events_present = false;
	break;
      }
      if (!running) break;
      
      erase();
      world_draw(world_front, offset, scope);
      refresh();
    }
    events_present = true;
  }

  endwin();
  free(world_front);
  free(world_back);
}
