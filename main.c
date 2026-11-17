// TODO: Transition from ncurses to termbox2
#include <dlfcn.h>
#include <math.h>
#include <ncurses.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "fields.h"

// TODO: remove this altogether
#define WORLD_SIZE 256
#define FAST_MODE_SPEED 1
#define SPEED 0.1

Cell cell_get(const Cell* cells, int32_t i) {
  if (i < 0) return (Cell){0, 0, 1};
  if (i > WORLD_SIZE - 1) return (Cell){0, 0, 1};
  return cells[i];
}

void cell_set(Cell cell, Cell* cells, int32_t i) {
  if (i < 0 || i > WORLD_SIZE - 1) return;
  cells[i] = cell;
}

double dif2_num(double nx, double x, double px, double dx) {
  return (nx - 2.0*x + px) / (dx * dx);
}

double dif2_cell(Cell nc, Cell c, Cell pc) {
  return dif2_num(nc.position, c.position, pc.position, 1.0);
}

double dif2(const Cell* cells, int32_t x) {
  return dif2_num(cell_get(cells, x-1).position,
		  cell_get(cells, x).position,
		  cell_get(cells, x+1).position, 1);
}

typedef struct {
  double x;
  double y;
  double range_x; // same
  double range_y; // means from -y to y
} Camera;

typedef struct {
  ext_f world_alloc;
  ext_f world_init;
  ext_f world_free;
  void* lib;
  world_t world;
  bool running;
  bool paused;
  bool fast_mode;
  bool should_lerp;
  struct timespec start;
  struct timespec end;
  double dt;
  double sim_speed;
  Camera camera;
} state_t;

void world_simulate(const Cell* world, Cell* output, double dt) {
  for (size_t i = 0; i < WORLD_SIZE; ++i) {
    Cell l = cell_get(world, (int32_t)i - 1);
    Cell r = cell_get(world, (int32_t)i + 1);
    Cell c = cell_get(world, (int32_t)i);

    double a = dif2_cell(l, c, r) * pow(1.0 / c.mass, 2);
    output[i] = c;
    output[i].velocity += a * dt;
    output[i].position += output[i].velocity * dt;
  }
}

void step_simulation(Cell** back, Cell** front, double dt) {
  world_simulate(*back, *front, dt);

  Cell* t = *back;
  *back = *front;
  *front = t;
}

// haha funny numbers go brr
double smootherstep(double a, double b, double x) { 
 return a + (b - a) * (x * x * x * (x * (6.0 * x - 15.0) + 10.0));
}

double lerp(double a, double b, double x) {
  return a + (b - a) * x;
}

int velocity_gradient(double velocity) {
  double v = fabs(velocity);
  if (v > 0.5) {
    return '@';
  } else if (v > 0.4) {
    return '#';
  } else if (v > 0.3) {
    return '=';
  } else if (v > 0.2) {
    return '-';
  } else if (v > 0.1) {
    return '.';
  }

  return ' ';
}

void world_draw(const Cell* world, Camera camera, bool should_lerp) {
  double from_x = -camera.range_x + camera.x;
  double to_x =    camera.range_x + camera.x;
  double from_y = -camera.range_y + camera.y;
  double to_y =    camera.range_y + camera.y;
  
  for (size_t x = 0; x < (size_t)COLS; x++) {
    for (size_t y = 0; y < (size_t)LINES; y++) {
      double threshold = lerp(from_y, to_y, (double)y/(double)LINES);
      double world_x = lerp(from_x, to_x, (double)x/(double)COLS);

      int32_t at = floor(world_x);

      // TODO: There aren't always enough pixels to draw the border
      if (at > WORLD_SIZE-1) {
	if (at - 1 <= WORLD_SIZE-1)
	  mvaddch(y, x, '|');
	continue;
      }
      if (at < 0) {
	if (at + 1 >= 0)
	  mvaddch(y, x, '|');
	continue;
      }
      
      double height;
      double velocity;

      if (should_lerp) {
	double t = world_x - (double)at;
      	Cell c0 = cell_get(world, at);
	Cell c1 = cell_get(world, at + 1);
	height = smootherstep(c0.position, c1.position, t);
	velocity = smootherstep(c0.velocity, c1.velocity, t);
      } else {
	Cell c = cell_get(world, at);
	height = c.position;
	velocity = c.velocity;
      }
      
      if (height >= threshold && threshold > 0.0)
	mvaddch(y, x, velocity_gradient(velocity) | A_REVERSE);

      if (height <= threshold && threshold < 0.0)
	mvaddch(y, x, velocity_gradient(velocity) | A_REVERSE);

    }
  }
}

