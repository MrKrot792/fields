#define TB_IMPL
#include "termbox2.h"

#include <dlfcn.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "fields.h"

#define FAST_MODE_SPEED 1
#define SPEED 0.1

Cell cell_get(const Cell* cells, size_t cells_size, int32_t i) {
  if (i < 0) return (Cell){0, 0, 1};
  if ((size_t)i > cells_size - 1) return (Cell){0, 0, 1};
  return cells[i];
}

double dif2_num(double nx, double x, double px, double dx) {
  return (nx - 2.0*x + px) / (dx * dx);
}

double dif2_cell(Cell nc, Cell c, Cell pc) {
  return dif2_num(nc.position, c.position, pc.position, 1.0);
}

double dif2(const Cell* cells, size_t cells_size, int32_t x) {
  return dif2_num(cell_get(cells, cells_size, x-1).position,
		  cell_get(cells, cells_size, x).position,
		  cell_get(cells, cells_size, x+1).position, 1);
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
  bool clean_mode;
  struct timespec start;
  struct timespec end;
  double dt;
  double sim_speed;
  Camera camera;
} state_t;

void world_simulate(const Cell* world, Cell* output, size_t world_size, double dt) {
  for (size_t i = 0; i < world_size; ++i) {
    Cell l = cell_get(world, world_size, (int32_t)i - 1);
    Cell r = cell_get(world, world_size, (int32_t)i + 1);
    Cell c = cell_get(world, world_size, (int32_t)i);

    double a = dif2_cell(l, c, r) * pow(1.0 / c.mass, 2);
    output[i] = c;
    output[i].velocity += a * dt;
    output[i].position += output[i].velocity * dt;
  }
}

