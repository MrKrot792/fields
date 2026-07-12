#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <ncurses.h>

#define WORLD_SIZE 512

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

typedef struct {
  int32_t x;
  float y;
} Camera;

void world_draw(const Cell* world, Camera camera) {
  erase();
  for (size_t x = 0; x < (size_t)(COLS < WORLD_SIZE - 1 ? WORLD_SIZE - 1 : COLS); x++) {
    for (size_t y = 0; y < (size_t)LINES; y++) {

      if (x + camera.x > WORLD_SIZE) continue;
      float threshold = -((float)y / (float)LINES * 2.0 - 1.0);
      threshold *= camera.y;

      if (world[x + camera.x].position >= threshold && threshold > 0)
	mvaddch(y, x, '#');

      if (world[x + camera.x].position <= threshold && threshold < 0)
	mvaddch(y, x, '#');
#ifdef DRAW_VELOCITY
      if (world[x + camera.x].velocity >= threshold && threshold > 0)
	mvaddch(y, x, '@');

      if (world[x + camera.x].velocity <= threshold && threshold < 0)
	mvaddch(y, x, '@');
#endif
      if (world[x + camera.x].position >= camera.y)
	mvaddch(0, x, '+');

      if (world[x + camera.x].position <= -camera.y)
	mvaddch(COLS-1, x, '+');
      
      if (y == (size_t)LINES/2)
	mvaddch(y, x, '$');
    }
  }
  refresh();
}

void events(bool* paused, bool* running, Camera* camera, const Cell* world) {
  do {
    switch (getch()) {
    case 'h':
      camera->x -= 1;
      break;
    case 'l':
      camera->x += 1;
      break;
    case KEY_LEFT:
      camera->x -= 10;
      break;
    case KEY_RIGHT:
      camera->x += 10;
      break;
    case 'j':
      camera->y *= 0.8;
      break;
    case 'k':
      camera->y *= 1.2;
      break;
    case 'q':
      *running = false;
      break;
    case ' ':
      nodelay(stdscr, *paused);
      *paused = !(*paused);
      break;
    }
    if (*paused) world_draw(world, *camera);
    if (!*running) break;
  } while(*paused);
}

int main(void) {
  Cell* world_front = malloc(sizeof(Cell) * WORLD_SIZE);
  Cell* world_back  = malloc(sizeof(Cell) * WORLD_SIZE);
  
  world_back[WORLD_SIZE/2] = (Cell){0.0, 1.0};
  world_back[WORLD_SIZE/2+1] = (Cell){0.0, -1.0};
  world_back[WORLD_SIZE/2-1] = (Cell){0.0, -1.0};
  memcpy(world_front, world_back, sizeof(Cell) * WORLD_SIZE);

  Camera camera = (Camera){WORLD_SIZE/2, 1.0};
  
  bool running = true;
  bool paused  = false;

  initscr();
  noecho();
  nodelay(stdscr, true);
  keypad(stdscr, true);

  camera.x -= COLS/2;

  while (running) {
    events(&paused, &running, &camera, world_front);
    world_simulate(world_back, world_front, 0.01);
    world_draw(world_front, camera);
    
    Cell* t = world_back;
    world_back = world_front;
    world_front = t;
  }

  endwin();
  free(world_front);
  free(world_back);
}
