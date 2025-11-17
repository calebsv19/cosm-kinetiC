#include "physics/fluid2d/fluid2d.h"
#include "physics/objects/object_manager.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define FLUID_SOLVER_ITERATIONS_DEFAULT 20

static inline size_t idx(const Fluid2D *f, int x, int y) {
    return (size_t)y * (size_t)f->w + (size_t)x;
}

static inline float clamp_cell(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static void swap_buffers(float **a, float **b) {
    float *tmp = *a;
    *a = *b;
    *b = tmp;
}

static bool boundary_open(const BoundaryFlow flows[BOUNDARY_EDGE_COUNT],
                          BoundaryFlowEdge edge) {
    if (!flows) return false;
    return flows[edge].mode != BOUNDARY_FLOW_DISABLED;
}

static void set_bnd(const Fluid2D *f,
                    int b,
                    float *x,
                    const BoundaryFlow flows[BOUNDARY_EDGE_COUNT]) {
    int w = f->w;
    int h = f->h;
    if (w < 2 || h < 2) return;

    bool top_open = boundary_open(flows, BOUNDARY_EDGE_TOP);
    bool bottom_open = boundary_open(flows, BOUNDARY_EDGE_BOTTOM);
    bool left_open = boundary_open(flows, BOUNDARY_EDGE_LEFT);
    bool right_open = boundary_open(flows, BOUNDARY_EDGE_RIGHT);

    for (int i = 1; i < w - 1; ++i) {
        if (top_open) {
            x[idx(f, i, 0)] = x[idx(f, i, 1)];
        } else {
            x[idx(f, i, 0)] = (b == 2) ? -x[idx(f, i, 1)] : x[idx(f, i, 1)];
        }
        if (bottom_open) {
            x[idx(f, i, h - 1)] = x[idx(f, i, h - 2)];
        } else {
            x[idx(f, i, h - 1)] = (b == 2) ? -x[idx(f, i, h - 2)] : x[idx(f, i, h - 2)];
        }
    }

    for (int j = 1; j < h - 1; ++j) {
        if (left_open) {
            x[idx(f, 0, j)] = x[idx(f, 1, j)];
        } else {
            x[idx(f, 0, j)] = (b == 1) ? -x[idx(f, 1, j)] : x[idx(f, 1, j)];
        }
        if (right_open) {
            x[idx(f, w - 1, j)] = x[idx(f, w - 2, j)];
        } else {
            x[idx(f, w - 1, j)] = (b == 1) ? -x[idx(f, w - 2, j)] : x[idx(f, w - 2, j)];
        }
    }

    x[idx(f, 0,      0     )] = 0.5f * (x[idx(f, 1,      0     )] + x[idx(f, 0,      1     )]);
    x[idx(f, 0,      h - 1 )] = 0.5f * (x[idx(f, 1,      h - 1 )] + x[idx(f, 0,      h - 2 )]);
    x[idx(f, w - 1,  0     )] = 0.5f * (x[idx(f, w - 2,  0     )] + x[idx(f, w - 1,  1     )]);
    x[idx(f, w - 1,  h - 1 )] = 0.5f * (x[idx(f, w - 2,  h - 1 )] + x[idx(f, w - 1,  h - 2 )]);
}

static void lin_solve(const Fluid2D *f,
                      int b,
                      float *x,
                      const float *x0,
                      float a,
                      float c,
                      int iterations,
                      const BoundaryFlow flows[BOUNDARY_EDGE_COUNT]) {
    int w = f->w;
    int h = f->h;
    if (iterations < 1) iterations = 1;
    for (int k = 0; k < iterations; ++k) {
        for (int j = 1; j < h - 1; ++j) {
            for (int i = 1; i < w - 1; ++i) {
                size_t id = idx(f, i, j);
                x[id] = (x0[id] + a * (x[idx(f, i - 1, j)] +
                                       x[idx(f, i + 1, j)] +
                                       x[idx(f, i, j - 1)] +
                                       x[idx(f, i, j + 1)])) / c;
            }
        }
        set_bnd(f, b, x, flows);
    }
}

static void diffuse(const Fluid2D *f,
                    int b,
                    float *x,
                    const float *x0,
                    float diff,
                    float dt,
                    int iterations,
                    const BoundaryFlow flows[BOUNDARY_EDGE_COUNT]) {
    float a = diff * (float)(f->w - 2) * (float)(f->h - 2) * dt;
    lin_solve(f, b, x, x0, a, 1.0f + 4.0f * a, iterations, flows);
}

static void advect(const Fluid2D *f,
                   int b,
                   float *d,
                   const float *d0,
                   const float *velX,
                   const float *velY,
                   float dt,
                   const BoundaryFlow flows[BOUNDARY_EDGE_COUNT]) {
    int w = f->w;
    int h = f->h;

    for (int j = 1; j < h - 1; ++j) {
        for (int i = 1; i < w - 1; ++i) {
            size_t id = idx(f, i, j);
            float x = (float)i - dt * velX[id];
            float y = (float)j - dt * velY[id];

            x = clamp_cell(x, 0.5f, (float)w - 1.5f);
            y = clamp_cell(y, 0.5f, (float)h - 1.5f);

            int i0 = (int)x;
            int i1 = i0 + 1;
            int j0 = (int)y;
            int j1 = j0 + 1;

            float s1 = x - (float)i0;
            float s0 = 1.0f - s1;
            float t1 = y - (float)j0;
            float t0 = 1.0f - t1;

            d[id] =
                s0 * (t0 * d0[idx(f, i0, j0)] + t1 * d0[idx(f, i0, j1)]) +
                s1 * (t0 * d0[idx(f, i1, j0)] + t1 * d0[idx(f, i1, j1)]);
        }
    }
    set_bnd(f, b, d, flows);
}

static void project(const Fluid2D *f,
                    float *velX,
                    float *velY,
                    float *p,
                    float *div,
                    int iterations,
                    const BoundaryFlow flows[BOUNDARY_EDGE_COUNT]) {
    int w = f->w;
    int h = f->h;
    float inv_w = 1.0f / (float)w;
    float inv_h = 1.0f / (float)h;

    for (int j = 1; j < h - 1; ++j) {
        for (int i = 1; i < w - 1; ++i) {
            size_t id = idx(f, i, j);
            div[id] = -0.5f * (
                (velX[idx(f, i + 1, j)] - velX[idx(f, i - 1, j)]) * inv_w +
                (velY[idx(f, i, j + 1)] - velY[idx(f, i, j - 1)]) * inv_h
            );
            p[id] = 0.0f;
        }
    }

    set_bnd(f, 0, div, flows);
    set_bnd(f, 0, p, flows);
    lin_solve(f, 0, p, div, 1.0f, 4.0f, iterations, flows);

    for (int j = 1; j < h - 1; ++j) {
        for (int i = 1; i < w - 1; ++i) {
            size_t id = idx(f, i, j);
            f->pressure[id] = p[id];
            velX[id] -= 0.5f * (p[idx(f, i + 1, j)] - p[idx(f, i - 1, j)]) * (float)w;
            velY[id] -= 0.5f * (p[idx(f, i, j + 1)] - p[idx(f, i, j - 1)]) * (float)h;
        }
    }

    set_bnd(f, 1, velX, flows);
    set_bnd(f, 2, velY, flows);
}

static void apply_buoyancy(Fluid2D *f, const AppConfig *cfg, float dt) {
    if (!cfg || cfg->fluid_buoyancy_force == 0.0f) return;
    size_t count = (size_t)f->w * (size_t)f->h;
    if (count == 0) return;

    double sum = 0.0;
    for (size_t i = 0; i < count; ++i) {
        sum += f->density[i];
    }
    float avg_density = (float)(sum / (double)count);
    float force = cfg->fluid_buoyancy_force;

    for (size_t i = 0; i < count; ++i) {
        float buoy = (f->density[i] - avg_density) * force;
        f->velY[i] -= buoy * dt;
    }
}

static int solver_iterations_from_config(const AppConfig *cfg) {
    int iterations = (cfg && cfg->fluid_solver_iterations > 0)
                         ? cfg->fluid_solver_iterations
                         : FLUID_SOLVER_ITERATIONS_DEFAULT;
    if (iterations < 1) iterations = 1;
    return iterations;
}

Fluid2D *fluid2d_create(int w, int h) {
    if (w < 2 || h < 2) return NULL;

    Fluid2D *f = (Fluid2D *)malloc(sizeof(Fluid2D));
    if (!f) return NULL;

    f->w = w;
    f->h = h;

    size_t count = (size_t)w * (size_t)h;
    f->density      = (float *)calloc(count, sizeof(float));
    f->density_prev = (float *)calloc(count, sizeof(float));
    f->velX         = (float *)calloc(count, sizeof(float));
    f->velY         = (float *)calloc(count, sizeof(float));
    f->velX_prev    = (float *)calloc(count, sizeof(float));
    f->velY_prev    = (float *)calloc(count, sizeof(float));
    f->pressure     = (float *)calloc(count, sizeof(float));

    if (!f->density || !f->density_prev ||
        !f->velX || !f->velY ||
        !f->velX_prev || !f->velY_prev ||
        !f->pressure) {
        fluid2d_destroy(f);
        return NULL;
    }

    return f;
}

void fluid2d_destroy(Fluid2D *f) {
    if (!f) return;
    free(f->density);
    free(f->density_prev);
    free(f->velX);
    free(f->velY);
    free(f->velX_prev);
    free(f->velY_prev);
    free(f->pressure);
    free(f);
}

void fluid2d_clear(Fluid2D *f) {
    if (!f) return;
    size_t count = (size_t)f->w * (size_t)f->h;
    memset(f->density,      0, count * sizeof(float));
    memset(f->density_prev, 0, count * sizeof(float));
    memset(f->velX,         0, count * sizeof(float));
    memset(f->velY,         0, count * sizeof(float));
    memset(f->velX_prev,    0, count * sizeof(float));
    memset(f->velY_prev,    0, count * sizeof(float));
    memset(f->pressure,     0, count * sizeof(float));
}

void fluid2d_add_density(Fluid2D *f, int x, int y, float amount) {
    if (!f) return;
    if (f->w < 2 || f->h < 2) return;
    if (x < 1) x = 1;
    if (x > f->w - 2) x = f->w - 2;
    if (y < 1) y = 1;
    if (y > f->h - 2) y = f->h - 2;
    size_t id = idx(f, x, y);
    f->density[id] += amount;
}

void fluid2d_add_velocity(Fluid2D *f, int x, int y, float vx, float vy) {
    if (!f) return;
    if (f->w < 2 || f->h < 2) return;
    if (x < 1) x = 1;
    if (x > f->w - 2) x = f->w - 2;
    if (y < 1) y = 1;
    if (y > f->h - 2) y = f->h - 2;
    size_t id = idx(f, x, y);
    f->velX[id] += vx;
    f->velY[id] += vy;
}

static int cell_from_world(float pos, int grid, float world_max) {
    float normalized = pos / world_max;
    if (normalized < 0.0f) normalized = 0.0f;
    if (normalized > 1.0f) normalized = 1.0f;
    return (int)(normalized * (float)(grid - 1));
}

static float cell_to_world(int cell, int grid, float world_max) {
    if (grid <= 1) return 0.0f;
    float t = (float)cell / (float)(grid - 1);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return t * world_max;
}

static float world_velocity_to_grid(float v, int grid, float world_max) {
    if (grid <= 1 || world_max <= 0.0f) return 0.0f;
    return v * (float)(grid - 1) / world_max;
}

void fluid2d_apply_object_mask(Fluid2D *f,
                               const ObjectManager *objects,
                               const AppConfig *cfg) {
    if (!f || !objects || !cfg) return;
    if (!objects->objects || objects->count == 0) return;

    for (int i = 0; i < objects->count; ++i) {
        const SceneObject *obj = &objects->objects[i];
        if (!obj || obj->body.is_static) continue;
        float vel_grid_x = world_velocity_to_grid(obj->body.velocity.x,
                                                  f->w,
                                                  (float)cfg->window_w);
        float vel_grid_y = world_velocity_to_grid(obj->body.velocity.y,
                                                  f->h,
                                                  (float)cfg->window_h);
        if (obj->type == SCENE_OBJECT_CIRCLE) {
            int cx = cell_from_world(obj->body.position.x, f->w, (float)cfg->window_w);
            int cy = cell_from_world(obj->body.position.y, f->h, (float)cfg->window_h);
            int radius = (int)(obj->body.radius / (float)cfg->window_w * (float)f->w);
            if (radius < 1) radius = 1;
            for (int y = cy - radius; y <= cy + radius; ++y) {
                if (y <= 0 || y >= f->h - 1) continue;
                for (int x = cx - radius; x <= cx + radius; ++x) {
                    if (x <= 0 || x >= f->w - 1) continue;
                    float dx = (float)(x - cx);
                    float dy = (float)(y - cy);
                    if (dx * dx + dy * dy <= (float)(radius * radius)) {
                        size_t id = idx(f, x, y);
                        f->density[id] = 0.0f;
                        f->velX[id] = vel_grid_x;
                        f->velY[id] = vel_grid_y;
                    }
                }
            }
        } else if (obj->type == SCENE_OBJECT_BOX) {
            float half_diag = sqrtf(obj->body.half_extents.x * obj->body.half_extents.x +
                                    obj->body.half_extents.y * obj->body.half_extents.y);
            int min_x = cell_from_world(obj->body.position.x - half_diag,
                                        f->w, (float)cfg->window_w);
            int max_x = cell_from_world(obj->body.position.x + half_diag,
                                        f->w, (float)cfg->window_w);
            int min_y = cell_from_world(obj->body.position.y - half_diag,
                                        f->h, (float)cfg->window_h);
            int max_y = cell_from_world(obj->body.position.y + half_diag,
                                        f->h, (float)cfg->window_h);
            if (min_x < 1) min_x = 1;
            if (max_x > f->w - 2) max_x = f->w - 2;
            if (min_y < 1) min_y = 1;
            if (max_y > f->h - 2) max_y = f->h - 2;

            float cos_a = cosf(obj->body.angle);
            float sin_a = sinf(obj->body.angle);
            for (int y = min_y; y <= max_y; ++y) {
                float world_y = cell_to_world(y, f->h, (float)cfg->window_h);
                for (int x = min_x; x <= max_x; ++x) {
                    float world_x = cell_to_world(x, f->w, (float)cfg->window_w);
                    float rel_x = world_x - obj->body.position.x;
                    float rel_y = world_y - obj->body.position.y;
                    float local_x =  rel_x * cos_a + rel_y * sin_a;
                    float local_y = -rel_x * sin_a + rel_y * cos_a;
                    if (fabsf(local_x) <= obj->body.half_extents.x &&
                        fabsf(local_y) <= obj->body.half_extents.y) {
                        size_t id = idx(f, x, y);
                        f->density[id] = 0.0f;
                        f->velX[id] = vel_grid_x;
                        f->velY[id] = vel_grid_y;
                    }
                }
            }
        }
    }
}

void fluid2d_apply_static_mask(Fluid2D *f, const uint8_t *mask) {
    if (!f || !mask) return;
    size_t count = (size_t)f->w * (size_t)f->h;
    for (size_t i = 0; i < count; ++i) {
        if (mask[i]) {
            f->density[i] = 0.0f;
            f->velX[i] = 0.0f;
            f->velY[i] = 0.0f;
        }
    }
}

void fluid2d_step(Fluid2D *f,
                  double dt,
                  const AppConfig *cfg,
                  const BoundaryFlow flows[BOUNDARY_EDGE_COUNT]) {
    if (!f || !cfg) return;
    float diff = cfg->density_diffusion;
    float visc = cfg->velocity_damping;
    float fdt  = (float)dt;
    int iterations = solver_iterations_from_config(cfg);

    swap_buffers(&f->velX_prev, &f->velX);
    diffuse(f, 1, f->velX, f->velX_prev, visc, fdt, iterations, flows);

    swap_buffers(&f->velY_prev, &f->velY);
    diffuse(f, 2, f->velY, f->velY_prev, visc, fdt, iterations, flows);

    project(f, f->velX, f->velY, f->velX_prev, f->velY_prev, iterations, flows);

    swap_buffers(&f->velX_prev, &f->velX);
    swap_buffers(&f->velY_prev, &f->velY);
    advect(f, 1, f->velX, f->velX_prev, f->velX_prev, f->velY_prev, fdt, flows);
    advect(f, 2, f->velY, f->velY_prev, f->velX_prev, f->velY_prev, fdt, flows);

    project(f, f->velX, f->velY, f->velX_prev, f->velY_prev, iterations, flows);

    apply_buoyancy(f, cfg, fdt);
    project(f, f->velX, f->velY, f->velX_prev, f->velY_prev, iterations, flows);

    swap_buffers(&f->density_prev, &f->density);
    diffuse(f, 0, f->density, f->density_prev, diff, fdt, iterations, flows);

    swap_buffers(&f->density_prev, &f->density);
    advect(f, 0, f->density, f->density_prev, f->velX, f->velY, fdt, flows);

    size_t count = (size_t)f->w * (size_t)f->h;
    float decay = (float)(cfg->density_decay * dt);
    if (decay > 1.0f) decay = 1.0f;
    float keep = 1.0f - decay;
    for (size_t i = 0; i < count; ++i) {
        f->density[i] = math_maxf(0.0f, f->density[i] * keep);
    }
}

static float sample_field(const float *field,
                          int w,
                          int h,
                          float x,
                          float y) {
    if (!field || w <= 1 || h <= 1) return 0.0f;
    x = clamp_cell(x, 0.5f, (float)w - 1.5f);
    y = clamp_cell(y, 0.5f, (float)h - 1.5f);
    int i0 = (int)x;
    int i1 = i0 + 1;
    int j0 = (int)y;
    int j1 = j0 + 1;
    float sx = x - (float)i0;
    float sy = y - (float)j0;
    float s0 = 1.0f - sx;
    float t0 = 1.0f - sy;
    float v00 = field[(size_t)j0 * (size_t)w + (size_t)i0];
    float v10 = field[(size_t)j0 * (size_t)w + (size_t)i1];
    float v01 = field[(size_t)j1 * (size_t)w + (size_t)i0];
    float v11 = field[(size_t)j1 * (size_t)w + (size_t)i1];
    return s0 * (t0 * v00 + sy * v01) + sx * (t0 * v10 + sy * v11);
}

float fluid2d_sample_density(const Fluid2D *f, float x, float y) {
    if (!f) return 0.0f;
    return sample_field(f->density, f->w, f->h, x, y);
}

float fluid2d_sample_velocity(const Fluid2D *f, float x, float y, Vec2 *out_vel) {
    if (!f || !out_vel) return 0.0f;
    float vx = sample_field(f->velX, f->w, f->h, x, y);
    float vy = sample_field(f->velY, f->w, f->h, x, y);
    out_vel->x = vx;
    out_vel->y = vy;
    return sqrtf(vx * vx + vy * vy);
}