void ui_draw(Camera camera, bool fast_mode, bool paused, double dt, double sim_speed) {
  // TODO: "to x" overflows at large values
  mvprintw(LINES/2, 0, "%.2f", -camera.range_x + camera.x); // from x
  mvprintw(LINES/2, COLS-6, "%.2f", camera.range_x + camera.x); // to x
  mvprintw(LINES-1, COLS/2, "%.2f", -camera.range_y + camera.y); // from y 
  mvprintw(0, COLS/2, "%.2f", camera.range_y + camera.y); // to y

  mvprintw(0, 0, "%s", fast_mode ? "" : "(fast mode active)");
  mvprintw(1, 0, "%s", paused ? "" : "(running the simulation)");
  mvprintw(2, 0, "dt: %f (%.2f FPS)", dt, 1.0/dt);
  mvprintw(3, 0, "simulation speed: %.2fx", sim_speed);
}

void redraw(state_t* s) {
  erase();
  world_draw(s->world.front, s->camera, s->should_lerp);
  ui_draw(s->camera, s->fast_mode, s->paused, s->dt, s->sim_speed);
  refresh();
}

#define dt_from_timestamps(s) (s)->dt = \
    ((double)((s)->end.tv_sec - (s)->start.tv_sec) \
  + (double)((s)->end.tv_nsec - (s)->start.tv_nsec) * 1e-9)

void events(state_t* s) {
  switch (getch()) {
  case 'h':
    s->camera.range_x *= 0.8;
    break;
  case 'l':
    s->camera.range_x *= 1.2;
    break;
  case 'j':
    s->camera.range_y *= 0.8;
    break;
  case 'k':
    s->camera.range_y *= 1.2;
    break;

  case KEY_LEFT:
    s->camera.x -= (!s->fast_mode ? FAST_MODE_SPEED : SPEED) * s->camera.range_x;
    break;
  case KEY_RIGHT:
    s->camera.x += (!s->fast_mode ? FAST_MODE_SPEED : SPEED) * s->camera.range_x;
    break;
  case KEY_UP:
    s->camera.y -= (!s->fast_mode ? FAST_MODE_SPEED : SPEED) * s->camera.range_y;
    break;
  case KEY_DOWN:
    s->camera.y += (!s->fast_mode ? FAST_MODE_SPEED : SPEED) * s->camera.range_y;
    break;

  case '+':
    s->sim_speed *= 1.2;
    break;
  case '-':
    s->sim_speed *= 0.8;
    break;
  case '=':
    s->sim_speed = 1.0;
    break;

  case 'r':
    // TODO: Hot reloading
    break;
    
  case 'a':
    s->should_lerp = !s->should_lerp;
    break;
  case 'n':
    s->camera.range_x = COLS/2.0;
    s->camera.x = WORLD_SIZE/2.0;
    s->camera.range_y = 1;
    s->camera.y = 0;
    break;
  case 's':
    step_simulation(&s->world.back, &s->world.front, 0.5 * (s->sim_speed));
    break;
  case 'x':
    s->fast_mode = !s->fast_mode;
    break;
  case 'q':
    s->running = false;
    break;
  case ' ':
    s->paused = !s->paused;
    break;
  }
}

int main(void) {
  state_t state = {0};
  
  state.lib = dlopen("./libfields.so", RTLD_NOW);
  if (state.lib == NULL) {
    fprintf(stderr, "Error: %s\n", dlerror());
    return -1;
  }
  state.world_alloc = dlsym(state.lib, "world_alloc");
  state.world_init  = dlsym(state.lib, "world_init");
  state.world_free  = dlsym(state.lib, "world_free");

  state.world_alloc(&state.world);
  state.world_init(&state.world);
  
  state.running = true;
  state.paused  = true;
  state.should_lerp = true;

  state.dt = 0.001;
  state.sim_speed = 1.0;

  initscr();
  noecho();
  nodelay(stdscr, true);
  keypad(stdscr, true);
  
  state.camera = (Camera){WORLD_SIZE/2.0, 0.0, COLS/2.0, 1.0};

  clock_gettime(CLOCK_MONOTONIC, &state.start);
  while (state.running) {
    clock_gettime(CLOCK_MONOTONIC, &state.end);
    dt_from_timestamps(&state);
    state.start = state.end;
    events(&state);

    if (!state.paused) {
      step_simulation(&state.world.back, &state.world.front, state.dt * state.sim_speed);
    }
    
    redraw(&state);
  }

  endwin();
  state.world_free(&state.world);
  dlclose(state.lib);
}
