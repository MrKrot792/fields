/// # Before using:
/// Define `SCOPE_IMPLEMENTATION` right before including the
/// file.
/// This library uses 64-bit floating pointer numbers. If you
/// don't like that, you can define `SCOPE_FLOAT_PRECISION` before
/// including the file, which will make the library use 32-bit floats.
///
/// # How to use;
/// todo
#include <stdbool.h>

#ifndef SCOPE_H
#define SCOPE_H

#ifdef SCOPE_FLOAT_PRECISION
  #define SCOPE_FLOAT float
#else
  #define SCOPE_FLOAT double
#endif

typedef struct {
  SCOPE_FLOAT x;
  SCOPE_FLOAT y;
  SCOPE_FLOAT range_x; // same
  SCOPE_FLOAT range_y; // means from -y to y
} Camera;

typedef struct Scope_State Scope_State;

typedef void Scope_Cell;
typedef Scope_Cell* Scope_World;

/// Params:
///   1. from
///     User should read the previous values from this pointer.
///   2. to
///     User should write the new values to this pointer.
///   3. delta_time
///     Formally: Change in time between frames. But it's actually the
///     time spent on the previous frame.
/// Returns;
///   Nothing
typedef void scope_simulate_fn(const Scope_World, Scope_World, SCOPE_FLOAT);
/// Params:
///   1. cell
///     The curent cell. May or may not be lerped.
///   2,3. position
///     Current cell's position.
/// Returns:
///   ncurses' character (int) that shall be drawn.
typedef int scope_draw_fn(const Scope_Cell*, SCOPE_FLOAT, SCOPE_FLOAT);
/// Params:
///   1. from
///   2. to
///   3. x
/// Returns:
///   A value between `from` to `to` interpolated using a preferred
///   method over `x`.
typedef const Scope_Cell* scope_lerp_fn(const Scope_Cell*, const Scope_Cell*, SCOPE_FLOAT);

/// Use this to create a scope instance, you can also start running
/// the moment you enter the loop
Scope_State* scope_init(scope_simulate_fn sim_fn, scope_draw_fn draw_fn, bool start_running);
/// Use this to enable anti alliasing
void scope_lerp(Scope_State* scope, scope_lerp_fn* lerp_fn);
/// This just starts the libscope loop
void scope_loop(Scope_State* scope);
/// This deallocates all memory allocated by libscope
void scope_deinit(Scope_State* scope);

#ifdef SCOPE_IMPLEMENTATION
#include <time.h>
#include <stsbool.h>

struct Scope_State {
  bool running;
  bool paused;
  bool anti_aliasing;
  
  struct timespec start;
  struct timespec end;

  SCOPE_FLOAT delta_time;
  SCOPE_FLOAT sim_speed;
  Camera camera;

  scope_simulate_fn sim_fn;
  scope_draw_fn     draw_fn;
  scope_lerp_fn     lerp_fn;
};

Scope_State* scope_init(scope_simulate_fn sim_fn, scope_draw_fn draw_fn, bool start_running) {
  Scope_State* r = malloc(sizeof(Scope_State));

  r->running = true;
  r->paused = start_running;
  r->anti_aliasing = true;
  
  r->start = {0};
  r->end   = {0};

  r->delta_time = 0.0;
  r->sim_speed = 1.0;
  r->camera = (Camera){}
}
void scope_lerp(Scope_State* scope, scope_lerp_fn* lerp_fn);
void scope_loop(Scope_State* scope);
void scope_deinit(Scope_State* scope);

#endif // SCOPE_IMPLEMENTATION
#endif // SCOPE_H
