#include "physics/fluid2d/fluid2d_boundary.h"

#include <math.h>
#include <stdbool.h>

static inline size_t cell_index(const Fluid2D *grid, int x, int y) {
    return (size_t)y * (size_t)grid->w + (size_t)x;
}

static void boundary_outward_normal(BoundaryFlowEdge edge, float *nx, float *ny) {
    if (!nx || !ny) return;
    switch (edge) {
    case BOUNDARY_EDGE_TOP:    *nx = 0.0f;  *ny = -1.0f; break;
    case BOUNDARY_EDGE_BOTTOM: *nx = 0.0f;  *ny = 1.0f;  break;
    case BOUNDARY_EDGE_LEFT:   *nx = -1.0f; *ny = 0.0f;  break;
    case BOUNDARY_EDGE_RIGHT:  *nx = 1.0f;  *ny = 0.0f;  break;
    default:                   *nx = 0.0f;  *ny = 0.0f;  break;
    }
}

static int boundary_span_end(const Fluid2D *grid, BoundaryFlowEdge edge) {
    if (!grid) return 0;
    return (edge == BOUNDARY_EDGE_TOP || edge == BOUNDARY_EDGE_BOTTOM)
               ? grid->w - 2
               : grid->h - 2;
}

static bool boundary_interior_cell(const Fluid2D *grid,
                                   BoundaryFlowEdge edge,
                                   int span_index,
                                   int depth,
                                   int *out_x,
                                   int *out_y) {
    if (!grid || !out_x || !out_y) return false;
    switch (edge) {
    case BOUNDARY_EDGE_TOP:
        *out_x = span_index;
        *out_y = 1 + depth;
        break;
    case BOUNDARY_EDGE_BOTTOM:
        *out_x = span_index;
        *out_y = grid->h - 2 - depth;
        break;
    case BOUNDARY_EDGE_LEFT:
        *out_x = 1 + depth;
        *out_y = span_index;
        break;
    case BOUNDARY_EDGE_RIGHT:
        *out_x = grid->w - 2 - depth;
        *out_y = span_index;
        break;
    default:
        return false;
    }
    if (*out_x < 1 || *out_x >= grid->w - 1 ||
        *out_y < 1 || *out_y >= grid->h - 1) {
        return false;
    }
    return true;
}

static bool boundary_ghost_cell(const Fluid2D *grid,
                                BoundaryFlowEdge edge,
                                int span_index,
                                int *out_x,
                                int *out_y) {
    if (!grid || !out_x || !out_y) return false;
    switch (edge) {
    case BOUNDARY_EDGE_TOP:
        *out_x = span_index;
        *out_y = 0;
        break;
    case BOUNDARY_EDGE_BOTTOM:
        *out_x = span_index;
        *out_y = grid->h - 1;
        break;
    case BOUNDARY_EDGE_LEFT:
        *out_x = 0;
        *out_y = span_index;
        break;
    case BOUNDARY_EDGE_RIGHT:
        *out_x = grid->w - 1;
        *out_y = span_index;
        break;
    default:
        return false;
    }
    if (*out_x < 0 || *out_x >= grid->w ||
        *out_y < 0 || *out_y >= grid->h) {
        return false;
    }
    return true;
}

static float clamp_positive(float value, float fallback) {
    if (!isfinite(value) || value <= 0.0f) {
        return fallback;
    }
    return value;
}

