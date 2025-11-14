#ifndef FLUID2D_H
#define FLUID2D_H

#include "app/app_config.h"
#include "physics/math/math2d.h"

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

#endif // FLUID2D_H
