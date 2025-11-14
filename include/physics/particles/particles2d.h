#ifndef PARTICLES2D_H
#define PARTICLES2D_H

#include "physics/math/math2d.h"
#include "app/app_config.h"
#include "physics/fluid2d/fluid2d.h"
#include "physics/rigid/rigid2d.h"

// Lightweight particle system for smoke tracers, sparks, etc.

typedef struct Particle2D {
    Vec2  position;
    Vec2  velocity;
    float lifetime;     // remaining time
    float max_lifetime; // initial lifetime (for normalization if needed)
} Particle2D;

typedef struct Particles2D {
    Particle2D *particles;
    int count;
    int capacity;
} Particles2D;

Particles2D *particles2d_create(int capacity);
void         particles2d_destroy(Particles2D *p);

void particles2d_spawn(Particles2D *p,
                       Vec2 position,
                       Vec2 velocity,
                       float lifetime);

// Step particles; optionally respond to fluid velocity and gravity.
void particles2d_step(Particles2D *p,
                      double dt,
                      const AppConfig *cfg,
                      const Fluid2D   *fluid,
                      const Rigid2DWorld *rigid);

#endif // PARTICLES2D_H