void step_simulation(Cell** back, Cell** front, size_t world_size, double dt) {
  world_simulate(*back, *front, world_size, dt);

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

// TODO: This should take state_t
void world_draw(const Cell* world, size_t world_size, Camera camera, bool should_lerp) {
  double from_x = -camera.range_x + camera.x;
  double to_x =    camera.range_x + camera.x;
  double from_y = -camera.range_y + camera.y;
  double to_y =    camera.range_y + camera.y;

  int screen_width  = tb_width();
  int screen_height = tb_height();

  for (size_t x = 0; x < (size_t)screen_width; x++) {
    for (size_t y = 0; y < (size_t)screen_height; y++) {
      double threshold = lerp(from_y, to_y, (double)y/(double)screen_height);
      double world_x = lerp(from_x, to_x, (double)x/(double)screen_width);

      int32_t at = floor(world_x);

      if (at > (int32_t)(world_size - 1)) {
	if (at - 1 <= (int32_t)(world_size - 1))
	  tb_set_cell(x, y, '|', 0, 0);
	continue;
      }
      if (at < 0) {
	if (at + 1 >= 0)
	  tb_set_cell(x, y, '|', 0, 0);
	continue;
      }
      
      double height, velocity;
      if (should_lerp) {
	double t = world_x - (double)at;
      	Cell c0 = cell_get(world, world_size, at);
	Cell c1 = cell_get(world, world_size, at + 1);
	height = smootherstep(c0.position, c1.position, t);
	velocity = smootherstep(c0.velocity, c1.velocity, t);
      } else {
	Cell c = cell_get(world, world_size, at);
	height = c.position;
	velocity = c.velocity;
      }
      
      if (height >= threshold && threshold > 0.0)
	tb_set_cell(x, y, velocity_gradient(velocity), TB_REVERSE, 0);

      if (height <= threshold && threshold < 0.0)
	tb_set_cell(x, y, velocity_gradient(velocity), TB_REVERSE, 0);
    }
  }
}

void ui_draw(state_t* s) {
  // TODO: "to x" overflows at large values
  if (s->clean_mode) {
    return;
  }

  int sw = tb_width();
  int sh = tb_height();

  tb_printf(0,    sh/2, 0, 0, "%.2f", s->camera.range_x - s->camera.x); // from x
  tb_printf(sw-6, sh/2, 0, 0, "%.2f", s->camera.range_x + s->camera.x); //   to x
  tb_printf(sw/2, sh-1, 0, 0, "%.2f", s->camera.range_y - s->camera.y); // from y
  tb_printf(sw/2, 0,    0, 0, "%.2f", s->camera.range_y + s->camera.y); //   to y

  tb_printf(0, 0, 0, 0, "%s", s->fast_mode ? "" : "(fast mode active)");
  tb_printf(0, 1, 0, 0, "%s", s->paused ? "" : "(running the simulation)");
  tb_printf(0, 2, 0, 0, "dt: %f (%.2f FPS)", s->dt, 1.0/s->dt);
  tb_printf(0, 3, 0, 0, "simulation speed: %.2fx", s->sim_speed);
}

void redraw(state_t* s) {
  tb_clear();
  world_draw(s->world.front, s->world.size, s->camera, s->should_lerp);
  ui_draw(s);
  tb_present();
}

#define dt_from_timestamps(s) (s)->dt = \
    ((double)((s)->end.tv_sec - (s)->start.tv_sec) \
  + (double)((s)->end.tv_nsec - (s)->start.tv_nsec) * 1e-9)

void init_library(state_t* s) {
  s->lib = dlopen("./libfields.so", RTLD_NOW);
  if (s->lib == NULL) {
    fprintf(stderr, "Error: %s\n", dlerror());
    exit(-1);
  }
  s->world_alloc = dlsym(s->lib, "world_alloc");
  s->world_init  = dlsym(s->lib, "world_init");
  s->world_free  = dlsym(s->lib, "world_free");

  s->world_alloc(&s->world);
  s->world_init(&s->world);
}

void reload_library(state_t* s) {
  s->world_free(&s->world);
  dlclose(s->lib);
  init_library(s);
}

void events(state_t* s, struct tb_event* ev) {
  if (ev->type != TB_EVENT_KEY) return;
  switch (ev->key) {
  case TB_KEY_ARROW_LEFT:
    s->camera.x -= (!s->fast_mode ? FAST_MODE_SPEED : SPEED) * s->camera.range_x;
    break;
  case TB_KEY_ARROW_RIGHT:
    s->camera.x += (!s->fast_mode ? FAST_MODE_SPEED : SPEED) * s->camera.range_x;
    break;
  case TB_KEY_ARROW_UP:
    s->camera.y -= (!s->fast_mode ? FAST_MODE_SPEED : SPEED) * s->camera.range_y;
    break;
  case TB_KEY_ARROW_DOWN:
    s->camera.y += (!s->fast_mode ? FAST_MODE_SPEED : SPEED) * s->camera.range_y;
    break;
  case TB_KEY_SPACE:
    s->paused = !s->paused;
    break;
  }
  switch (ev->ch) {
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

  case 'c':
    s->clean_mode = !s->clean_mode;
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
    reload_library(s);
    break;
    
  case 'a':
    s->should_lerp = !s->should_lerp;
    break;
  case 'n':
    s->camera.range_x = tb_width()/2.0;
    s->camera.x = s->world.size/2.0;
    s->camera.range_y = 1;
    s->camera.y = 0;
    break;
  case 's':
    step_simulation(&s->world.back, &s->world.front, s->world.size, 0.5 * (s->sim_speed));
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

  init_library(&state);
  
  state.running     = true;
  state.paused      = true;
  state.should_lerp = true;
  state.sim_speed   = 1.0;
  state.fast_mode   = true;
  state.camera = (Camera){state.world.size/2.0, 0.0, tb_width()/2.0, 1.0};

  struct tb_event ev;
  
  tb_init();
  tb_hide_cursor();

  clock_gettime(CLOCK_MONOTONIC, &state.start);
  while (state.running) {
    clock_gettime(CLOCK_MONOTONIC, &state.end);
    dt_from_timestamps(&state);
    state.start = state.end;
    tb_peek_event(&ev, 0);
    events(&state, &ev);

    if (!state.paused) {
      step_simulation(&state.world.back, &state.world.front, state.world.size, state.dt * state.sim_speed);
    }
    
    redraw(&state);
  }

  tb_shutdown();
  state.world_free(&state.world);
  dlclose(state.lib);
}
