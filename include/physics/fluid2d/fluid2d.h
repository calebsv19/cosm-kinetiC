#ifndef FLUID2D_H
#define FLUID2D_H

#include <stdint.h>
#include "app/app_config.h"
#include "app/scene_presets.h"
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
    float *pressure;
} Fluid2D;

Fluid2D *fluid2d_create(int w, int h);
void      fluid2d_destroy(Fluid2D *f);

void fluid2d_clear(Fluid2D *f);

void fluid2d_add_density(Fluid2D *f, int x, int y, float amount);
void fluid2d_add_velocity(Fluid2D *f, int x, int y, float vx, float vy);

void fluid2d_step(Fluid2D *f,
                  double dt,
                  const AppConfig *cfg,
                  const BoundaryFlow flows[BOUNDARY_EDGE_COUNT],
                  const uint8_t *solid_mask,
                  const float *solid_vel_x,
                  const float *solid_vel_y);
void fluid2d_enforce_solid_mask(Fluid2D *f,
                                const uint8_t *mask,
                                const float *mask_vel_x,
                                const float *mask_vel_y);

float fluid2d_sample_density(const Fluid2D *f, float x, float y);
float fluid2d_sample_velocity(const Fluid2D *f, float x, float y, Vec2 *out_vel);

#endif // FLUID2D_H
