#include "physics/fluid2d/fluid2d.h"
#include "physics/objects/object_manager.h"

#include "timer_hud/time_scope.h"

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

void fluid2d_enforce_solid_mask(Fluid2D *f,
                                const uint8_t *mask,
                                const float *mask_vel_x,
                                const float *mask_vel_y) {
    if (!f || !mask) return;
    int w = f->w;
    int h = f->h;
    size_t count = (size_t)w * (size_t)h;
    for (size_t i = 0; i < count; ++i) {
        if (mask[i]) {
            float vx = mask_vel_x ? mask_vel_x[i] : 0.0f;
            float vy = mask_vel_y ? mask_vel_y[i] : 0.0f;
            f->velX[i] = vx;
            f->velY[i] = vy;
            f->density[i] = 0.0f;
            f->pressure[i] = 0.0f;
        }
    }

    for (int y = 1; y < h - 1; ++y) {
        for (int x = 1; x < w - 1; ++x) {
            size_t id = idx(f, x, y);
            if (mask[id]) continue;

            float vx_sum = 0.0f;
            float vy_sum = 0.0f;
            int touching = 0;
            const int offsets[4][2] = {
                {-1, 0}, {1, 0}, {0, -1}, {0, 1}
            };
            for (int n = 0; n < 4; ++n) {
                int nx = x + offsets[n][0];
                int ny = y + offsets[n][1];
                if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                size_t nid = idx(f, nx, ny);
                if (!mask[nid]) continue;
                float target_vx = mask_vel_x ? mask_vel_x[nid] : 0.0f;
                float target_vy = mask_vel_y ? mask_vel_y[nid] : 0.0f;
                vx_sum += target_vx;
                vy_sum += target_vy;
                ++touching;
            }
            if (touching > 0) {
                float inv = 1.0f / (float)touching;
                f->velX[id] = vx_sum * inv;
                f->velY[id] = vy_sum * inv;
            }
        }
    }
}

static inline void enforce_solids_if_needed(Fluid2D *f,
                                            const uint8_t *mask,
                                            const float *mask_vel_x,
                                            const float *mask_vel_y) {
    if (!mask) return;
    fluid2d_enforce_solid_mask(f, mask, mask_vel_x, mask_vel_y);
}

static inline void boundary_outward_normal(BoundaryFlowEdge edge, float *nx, float *ny) {
    if (!nx || !ny) return;
    switch (edge) {
    case BOUNDARY_EDGE_TOP:    *nx = 0.0f;  *ny = -1.0f; break;
    case BOUNDARY_EDGE_BOTTOM: *nx = 0.0f;  *ny = 1.0f;  break;
    case BOUNDARY_EDGE_LEFT:   *nx = -1.0f; *ny = 0.0f;  break;
    case BOUNDARY_EDGE_RIGHT:  *nx = 1.0f;  *ny = 0.0f;  break;
    default:                   *nx = 0.0f;  *ny = 0.0f;  break;
    }
}

