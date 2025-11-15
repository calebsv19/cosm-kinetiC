#ifndef FLUID2D_H
#define FLUID2D_H

#include <stdint.h>
#include "app/app_config.h"
#include "physics/math/math2d.h"
#include "physics/objects/object_manager.h"

typedef struct Fluid2D {
    int w;
    int h;
    float *density;
    float *density_prev;
    float *velX;
    float *velY;
    float *velX_prev;
    float *velY_prev;
} Fluid2D;

Fluid2D *fluid2d_create(int w, int h);
void      fluid2d_destroy(Fluid2D *f);

void fluid2d_clear(Fluid2D *f);

void fluid2d_add_density(Fluid2D *f, int x, int y, float amount);
void fluid2d_add_velocity(Fluid2D *f, int x, int y, float vx, float vy);

void fluid2d_step(Fluid2D *f, double dt, const AppConfig *cfg);
void fluid2d_apply_object_mask(Fluid2D *f,
                               const ObjectManager *objects,
                               const AppConfig *cfg);
void fluid2d_apply_static_mask(Fluid2D *f, const uint8_t *mask);

#endif // FLUID2D_H
