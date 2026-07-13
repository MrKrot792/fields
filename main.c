#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <ncurses.h>

#define WORLD_SIZE 512

typedef struct {
  double velocity;
  double position;
} Cell;

Cell cell_get(const Cell* cells, int32_t i) {
  if (i < 0) return (Cell){0, 0};
  if (i > WORLD_SIZE - 1) return (Cell){0, 0};
  return cells[i];
}

void cell_set(Cell cell, Cell* cells, int32_t i) {
  if (i < 0 || i > WORLD_SIZE - 1) return;
  cells[i] = cell;
}

inline Cell cell_make(double position) { return (Cell){0, position}; }

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

void world_simulate(const Cell* world, Cell* output, double dt) {
  for (size_t i = 0; i < WORLD_SIZE; ++i) {
    Cell l = cell_get(world, (int32_t)i - 1);
    Cell r  = cell_get(world, (int32_t)i + 1);
    Cell c = cell_get(world, (int32_t)i);

    double a = dif2_cell(l, c, r);
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

typedef struct {
  double x;
  double y;
  double range_x; // same
  double range_y; // means from -y to y
} Camera;

double lerp(double a, double b, double f) {
  return a + f * (b - a);
}

int gradient(double velocity) {
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
      double t = world_x - (double)at;

      double height;
      double velocity;

      if (should_lerp) {
	Cell c0 = cell_get(world, at);
	Cell c1 = cell_get(world, at + 1);
	height = lerp(c0.position, c1.position, t);
	velocity = lerp(c0.velocity, c1.velocity, t);
      } else {
	Cell c = cell_get(world, at);
	height = c.position;
	velocity = c.velocity;
      }
      
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

      if (height >= threshold && threshold > 0.0)
	mvaddch(y, x, gradient(velocity) | A_REVERSE);

      if (height <= threshold && threshold < 0.0)
	mvaddch(y, x, gradient(velocity) | A_REVERSE);

    }
  }
}

void ui_draw(Camera camera, bool fast_mode, bool paused, double dt, double sim_speed) {
  mvprintw(LINES/2, 0, "%.2f", -camera.range_x + camera.x); // from x
  mvprintw(LINES/2, COLS-6, "%.2f", camera.range_x + camera.x); // to x
  mvprintw(LINES-1, COLS/2, "%.2f", -camera.range_y + camera.y); // from y 
  mvprintw(0, COLS/2, "%.2f", camera.range_y + camera.y); // to y

  mvprintw(0, 0, "%s", fast_mode ? "" : "(fast mode active)");
  mvprintw(1, 0, "%s", paused ? "" : "(running the simulation)");
  mvprintw(2, 0, "dt: %f", dt);
  mvprintw(3, 0, "simulation speed: %.2fx", sim_speed);
}

void redraw(const Cell* cells, Camera* camera, bool fast_mode, bool paused, double dt, double sim_speed, bool should_lerp) {
  erase();
  world_draw(cells, *camera, should_lerp);
  ui_draw(*camera, fast_mode, paused, dt, sim_speed);
  refresh();
}

double dt_from_timestamps(struct timespec start, struct timespec end) {
  return (double)(end.tv_sec - start.tv_sec) +
    (double)(end.tv_nsec - start.tv_nsec) * 1e-9;
}

#define FAST_MODE_SPEED 2.0
#define SPEED 0.25
void events(
  bool* paused,
  bool* running,
  bool* fast_mode,
  bool* should_lerp,
  Camera* camera,
  Cell** world_back,
  Cell** world_front,
  double* sim_speed
) {
  switch (getch()) {
  case 'h':
    camera->range_x *= 0.8;
    break;
  case 'l':
    camera->range_x *= 1.2;
    break;
  case 'j':
    camera->range_y *= 0.8;
    break;
  case 'k':
    camera->range_y *= 1.2;
    break;

  case KEY_LEFT:
    camera->x -= (!*fast_mode ? FAST_MODE_SPEED : SPEED) * camera->range_x;
    break;
  case KEY_RIGHT:
    camera->x += (!*fast_mode ? FAST_MODE_SPEED : SPEED) * camera->range_x;
    break;
  case KEY_UP:
    camera->y -= (!*fast_mode ? FAST_MODE_SPEED : SPEED) * camera->range_y;
    break;
  case KEY_DOWN:
    camera->y += (!*fast_mode ? FAST_MODE_SPEED : SPEED) * camera->range_y;
    break;

  case '+':
    *sim_speed *= 1.2;
    break;
  case '-':
    *sim_speed *= 0.8;
    break;
  case '=':
    *sim_speed = 1.0;
    break;

  case 'a':
    *should_lerp = !*should_lerp;
    break;
  case 'n':
    camera->range_x = COLS/2.0;
    camera->x = WORLD_SIZE/2.0;
    camera->range_y = 1;
    camera->y = 0;
    break;
  case 's':
    step_simulation(world_back, world_front, 0.5 * *sim_speed);
    break;
  case 'x':
    *fast_mode = !*fast_mode;
    break;
  case 'q':
    *running = false;
    break;
  case ' ':
    *paused = !*paused;
    break;
  }
}

int main(void) {
  Cell* world_front = malloc(sizeof(Cell) * WORLD_SIZE);
  Cell* world_back  = malloc(sizeof(Cell) * WORLD_SIZE);

  world_back[WORLD_SIZE/2 + 1] = cell_make(1);
  world_back[WORLD_SIZE/2 - 1] = cell_make(1);
  memcpy(world_front, world_back, sizeof(Cell) * WORLD_SIZE);

  bool running = true;
  bool paused  = true;
  bool fast_mode = false;
  bool should_lerp = true;

  struct timespec start = {0};
  struct timespec end = {0};
  double dt = 0.001;
  double simulation_speed = 1.0;

  initscr();
  noecho();
  nodelay(stdscr, true);
  keypad(stdscr, true);
  
  Camera camera = (Camera){WORLD_SIZE/2.0, 0.0, COLS/2.0, 1.0};

  world_draw(world_front, camera, should_lerp);
  clock_gettime(CLOCK_MONOTONIC, &start);
  while (running) {
    clock_gettime(CLOCK_MONOTONIC, &end);
    dt = dt_from_timestamps(start, end);
    start = end;
    events(&paused, &running, &fast_mode, &should_lerp, &camera, &world_front, &world_back, &simulation_speed);

    if (!paused) {
      step_simulation(&world_back, &world_front, dt * simulation_speed);
    }
    redraw(world_front, &camera, fast_mode, paused, dt, simulation_speed, should_lerp);
  }

  endwin();
  free(world_front);
  free(world_back);
}