static inline int clamp_band(int value, int lo, int hi) {
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

static inline float smooth_clamp_weight(int depth, int band) {
    if (band <= 1) return 1.0f;
    float t = 1.0f - (float)depth / (float)band;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    // smoothstep for gentler transition.
    return t * t * (3.0f - 2.0f * t);
}

static inline void clamp_velocity_along_normal(float *vx,
                                               float *vy,
                                               float nx,
                                               float ny,
                                               float min_out) {
    float vn = (*vx) * nx + (*vy) * ny;
    float tx = *vx - vn * nx;
    float ty = *vy - vn * ny;
    float clamped_vn = vn;
    if (clamped_vn < 0.0f) clamped_vn = 0.0f;
    if (min_out > 0.0f && clamped_vn < min_out) clamped_vn = min_out;
    *vx = tx + clamped_vn * nx;
    *vy = ty + clamped_vn * ny;
}

static int outlet_band_size(const Fluid2D *f, BoundaryFlowEdge edge) {
    if (!f) return 0;
    int dim = (edge == BOUNDARY_EDGE_TOP || edge == BOUNDARY_EDGE_BOTTOM)
                  ? f->h
                  : f->w;
    if (dim < 4) return 0;
    int band = dim / 5;
    if (band < 8) band = 8;
    if (band > 100) band = 100;
    if (band > dim - 2) band = dim - 2;
    return band;
}

static void enforce_outflow_band_for_edge(Fluid2D *f,
                                          BoundaryFlowEdge edge,
                                          int band,
                                          float min_out) {
    if (!f || band <= 0) return;
    int w = f->w;
    int h = f->h;
    float nx = 0.0f, ny = 0.0f;
    boundary_outward_normal(edge, &nx, &ny);
    if (nx == 0.0f && ny == 0.0f) return;

    switch (edge) {
    case BOUNDARY_EDGE_RIGHT: {
        int x_start = clamp_band(w - 1 - band, 1, w - 2);
        for (int y = 1; y < h - 1; ++y) {
            for (int x = x_start; x < w - 1; ++x) {
                int depth = (w - 2) - x;
                if (depth < 0) depth = 0;
                float weight = smooth_clamp_weight(depth, band);
                if (weight <= 0.0f) continue;
                size_t id = idx(f, x, y);
                float orig_vx = f->velX[id];
                float orig_vy = f->velY[id];
                float clamped_vx = orig_vx;
                float clamped_vy = orig_vy;
                clamp_velocity_along_normal(&clamped_vx, &clamped_vy, nx, ny, min_out);
                f->velX[id] = orig_vx + (clamped_vx - orig_vx) * weight;
                f->velY[id] = orig_vy + (clamped_vy - orig_vy) * weight;
            }
        }
        break;
    }
    case BOUNDARY_EDGE_LEFT: {
        int x_end = clamp_band(band, 1, w - 2);
        for (int y = 1; y < h - 1; ++y) {
            for (int x = 1; x <= x_end; ++x) {
                int depth = x - 1;
                float weight = smooth_clamp_weight(depth, band);
                if (weight <= 0.0f) continue;
                size_t id = idx(f, x, y);
                float orig_vx = f->velX[id];
                float orig_vy = f->velY[id];
                float clamped_vx = orig_vx;
                float clamped_vy = orig_vy;
                clamp_velocity_along_normal(&clamped_vx, &clamped_vy, nx, ny, min_out);
                f->velX[id] = orig_vx + (clamped_vx - orig_vx) * weight;
                f->velY[id] = orig_vy + (clamped_vy - orig_vy) * weight;
            }
        }
        break;
    }
    case BOUNDARY_EDGE_TOP: {
        int y_end = clamp_band(band, 1, h - 2);
        for (int y = 1; y <= y_end; ++y) {
            int depth = y - 1;
            float weight = smooth_clamp_weight(depth, band);
            if (weight <= 0.0f) continue;
            for (int x = 1; x < w - 1; ++x) {
                size_t id = idx(f, x, y);
                float orig_vx = f->velX[id];
                float orig_vy = f->velY[id];
                float clamped_vx = orig_vx;
                float clamped_vy = orig_vy;
                clamp_velocity_along_normal(&clamped_vx, &clamped_vy, nx, ny, min_out);
                f->velX[id] = orig_vx + (clamped_vx - orig_vx) * weight;
                f->velY[id] = orig_vy + (clamped_vy - orig_vy) * weight;
            }
        }
        break;
    }
    case BOUNDARY_EDGE_BOTTOM: {
        int y_start = clamp_band(h - 1 - band, 1, h - 2);
        for (int y = y_start; y < h - 1; ++y) {
            int depth = (h - 2) - y;
            float weight = smooth_clamp_weight(depth, band);
            if (weight <= 0.0f) continue;
            for (int x = 1; x < w - 1; ++x) {
                size_t id = idx(f, x, y);
                float orig_vx = f->velX[id];
                float orig_vy = f->velY[id];
                float clamped_vx = orig_vx;
                float clamped_vy = orig_vy;
                clamp_velocity_along_normal(&clamped_vx, &clamped_vy, nx, ny, min_out);
                f->velX[id] = orig_vx + (clamped_vx - orig_vx) * weight;
                f->velY[id] = orig_vy + (clamped_vy - orig_vy) * weight;
            }
        }
        break;
    }
    default:
        break;
    }
}

static void enforce_outflow_direction(Fluid2D *f,
                                      const AppConfig *cfg,
                                      const BoundaryFlow flows[BOUNDARY_EDGE_COUNT]) {
    if (!f || !cfg || !flows) return;
    float inflow_speed = cfg->tunnel_inflow_speed;
    if (!isfinite(inflow_speed) || inflow_speed < 0.0f) inflow_speed = 0.0f;
    float min_out = inflow_speed > 0.0f ? inflow_speed * 0.25f : 0.0f;
    for (int edge = 0; edge < BOUNDARY_EDGE_COUNT; ++edge) {
        if (flows[edge].mode != BOUNDARY_FLOW_RECEIVE) continue;
        int band = outlet_band_size(f, (BoundaryFlowEdge)edge);
        if (band <= 0) continue;
        enforce_outflow_band_for_edge(f, (BoundaryFlowEdge)edge, band, min_out);
    }
}

void fluid2d_step(Fluid2D *f,
                  double dt,
                  const AppConfig *cfg,
                  const BoundaryFlow flows[BOUNDARY_EDGE_COUNT],
                  const uint8_t *solid_mask,
                  const float *solid_vel_x,
                  const float *solid_vel_y) {
    if (!f || !cfg) return;
    float diff = cfg->density_diffusion;
    float visc = cfg->velocity_damping;
    float fdt  = (float)dt;
    int iterations = solver_iterations_from_config(cfg);

    enforce_solids_if_needed(f, solid_mask, solid_vel_x, solid_vel_y);

    ts_start_timer("1st_diffuse");
    swap_buffers(&f->velX_prev, &f->velX);
    diffuse(f, 1, f->velX, f->velX_prev, visc, fdt, iterations, flows);

    swap_buffers(&f->velY_prev, &f->velY);
    diffuse(f, 2, f->velY, f->velY_prev, visc, fdt, iterations, flows);
    ts_stop_timer("1st_diffuse");

    ts_start_timer("proj_advect");
    enforce_solids_if_needed(f, solid_mask, solid_vel_x, solid_vel_y);
    project(f, f->velX, f->velY, f->velX_prev, f->velY_prev, iterations, flows);
    enforce_outflow_direction(f, cfg, flows);

    swap_buffers(&f->velX_prev, &f->velX);
    swap_buffers(&f->velY_prev, &f->velY);
    advect(f, 1, f->velX, f->velX_prev, f->velX_prev, f->velY_prev, fdt, flows);
    advect(f, 2, f->velY, f->velY_prev, f->velX_prev, f->velY_prev, fdt, flows);

    enforce_solids_if_needed(f, solid_mask, solid_vel_x, solid_vel_y);
    project(f, f->velX, f->velY, f->velX_prev, f->velY_prev, iterations, flows);
    enforce_outflow_direction(f, cfg, flows);
    ts_stop_timer("proj_advect");

    ts_start_timer("buoyancy");
    apply_buoyancy(f, cfg, fdt);
    enforce_solids_if_needed(f, solid_mask, solid_vel_x, solid_vel_y);
    project(f, f->velX, f->velY, f->velX_prev, f->velY_prev, iterations, flows);
    enforce_outflow_direction(f, cfg, flows);

    swap_buffers(&f->density_prev, &f->density);
    diffuse(f, 0, f->density, f->density_prev, diff, fdt, iterations, flows);

    swap_buffers(&f->density_prev, &f->density);
    advect(f, 0, f->density, f->density_prev, f->velX, f->velY, fdt, flows);
    ts_stop_timer("buoyancy");
    enforce_solids_if_needed(f, solid_mask, solid_vel_x, solid_vel_y);

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
