#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <ncurses.h>

#define WORLD_SIZE 40

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

float dif2_num(float nx, float x, float px, float dx) {
  return (nx - 2.0*x + px) / (dx * dx);
}

float dif2_cell(Cell nc, Cell c, Cell pc) {
  return dif2_num(nc.position, c.position, pc.position, 1.0);
}

float dif2(const Cell* cells, int32_t x) {
  return dif2_num(cell_get(cells, x-1).position,
		  cell_get(cells, x).position,
		  cell_get(cells, x+1).position, 1);
}

void world_simulate(const Cell* world, Cell* output, float dt) {
  for (size_t i = 0; i < WORLD_SIZE; ++i) {
    Cell l = cell_get(world, (int32_t)i - 1);
    Cell r  = cell_get(world, (int32_t)i + 1);
    Cell c = cell_get(world, (int32_t)i);

    float a = dif2_cell(l, c, r);
    output[i] = c;
    output[i].velocity += a * dt;
    output[i].position += output[i].velocity * dt;
  }
}

//#define DRAW_VELOCITY

void world_draw(Cell* world, int32_t cam, float max) {
  for (size_t x = 0; x < (size_t)(COLS < WORLD_SIZE ? WORLD_SIZE : COLS); x++) {
    for (size_t y = 0; y < (size_t)LINES; y++) {

      if (x + cam > WORLD_SIZE) continue;
      float threshold = -((float)y / (float)LINES * 2.0 - 1.0);
      threshold *= max;

      if (world[x + cam].position >= threshold && threshold > 0)
	mvaddch(y, x, '#');

      if (world[x + cam].position <= threshold && threshold < 0)
	mvaddch(y, x, '#');
#ifdef DRAW_VELOCITY
      if (world[x + cam].velocity >= threshold && threshold > 0)
	mvaddch(y, x, '@');

      if (world[x + cam].velocity <= threshold && threshold < 0)
	mvaddch(y, x, '@');
#endif
      if (world[x + cam].position >= max)
	mvaddch(0, x, '+');

      if (world[x + cam].position <= -max)
	mvaddch(COLS-1, x, '+');
      
      if (y == (size_t)LINES/2)
	mvaddch(y, x, '$');
    }
  }
}

void world_write(const Cell* world) {
  FILE* fd = fopen("world.txt", "w");
  fprintf(fd, "#velocity position\n");
  for (size_t i = 0; i < WORLD_SIZE; i++) {
    fprintf(fd, "%f %f\n", world[i].velocity, world[i].position);
  }
  fflush(fd);
  fclose(fd);
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
    world_simulate(world_back, world_front, 0.01);
    erase();
    world_draw(world_front, offset, scope);
    refresh();
    world_write(world_front);
    
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