static float clamp01(float value) {
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

static void wind_clamp_backflow(Fluid2D *grid,
                                BoundaryFlowEdge edge,
                                size_t id) {
    float nx = 0.0f, ny = 0.0f;
    boundary_outward_normal(edge, &nx, &ny);
    float vn = grid->velX[id] * nx + grid->velY[id] * ny;
    if (vn < 0.0f) {
        grid->velX[id] -= vn * nx;
        grid->velY[id] -= vn * ny;
    }
}

static void wind_set_inlet(Fluid2D *grid,
                           BoundaryFlowEdge edge,
                           float inflow_speed,
                           float inflow_density,
                           float vel_mix,
                           float density_mix) {
    if (!grid || inflow_speed <= 0.0f) return;
    int span_end = boundary_span_end(grid, edge);
    if (span_end < 1) return;

    float nx = 0.0f, ny = 0.0f;
    boundary_outward_normal(edge, &nx, &ny);
    float inward_x = -nx;
    float inward_y = -ny;

    float target_vx = inward_x * inflow_speed;
    float target_vy = inward_y * inflow_speed;

    vel_mix = clamp01(vel_mix);
    density_mix = clamp01(density_mix);

    for (int i = 1; i <= span_end; ++i) {
        int ix = 0, iy = 0;
        if (boundary_interior_cell(grid, edge, i, 0, &ix, &iy)) {
            size_t id = cell_index(grid, ix, iy);
            if (vel_mix >= 1.0f) {
                grid->velX[id] = target_vx;
                grid->velY[id] = target_vy;
            } else if (vel_mix > 0.0f) {
                grid->velX[id] = grid->velX[id] * (1.0f - vel_mix) + target_vx * vel_mix;
                grid->velY[id] = grid->velY[id] * (1.0f - vel_mix) + target_vy * vel_mix;
            }
            if (density_mix >= 1.0f) {
                grid->density[id] = inflow_density;
            } else if (density_mix > 0.0f) {
                grid->density[id] = grid->density[id] * (1.0f - density_mix) +
                                    inflow_density * density_mix;
            }
        }

        int gx = 0, gy = 0;
        if (boundary_ghost_cell(grid, edge, i, &gx, &gy)) {
            size_t gid = cell_index(grid, gx, gy);
            grid->velX[gid] = target_vx;
            grid->velY[gid] = target_vy;
            grid->density[gid] = inflow_density;
        }
    }
}

static void wind_set_outlet(Fluid2D *grid,
                            BoundaryFlowEdge edge,
                            float sponge_strength,
                            bool clamp_backflow) {
    if (!grid) return;
    int span_end = boundary_span_end(grid, edge);
    if (span_end < 1) return;

    sponge_strength = clamp01(sponge_strength);
    int layers = 4;

    for (int i = 1; i <= span_end; ++i) {
        int inner_x = 0, inner_y = 0;
        if (boundary_interior_cell(grid, edge, i, 0, &inner_x, &inner_y)) {
            size_t inner_id = cell_index(grid, inner_x, inner_y);
            int gx = 0, gy = 0;
            if (boundary_ghost_cell(grid, edge, i, &gx, &gy)) {
                size_t gid = cell_index(grid, gx, gy);
                grid->velX[gid] = grid->velX[inner_id];
                grid->velY[gid] = grid->velY[inner_id];
                grid->density[gid] = grid->density[inner_id];
            }
        }

        for (int depth = 0; depth < layers; ++depth) {
            int x = 0, y = 0;
            if (!boundary_interior_cell(grid, edge, i, depth, &x, &y)) continue;
            size_t id = cell_index(grid, x, y);
            float layer_weight = (float)(layers - depth) / (float)layers;
            float damp = 1.0f - sponge_strength * layer_weight;
            if (damp < 0.0f) damp = 0.0f;
            grid->velX[id] *= damp;
            grid->velY[id] *= damp;
            grid->density[id] *= damp;
            if (clamp_backflow) {
                wind_clamp_backflow(grid, edge, id);
            }
        }
    }
}


static void apply_emit_edge(Fluid2D *grid,
                            BoundaryFlowEdge edge,
                            float strength,
                            double dt) {
    if (!grid || strength <= 0.0f || dt <= 0.0) return;
    if (grid->w < 3 || grid->h < 3) return;

    int span_end = boundary_span_end(grid, edge);
    if (span_end < 1) return;

    float outward_x = 0.0f, outward_y = 0.0f;
    boundary_outward_normal(edge, &outward_x, &outward_y);
    float inward_x = -outward_x;
    float inward_y = -outward_y;

    float per_cell = strength * (float)dt;
    float falloff = 0.35f;

    for (int i = 1; i <= span_end; ++i) {
        int ix = 0, iy = 0;
        if (boundary_interior_cell(grid, edge, i, 0, &ix, &iy)) {
            size_t id = cell_index(grid, ix, iy);
            grid->density[id] += per_cell * falloff;
            grid->velX[id] += inward_x * per_cell;
            grid->velY[id] += inward_y * per_cell;
        }
        int gx = 0, gy = 0;
        if (boundary_ghost_cell(grid, edge, i, &gx, &gy)) {
            size_t gid = cell_index(grid, gx, gy);
            grid->density[gid] += per_cell;
            grid->velX[gid] = inward_x * per_cell;
            grid->velY[gid] = inward_y * per_cell;
        }
    }
}

static void apply_receive_edge(Fluid2D *grid,
                               BoundaryFlowEdge edge,
                               float strength,
                               double dt) {
    if (!grid || strength <= 0.0f || dt <= 0.0) return;
    if (grid->w < 3 || grid->h < 3) return;
    int span_end = boundary_span_end(grid, edge);
    if (span_end < 1) return;

    float outward_x = 0.0f, outward_y = 0.0f;
    boundary_outward_normal(edge, &outward_x, &outward_y);

    float damp = strength * (float)dt;
    if (damp > 1.0f) damp = 1.0f;
    float keep = 1.0f - damp;
    float pull = strength * 0.4f * (float)dt;

    for (int i = 1; i <= span_end; ++i) {
        for (int depth = 0; depth < 2; ++depth) {
            int x = 0, y = 0;
            if (!boundary_interior_cell(grid, edge, i, depth, &x, &y)) continue;
            size_t id = cell_index(grid, x, y);
            grid->density[id] = fmaxf(grid->density[id] * keep - strength * 0.1f, 0.0f);
            grid->velX[id] = outward_x * pull;
            grid->velY[id] = outward_y * pull;
        }

        int gx = 0, gy = 0;
        if (boundary_ghost_cell(grid, edge, i, &gx, &gy)) {
            size_t boundary_id = cell_index(grid, gx, gy);
            grid->density[boundary_id] = 0.0f;
            grid->velX[boundary_id] = outward_x * pull;
            grid->velY[boundary_id] = outward_y * pull;
        }
    }
}

void fluid2d_boundary_apply(const BoundaryFlow flows[BOUNDARY_EDGE_COUNT],
                            Fluid2D *grid,
                            double dt) {
    if (!grid || !flows || dt <= 0.0) return;
    for (int edge = 0; edge < BOUNDARY_EDGE_COUNT; ++edge) {
        const BoundaryFlow *flow = &flows[edge];
        if (flow->mode == BOUNDARY_FLOW_EMIT) {
            apply_emit_edge(grid, (BoundaryFlowEdge)edge, flow->strength, dt);
        } else if (flow->mode == BOUNDARY_FLOW_RECEIVE) {
            apply_receive_edge(grid, (BoundaryFlowEdge)edge, flow->strength, dt);
        }
    }
}

static void enforce_receiver(Fluid2D *grid, BoundaryFlowEdge edge) {
    if (!grid) return;
    int span_end = boundary_span_end(grid, edge);
    if (span_end < 1) return;

    for (int i = 1; i <= span_end; ++i) {
        int x = 0, y = 0;
        if (!boundary_interior_cell(grid, edge, i, 0, &x, &y)) continue;
        size_t id = cell_index(grid, x, y);
        grid->density[id] *= 0.2f;
        grid->velX[id] *= 0.2f;
        grid->velY[id] *= 0.2f;
    }
}

static void enforce_emitter(Fluid2D *grid, BoundaryFlowEdge edge) {
    if (!grid) return;
    bool horizontal = (edge == BOUNDARY_EDGE_TOP || edge == BOUNDARY_EDGE_BOTTOM);
    int start = 1;
    int end = horizontal ? grid->w - 2 : grid->h - 2;
    int fixed = (edge == BOUNDARY_EDGE_TOP || edge == BOUNDARY_EDGE_LEFT) ? 1 :
                (horizontal ? grid->h - 2 : grid->w - 2);
    if (end < start) return;
    for (int i = start; i <= end; ++i) {
        int x = horizontal ? i : fixed;
        int y = horizontal ? fixed : i;
        int bx = x;
        int by = y;
        if (edge == BOUNDARY_EDGE_TOP) by = 0;
        else if (edge == BOUNDARY_EDGE_BOTTOM) by = grid->h - 1;
        else if (edge == BOUNDARY_EDGE_LEFT) bx = 0;
        else if (edge == BOUNDARY_EDGE_RIGHT) bx = grid->w - 1;
        size_t id = cell_index(grid, x, y);
        size_t bid = cell_index(grid, bx, by);
        grid->density[bid] = grid->density[id];
        grid->velX[bid] = grid->velX[id];
        grid->velY[bid] = grid->velY[id];
    }
}

void fluid2d_boundary_enforce(const BoundaryFlow flows[BOUNDARY_EDGE_COUNT],
                              Fluid2D *grid) {
    if (!grid || !flows) return;
    for (int edge = 0; edge < BOUNDARY_EDGE_COUNT; ++edge) {
        const BoundaryFlow *flow = &flows[edge];
        switch (flow->mode) {
        case BOUNDARY_FLOW_RECEIVE:
            enforce_receiver(grid, (BoundaryFlowEdge)edge);
            break;
        case BOUNDARY_FLOW_EMIT:
            enforce_emitter(grid, (BoundaryFlowEdge)edge);
            break;
        default:
            break;
        }
    }
}

void fluid2d_boundary_apply_wind(const AppConfig *cfg,
                                 const FluidScenePreset *preset,
                                 Fluid2D *grid,
                                 double dt,
                                 float ramp) {
    if (!cfg || !preset || !grid) return;
    const BoundaryFlow *flows = preset->boundary_flows;
    float inflow_density = clamp_positive(cfg->tunnel_inflow_density, 10.0f);
    if (!isfinite(ramp) || ramp < 0.0f) ramp = 0.0f;
    if (ramp > 1.0f) ramp = 1.0f;
    for (int edge = 0; edge < BOUNDARY_EDGE_COUNT; ++edge) {
        const BoundaryFlow *flow = &flows[edge];
        switch (flow->mode) {
        case BOUNDARY_FLOW_EMIT: {
            float speed = clamp_positive(flow->strength, cfg->tunnel_inflow_speed) * ramp;
            float vel_mix = clamp01((float)(dt * 6.0));
            float density_mix = clamp01((float)(dt * 3.0));
            wind_set_inlet(grid,
                           (BoundaryFlowEdge)edge,
                           speed,
                           inflow_density,
                           vel_mix,
                           density_mix);
            break;
        }
        case BOUNDARY_FLOW_RECEIVE: {
            float strength = clamp_positive(flow->strength, cfg->tunnel_inflow_speed);
            float sponge = clamp01((float)(dt * (0.02f * strength + 1.5f)));
            wind_set_outlet(grid,
                            (BoundaryFlowEdge)edge,
                            sponge,
                            true);
            break;
        }
        default:
            break;
        }
    }
}

void fluid2d_boundary_enforce_wind(const AppConfig *cfg,
                                   const FluidScenePreset *preset,
                                   Fluid2D *grid) {
    if (!cfg || !preset || !grid) return;
    const BoundaryFlow *flows = preset->boundary_flows;
    float inflow_density = clamp_positive(cfg->tunnel_inflow_density, 10.0f);
    for (int edge = 0; edge < BOUNDARY_EDGE_COUNT; ++edge) {
        const BoundaryFlow *flow = &flows[edge];
        switch (flow->mode) {
        case BOUNDARY_FLOW_EMIT: {
            float speed = clamp_positive(flow->strength, cfg->tunnel_inflow_speed);
            wind_set_inlet(grid,
                           (BoundaryFlowEdge)edge,
                           speed,
                           inflow_density,
                           1.0f,
                           1.0f);
            break;
        }
        case BOUNDARY_FLOW_RECEIVE:
            wind_set_outlet(grid,
                            (BoundaryFlowEdge)edge,
                            0.0f,
                            true);
            break;
        default:
            break;
        }
    }
}
