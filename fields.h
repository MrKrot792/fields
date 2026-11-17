#ifndef FIELDS_H
#define FIELDS_H

typedef struct {
  double velocity;
  double position;
  double mass;
} Cell;

#define cell_make(p) (Cell){0, (p), 1}
#define cell_make_mass(p, m) (Cell){0, (p), (m)}

typedef struct {
  Cell* back;
  Cell* front;
  size_t size;
} world_t;

typedef void (*ext_f)(world_t*);

#endif
