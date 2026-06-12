#include "app/sim_runtime_backend_3d_runtime.h"

#include "app/scene_state.h"
#include "app/sim_runtime_3d_solver_core_sim.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static const int SCAFFOLD_SOLVER_REGION_PADDING = 2;
static const int SCAFFOLD_SOLVER_OBSTACLE_BRICK_MARGIN = 1;
enum {
    SCAFFOLD_MAX_SOLVER_CLUSTERS = 64,
};

typedef struct SimRuntime3DSolverCluster {
    SimRuntime3DBrickRegion active_region;
    SimRuntime3DBrickRegion solver_region;
    size_t active_region_cell_count;
    size_t solver_region_cell_count;
} SimRuntime3DSolverCluster;

static size_t backend_3d_scaffold_obstacle_brick_index(const SimRuntimeBackend3DScaffold *state,
                                                       int brick_x,
                                                       int brick_y,
                                                       int brick_z) {
    return ((size_t)brick_z * (size_t)state->brick_store.bricks_h + (size_t)brick_y) *
               (size_t)state->brick_store.bricks_w +
           (size_t)brick_x;
}

static bool backend_3d_scaffold_region_cell_count(const SimRuntime3DBrickRegion *region,
                                                  size_t *out_cell_count) {
    size_t region_w = 0;
    size_t region_h = 0;
    size_t region_d = 0;
    if (!region || !out_cell_count) return false;
    if (region->max_x < region->min_x ||
        region->max_y < region->min_y ||
        region->max_z < region->min_z) {
        return false;
    }
    region_w = (size_t)(region->max_x - region->min_x + 1);
    region_h = (size_t)(region->max_y - region->min_y + 1);
    region_d = (size_t)(region->max_z - region->min_z + 1);
    if (region_w > SIZE_MAX / region_h) return false;
    if (region_w * region_h > SIZE_MAX / region_d) return false;
    *out_cell_count = region_w * region_h * region_d;
    return true;
}

static void backend_3d_scaffold_expand_region_for_wind_corridor(
    const SimRuntimeBackend3DScaffold *state,
    SimRuntime3DBrickRegion *region) {
    if (!state || !region || !state->wind_tunnel_active ||
        !wind_tunnel_3d_config_validate(&state->wind_tunnel)) {
        return;
    }

    switch (state->wind_tunnel.inlet_face) {
    case WIND_TUNNEL_3D_FACE_LEFT:
    case WIND_TUNNEL_3D_FACE_RIGHT:
        if (state->wind_tunnel.outlet_face == WIND_TUNNEL_3D_FACE_LEFT ||
            state->wind_tunnel.outlet_face == WIND_TUNNEL_3D_FACE_RIGHT) {
            region->min_x = 0;
            region->max_x = state->volume.desc.grid_w - 1;
            region->min_y = 0;
            region->max_y = state->volume.desc.grid_h - 1;
            region->min_z = 0;
            region->max_z = state->volume.desc.grid_d - 1;
        }
        break;
    case WIND_TUNNEL_3D_FACE_BOTTOM:
    case WIND_TUNNEL_3D_FACE_TOP:
        if (state->wind_tunnel.outlet_face == WIND_TUNNEL_3D_FACE_BOTTOM ||
            state->wind_tunnel.outlet_face == WIND_TUNNEL_3D_FACE_TOP) {
            region->min_x = 0;
            region->max_x = state->volume.desc.grid_w - 1;
            region->min_y = 0;
            region->max_y = state->volume.desc.grid_h - 1;
            region->min_z = 0;
            region->max_z = state->volume.desc.grid_d - 1;
        }
        break;
    case WIND_TUNNEL_3D_FACE_FRONT:
    case WIND_TUNNEL_3D_FACE_BACK:
        if (state->wind_tunnel.outlet_face == WIND_TUNNEL_3D_FACE_FRONT ||
            state->wind_tunnel.outlet_face == WIND_TUNNEL_3D_FACE_BACK) {
            region->min_x = 0;
            region->max_x = state->volume.desc.grid_w - 1;
            region->min_y = 0;
            region->max_y = state->volume.desc.grid_h - 1;
            region->min_z = 0;
            region->max_z = state->volume.desc.grid_d - 1;
        }
        break;
    case WIND_TUNNEL_3D_FACE_NONE:
    default:
        break;
    }
}

static bool backend_3d_scaffold_regions_overlap(const SimRuntime3DBrickRegion *a,
                                                const SimRuntime3DBrickRegion *b) {
    if (!a || !b) return false;
    return !(a->max_x < b->min_x || b->max_x < a->min_x ||
             a->max_y < b->min_y || b->max_y < a->min_y ||
             a->max_z < b->min_z || b->max_z < a->min_z);
}

static bool backend_3d_scaffold_wind_carrier_velocity(const SimRuntimeBackend3DScaffold *state,
                                                      float *out_vx,
                                                      float *out_vy,
                                                      float *out_vz) {
    float speed = 0.0f;
    if (out_vx) *out_vx = 0.0f;
    if (out_vy) *out_vy = 0.0f;
    if (out_vz) *out_vz = 0.0f;
    if (!state || !state->wind_tunnel_active ||
        !wind_tunnel_3d_config_validate(&state->wind_tunnel)) {
        return false;
    }
    speed = state->wind_tunnel.inflow_speed;
    switch (state->wind_tunnel.inlet_face) {
    case WIND_TUNNEL_3D_FACE_LEFT:
        if (state->wind_tunnel.outlet_face != WIND_TUNNEL_3D_FACE_RIGHT) return false;
        if (out_vx) *out_vx = speed;
        return true;
    case WIND_TUNNEL_3D_FACE_RIGHT:
        if (state->wind_tunnel.outlet_face != WIND_TUNNEL_3D_FACE_LEFT) return false;
        if (out_vx) *out_vx = -speed;
        return true;
    case WIND_TUNNEL_3D_FACE_BOTTOM:
        if (state->wind_tunnel.outlet_face != WIND_TUNNEL_3D_FACE_TOP) return false;
        if (out_vy) *out_vy = speed;
        return true;
    case WIND_TUNNEL_3D_FACE_TOP:
        if (state->wind_tunnel.outlet_face != WIND_TUNNEL_3D_FACE_BOTTOM) return false;
        if (out_vy) *out_vy = -speed;
        return true;
    case WIND_TUNNEL_3D_FACE_FRONT:
        if (state->wind_tunnel.outlet_face != WIND_TUNNEL_3D_FACE_BACK) return false;
        if (out_vz) *out_vz = speed;
        return true;
    case WIND_TUNNEL_3D_FACE_BACK:
        if (state->wind_tunnel.outlet_face != WIND_TUNNEL_3D_FACE_FRONT) return false;
        if (out_vz) *out_vz = -speed;
        return true;
    case WIND_TUNNEL_3D_FACE_NONE:
    default:
        return false;
    }
}

static bool backend_3d_scaffold_wind_axis(float vx,
                                          float vy,
                                          float vz,
                                          int *out_axis,
                                          int *out_sign,
                                          float *out_speed) {
    float ax = fabsf(vx);
    float ay = fabsf(vy);
    float az = fabsf(vz);
    int axis = 0;
    float speed = ax;
    float value = vx;
    if (ay > speed) {
        axis = 1;
        speed = ay;
        value = vy;
    }
    if (az > speed) {
        axis = 2;
        speed = az;
        value = vz;
    }
    if (speed <= 0.0f) return false;
    if (out_axis) *out_axis = axis;
    if (out_sign) *out_sign = value >= 0.0f ? 1 : -1;
    if (out_speed) *out_speed = speed;
    return true;
}

static bool backend_3d_scaffold_solver_solid_at(const SimRuntime3DVolume *volume,
                                                const uint8_t *solid_mask,
                                                int x,
                                                int y,
                                                int z) {
    if (!volume || !solid_mask) return false;
    if (x < 0 || x >= volume->desc.grid_w ||
        y < 0 || y >= volume->desc.grid_h ||
        z < 0 || z >= volume->desc.grid_d) {
        return false;
    }
    return solid_mask[sim_runtime_3d_volume_index(&volume->desc, x, y, z)] != 0u;
}

static void backend_3d_scaffold_apply_wind_wake_modifier(const SimRuntime3DVolume *volume,
                                                         const uint8_t *solid_mask,
                                                         int x,
                                                         int y,
                                                         int z,
                                                         int axis,
                                                         int sign,
                                                         float speed,
                                                         float *io_vx,
                                                         float *io_vy,
                                                         float *io_vz,
                                                         float *io_pressure) {
    float velocity_scale = 1.0f;
    float swirl_a = 0.0f;
    float swirl_b = 0.0f;
    float pressure_lift = 0.0f;
    if (!volume || !solid_mask || !io_vx || !io_vy || !io_vz || speed <= 0.0f) return;

    for (int depth = 1; depth <= 5; ++depth) {
        int upstream_x = x;
        int upstream_y = y;
        int upstream_z = z;
        if (axis == 0) upstream_x -= sign * depth;
        if (axis == 1) upstream_y -= sign * depth;
        if (axis == 2) upstream_z -= sign * depth;
        for (int offset_b = -2; offset_b <= 2; ++offset_b) {
            for (int offset_a = -2; offset_a <= 2; ++offset_a) {
                int sx = upstream_x;
                int sy = upstream_y;
                int sz = upstream_z;
                int abs_a = abs(offset_a);
                int abs_b = abs(offset_b);
                int max_abs = abs_a > abs_b ? abs_a : abs_b;
                float depth_weight = (float)(6 - depth) / 5.0f;
                float lateral_weight = (float)(3 - max_abs) / 3.0f;
                float influence = 0.0f;
                int vortex_a = offset_a;
                int vortex_b = offset_b;
                if (lateral_weight <= 0.0f) continue;
                if (axis == 0) {
                    sy += offset_a;
                    sz += offset_b;
                } else if (axis == 1) {
                    sx += offset_a;
                    sz += offset_b;
                } else {
                    sx += offset_a;
                    sy += offset_b;
                }
                if (!backend_3d_scaffold_solver_solid_at(volume, solid_mask, sx, sy, sz)) continue;
                influence = depth_weight * lateral_weight;
                {
                    float candidate_scale = 1.0f - 0.78f * influence;
                    if (candidate_scale < velocity_scale) velocity_scale = candidate_scale;
                }
                if (vortex_a == 0 && vortex_b == 0) {
                    vortex_a = ((x + y + z + depth) & 1) ? 1 : -1;
                    vortex_b = ((x + z + depth) & 1) ? -1 : 1;
                }
                swirl_a += (float)(-vortex_b) * influence;
                swirl_b += (float)(vortex_a) * influence;
                pressure_lift += influence;
            }
        }
    }

    if (velocity_scale >= 0.999f && fabsf(swirl_a) < 0.0001f && fabsf(swirl_b) < 0.0001f) return;
    if (velocity_scale < 0.12f) velocity_scale = 0.12f;
    if (axis == 0) {
        *io_vx *= velocity_scale;
        *io_vy += swirl_a * speed * 0.12f;
        *io_vz += swirl_b * speed * 0.12f;
    } else if (axis == 1) {
        *io_vy *= velocity_scale;
        *io_vx += swirl_a * speed * 0.12f;
        *io_vz += swirl_b * speed * 0.12f;
    } else {
        *io_vz *= velocity_scale;
        *io_vx += swirl_a * speed * 0.12f;
        *io_vy += swirl_b * speed * 0.12f;
    }
    if (io_pressure && pressure_lift > 0.0f) {
        *io_pressure += pressure_lift * speed * speed * 0.0015f;
    }
}

static void backend_3d_scaffold_apply_wind_carrier_transport(SimRuntimeBackend3DScaffold *state,
                                                             double dt) {
    float vx = 0.0f;
    float vy = 0.0f;
    float vz = 0.0f;
    float voxel_size = 1.0f;
    float dt_cells = 0.0f;
    int wind_axis = 0;
    int wind_sign = 1;
    float wind_speed = 0.0f;
    if (!state || !state->solver_volume.density || !state->solver_scratch.density_prev ||
        !backend_3d_scaffold_wind_carrier_velocity(state, &vx, &vy, &vz) || dt <= 0.0) {
        return;
    }
    if (!backend_3d_scaffold_wind_axis(vx, vy, vz, &wind_axis, &wind_sign, &wind_speed)) return;
    voxel_size = state->solver_volume.desc.voxel_size > 0.0f
                     ? state->solver_volume.desc.voxel_size
                     : 1.0f;
    dt_cells = (float)dt / voxel_size;
    if (state->runtime_solver_max_velocity_displacement_cells_limit > 0.0f) {
        float max_displacement = state->runtime_solver_max_velocity_displacement_cells_limit;
        float dx = vx * dt_cells;
        float dy = vy * dt_cells;
        float dz = vz * dt_cells;
        float displacement_sq = dx * dx + dy * dy + dz * dz;
        if (displacement_sq > max_displacement * max_displacement && displacement_sq > 0.0f) {
            float scale = max_displacement / sqrtf(displacement_sq);
            vx *= scale;
            vy *= scale;
            vz *= scale;
        }
    }

    memcpy(state->solver_scratch.density_prev,
           state->solver_volume.density,
           state->solver_volume.desc.cell_count * sizeof(float));

    for (int z = 0; z < state->solver_volume.desc.grid_d; ++z) {
        for (int y = 0; y < state->solver_volume.desc.grid_h; ++y) {
            for (int x = 0; x < state->solver_volume.desc.grid_w; ++x) {
                size_t idx = sim_runtime_3d_volume_index(&state->solver_volume.desc, x, y, z);
                float cell_vx = vx;
                float cell_vy = vy;
                float cell_vz = vz;
                float cell_pressure = state->solver_volume.pressure[idx];
                float sample_x = 0.0f;
                float sample_y = 0.0f;
                float sample_z = 0.0f;
                if (state->solver_solid_mask && state->solver_solid_mask[idx]) {
                    state->solver_volume.density[idx] = 0.0f;
                    state->solver_volume.velocity_x[idx] = 0.0f;
                    state->solver_volume.velocity_y[idx] = 0.0f;
                    state->solver_volume.velocity_z[idx] = 0.0f;
                    continue;
                }
                backend_3d_scaffold_apply_wind_wake_modifier(&state->solver_volume,
                                                             state->solver_solid_mask,
                                                             x,
                                                             y,
                                                             z,
                                                             wind_axis,
                                                             wind_sign,
                                                             wind_speed,
                                                             &cell_vx,
                                                             &cell_vy,
                                                             &cell_vz,
                                                             &cell_pressure);
                sample_x = (float)x - cell_vx * dt_cells;
                sample_y = (float)y - cell_vy * dt_cells;
                sample_z = (float)z - cell_vz * dt_cells;
                state->solver_volume.density[idx] =
                    sim_runtime_3d_sample_field_trilinear(state->solver_scratch.density_prev,
                                                          &state->solver_volume.desc,
                                                          sample_x,
                                                          sample_y,
                                                          sample_z);
                state->solver_volume.velocity_x[idx] = cell_vx;
                state->solver_volume.velocity_y[idx] = cell_vy;
                state->solver_volume.velocity_z[idx] = cell_vz;
                state->solver_volume.pressure[idx] = cell_pressure;
            }
        }
    }

    {
        int slab = state->wind_tunnel.inlet_slab_cells > 0
                       ? state->wind_tunnel.inlet_slab_cells
                       : 1;
        int min_x = 0;
        int max_x = state->solver_volume.desc.grid_w;
        int min_y = 0;
        int max_y = state->solver_volume.desc.grid_h;
        int min_z = 0;
        int max_z = state->solver_volume.desc.grid_d;
        if (state->wind_tunnel.inlet_face == WIND_TUNNEL_3D_FACE_LEFT) {
            if (slab < max_x) max_x = slab;
        } else if (state->wind_tunnel.inlet_face == WIND_TUNNEL_3D_FACE_RIGHT) {
            if (slab < max_x) min_x = max_x - slab;
        } else if (state->wind_tunnel.inlet_face == WIND_TUNNEL_3D_FACE_BOTTOM) {
            if (slab < max_y) max_y = slab;
        } else if (state->wind_tunnel.inlet_face == WIND_TUNNEL_3D_FACE_TOP) {
            if (slab < max_y) min_y = max_y - slab;
        } else if (state->wind_tunnel.inlet_face == WIND_TUNNEL_3D_FACE_FRONT) {
            if (slab < max_z) max_z = slab;
        } else if (state->wind_tunnel.inlet_face == WIND_TUNNEL_3D_FACE_BACK) {
            if (slab < max_z) min_z = max_z - slab;
        }
        for (int z = min_z; z < max_z; ++z) {
            for (int y = min_y; y < max_y; ++y) {
                for (int x = min_x; x < max_x; ++x) {
                    size_t idx = sim_runtime_3d_volume_index(&state->solver_volume.desc, x, y, z);
                    if (state->solver_solid_mask && state->solver_solid_mask[idx]) continue;
                    state->solver_volume.density[idx] = state->wind_tunnel.inflow_density;
                    state->solver_volume.velocity_x[idx] = vx;
                    state->solver_volume.velocity_y[idx] = vy;
                    state->solver_volume.velocity_z[idx] = vz;
                }
            }
        }
    }
}

static void backend_3d_scaffold_region_union(SimRuntime3DBrickRegion *dst,
                                             const SimRuntime3DBrickRegion *src) {
    if (!dst || !src) return;
    if (src->min_x < dst->min_x) dst->min_x = src->min_x;
    if (src->min_y < dst->min_y) dst->min_y = src->min_y;
    if (src->min_z < dst->min_z) dst->min_z = src->min_z;
    if (src->max_x > dst->max_x) dst->max_x = src->max_x;
    if (src->max_y > dst->max_y) dst->max_y = src->max_y;
    if (src->max_z > dst->max_z) dst->max_z = src->max_z;
}

static void backend_3d_scaffold_region_align_to_bricks(SimRuntimeBackend3DScaffold *state,
                                                       SimRuntime3DBrickRegion *region) {
    int brick_size = 0;
    if (!state || !region || state->brick_store.brick_size <= 0) return;
    brick_size = state->brick_store.brick_size;
    region->min_x = (region->min_x / brick_size) * brick_size;
    region->min_y = (region->min_y / brick_size) * brick_size;
    region->min_z = (region->min_z / brick_size) * brick_size;
    region->max_x = ((region->max_x / brick_size) + 1) * brick_size - 1;
    region->max_y = ((region->max_y / brick_size) + 1) * brick_size - 1;
    region->max_z = ((region->max_z / brick_size) + 1) * brick_size - 1;
    if (region->max_x >= state->volume.desc.grid_w) region->max_x = state->volume.desc.grid_w - 1;
    if (region->max_y >= state->volume.desc.grid_h) region->max_y = state->volume.desc.grid_h - 1;
    if (region->max_z >= state->volume.desc.grid_d) region->max_z = state->volume.desc.grid_d - 1;
}

static void backend_3d_scaffold_expand_region_to_nearby_obstacle_bricks(
    SimRuntimeBackend3DScaffold *state,
    SimRuntime3DBrickRegion *region,
    int brick_margin) {
    int brick_size = 0;
    int min_brick_x = 0;
    int min_brick_y = 0;
    int min_brick_z = 0;
    int max_brick_x = 0;
    int max_brick_y = 0;
    int max_brick_z = 0;
    if (!state || !region || !state->obstacle_brick_flags || state->brick_store.brick_size <= 0) return;
    brick_size = state->brick_store.brick_size;
    min_brick_x = region->min_x / brick_size;
    min_brick_y = region->min_y / brick_size;
    min_brick_z = region->min_z / brick_size;
    max_brick_x = region->max_x / brick_size;
    max_brick_y = region->max_y / brick_size;
    max_brick_z = region->max_z / brick_size;

    for (int brick_z = 0; brick_z < state->brick_store.bricks_d; ++brick_z) {
        for (int brick_y = 0; brick_y < state->brick_store.bricks_h; ++brick_y) {
            for (int brick_x = 0; brick_x < state->brick_store.bricks_w; ++brick_x) {
                int brick_min_x = 0;
                int brick_min_y = 0;
                int brick_min_z = 0;
                int brick_max_x = 0;
                int brick_max_y = 0;
                int brick_max_z = 0;
                size_t brick_index =
                    backend_3d_scaffold_obstacle_brick_index(state, brick_x, brick_y, brick_z);
                if (!state->obstacle_brick_flags[brick_index]) continue;
                if (brick_x < min_brick_x - brick_margin || brick_x > max_brick_x + brick_margin ||
                    brick_y < min_brick_y - brick_margin || brick_y > max_brick_y + brick_margin ||
                    brick_z < min_brick_z - brick_margin || brick_z > max_brick_z + brick_margin) {
                    continue;
                }
                brick_min_x = brick_x * brick_size;
                brick_min_y = brick_y * brick_size;
                brick_min_z = brick_z * brick_size;
                brick_max_x = brick_min_x + brick_size - 1;
                brick_max_y = brick_min_y + brick_size - 1;
                brick_max_z = brick_min_z + brick_size - 1;
                if (brick_max_x >= state->volume.desc.grid_w) brick_max_x = state->volume.desc.grid_w - 1;
                if (brick_max_y >= state->volume.desc.grid_h) brick_max_y = state->volume.desc.grid_h - 1;
                if (brick_max_z >= state->volume.desc.grid_d) brick_max_z = state->volume.desc.grid_d - 1;
                if (brick_min_x < region->min_x) region->min_x = brick_min_x;
                if (brick_min_y < region->min_y) region->min_y = brick_min_y;
                if (brick_min_z < region->min_z) region->min_z = brick_min_z;
                if (brick_max_x > region->max_x) region->max_x = brick_max_x;
                if (brick_max_y > region->max_y) region->max_y = brick_max_y;
                if (brick_max_z > region->max_z) region->max_z = brick_max_z;
            }
        }
    }
}

static bool backend_3d_scaffold_sync_dense_mirror_region(SimRuntimeBackend3DScaffold *state,
                                                         const SimRuntime3DBrickRegion *region,
                                                         const SimRuntime3DVolume *region_volume) {
    if (!backend_3d_scaffold_dense_mirror_live(state) || !region || !region_volume) return true;
    for (int z = region->min_z; z <= region->max_z; ++z) {
        for (int y = region->min_y; y <= region->max_y; ++y) {
            for (int x = region->min_x; x <= region->max_x; ++x) {
                size_t dst_idx = sim_runtime_3d_volume_index(&state->volume.desc, x, y, z);
                size_t src_idx = sim_runtime_3d_volume_index(&region_volume->desc,
                                                             x - region->min_x,
                                                             y - region->min_y,
                                                             z - region->min_z);
                state->volume.density[dst_idx] = region_volume->density[src_idx];
                state->volume.velocity_x[dst_idx] = region_volume->velocity_x[src_idx];
                state->volume.velocity_y[dst_idx] = region_volume->velocity_y[src_idx];
                state->volume.velocity_z[dst_idx] = region_volume->velocity_z[src_idx];
                state->volume.pressure[dst_idx] = region_volume->pressure[src_idx];
            }
        }
    }
    return true;
}

static bool backend_3d_scaffold_prepare_solver_region(SimRuntimeBackend3DScaffold *state,
                                                      const SimRuntime3DBrickRegion *region) {
    SimRuntime3DDomainDesc solver_desc = {0};
    if (!state || !region) return false;
    if (!sim_runtime_3d_brick_region_desc(&state->volume.desc, region, &solver_desc)) return false;
    if (state->solver_volume.desc.grid_w != solver_desc.grid_w ||
        state->solver_volume.desc.grid_h != solver_desc.grid_h ||
        state->solver_volume.desc.grid_d != solver_desc.grid_d ||
        !state->solver_volume.density) {
        sim_runtime_3d_volume_destroy(&state->solver_volume);
        sim_runtime_3d_solver_scratch_destroy(&state->solver_scratch);
        free(state->solver_solid_mask);
        state->solver_solid_mask = NULL;
        if (!sim_runtime_3d_volume_init(&state->solver_volume, &solver_desc)) return false;
        if (!sim_runtime_3d_solver_scratch_init(&state->solver_scratch, &solver_desc)) return false;
        state->solver_solid_mask = (uint8_t *)calloc(solver_desc.cell_count, sizeof(uint8_t));
        if (!state->solver_solid_mask) return false;
    } else {
        state->solver_volume.desc = solver_desc;
        state->solver_scratch.desc = solver_desc;
    }
    if (!sim_runtime_3d_brick_store_materialize_region(&state->brick_store,
                                                       region,
                                                       &state->solver_volume)) {
        return false;
    }
    if (!backend_3d_scaffold_obstacle_materialize_region(state,
                                                         region,
                                                         state->solver_solid_mask,
                                                         solver_desc.cell_count)) {
        return false;
    }
    return true;
}

size_t backend_3d_scaffold_runtime_default_solver_region_cell_budget(void) {
    return app_config_3d_solver_region_cell_budget(NULL);
}

void backend_3d_scaffold_runtime_reset_metrics(SimRuntimeBackend3DScaffold *state) {
    if (!state) return;
    state->runtime_solver_region_guard_triggered = false;
    state->runtime_solver_cluster_limit_reached = false;
    state->runtime_last_active_region_cell_count = 0u;
    state->runtime_last_solver_region_cell_count = 0u;
    state->runtime_last_solver_cluster_count = 0u;
    state->runtime_last_solver_max_cluster_cell_count = 0u;
    state->runtime_last_solver_solved_cluster_count = 0u;
    state->runtime_last_solver_skipped_cluster_count = 0u;
    state->runtime_last_solver_skipped_solver_cell_count = 0u;
    state->runtime_export_cache_materialization_count = 0u;
    state->runtime_solver_velocity_clamp_cell_count = 0u;
    state->runtime_solver_max_velocity_magnitude_pre_clamp = 0.0f;
    state->runtime_solver_max_velocity_magnitude_post_clamp = 0.0f;
    state->runtime_solver_max_velocity_displacement_cells_pre_clamp = 0.0f;
    state->runtime_solver_max_velocity_displacement_cells_post_clamp = 0.0f;
    state->runtime_solver_max_abs_divergence_after_project = 0.0f;
}

void backend_3d_scaffold_runtime_note_export_cache_materialized(SimRuntimeBackend3DScaffold *state) {
    if (!state) return;
    state->runtime_export_cache_materialization_count += 1u;
}

bool backend_3d_scaffold_runtime_step(SimRuntimeBackend *backend,
                                      struct SceneState *scene,
                                      const AppConfig *cfg,
                                      double dt [[fisics::dim(time)]] [[fisics::unit(second)]]) {
    SimRuntimeBackend3DScaffold *state = backend ? (SimRuntimeBackend3DScaffold *)backend->impl : NULL;
    SimRuntime3DForceAxis scene_up_axis = {0};
    SimRuntime3DBrickRegion raw_regions[SCAFFOLD_MAX_SOLVER_CLUSTERS] = {0};
    SimRuntime3DSolverCluster clusters[SCAFFOLD_MAX_SOLVER_CLUSTERS] = {0};
    size_t raw_region_count = 0u;
    bool cluster_limit_reached = false;
    double zero_seconds [[fisics::dim(time)]] [[fisics::unit(second)]] = 0.0;
    (void)scene;
    if (state && state->obstacle_volume_dirty) {
        backend_3d_scaffold_build_obstacles(backend, scene);
    }
    if (!state || !cfg || dt <= zero_seconds) return false;
    state->runtime_solver_region_guard_triggered = false;
    state->runtime_solver_cluster_limit_reached = false;
    state->runtime_last_active_region_cell_count = 0u;
    state->runtime_last_solver_region_cell_count = 0u;
    state->runtime_last_solver_cluster_count = 0u;
    state->runtime_last_solver_max_cluster_cell_count = 0u;
    state->runtime_last_solver_solved_cluster_count = 0u;
    state->runtime_last_solver_skipped_cluster_count = 0u;
    state->runtime_last_solver_skipped_solver_cell_count = 0u;
    state->runtime_solver_region_cell_budget = app_config_3d_solver_region_cell_budget(cfg);
    state->runtime_solver_region_cell_budget_overridden =
        app_config_3d_solver_region_cell_budget_overridden(cfg);
    state->runtime_solver_max_velocity_displacement_cells_limit =
        app_config_3d_max_velocity_displacement_cells(cfg);
    state->runtime_solver_max_velocity_displacement_cells_limit_overridden =
        app_config_3d_max_velocity_displacement_cells_overridden(cfg);
    state->runtime_solver_velocity_clamp_cell_count = 0u;
    state->runtime_solver_max_velocity_magnitude_pre_clamp = 0.0f;
    state->runtime_solver_max_velocity_magnitude_post_clamp = 0.0f;
    state->runtime_solver_max_velocity_displacement_cells_pre_clamp = 0.0f;
    state->runtime_solver_max_velocity_displacement_cells_post_clamp = 0.0f;
    state->runtime_solver_max_abs_divergence_after_project = 0.0f;
    if (!sim_runtime_3d_brick_store_collect_active_clusters(&state->brick_store,
                                                            raw_regions,
                                                            SCAFFOLD_MAX_SOLVER_CLUSTERS,
                                                            &raw_region_count,
                                                            &cluster_limit_reached)) {
        return true;
    }
    state->runtime_solver_cluster_limit_reached = cluster_limit_reached;
    if (state->scene_up_valid) {
        scene_up_axis.valid = true;
        scene_up_axis.x = state->scene_up_x;
        scene_up_axis.y = state->scene_up_y;
        scene_up_axis.z = state->scene_up_z;
    }

    for (size_t i = 0u; i < raw_region_count; ++i) {
        size_t active_region_cell_count = 0u;
        size_t solver_region_cell_count = 0u;
        clusters[i].active_region = raw_regions[i];
        if (!backend_3d_scaffold_region_cell_count(&clusters[i].active_region,
                                                   &active_region_cell_count)) {
            return false;
        }
        clusters[i].active_region_cell_count = active_region_cell_count;
        clusters[i].solver_region = clusters[i].active_region;
        sim_runtime_3d_brick_region_expand_clamped(&state->volume.desc,
                                                   &clusters[i].solver_region,
                                                   SCAFFOLD_SOLVER_REGION_PADDING);
        backend_3d_scaffold_region_align_to_bricks(state, &clusters[i].solver_region);
        backend_3d_scaffold_expand_region_to_nearby_obstacle_bricks(state,
                                                                    &clusters[i].solver_region,
                                                                    SCAFFOLD_SOLVER_OBSTACLE_BRICK_MARGIN);
        backend_3d_scaffold_region_align_to_bricks(state, &clusters[i].solver_region);
        backend_3d_scaffold_expand_region_for_wind_corridor(state, &clusters[i].solver_region);
        backend_3d_scaffold_region_align_to_bricks(state, &clusters[i].solver_region);
        if (!backend_3d_scaffold_region_cell_count(&clusters[i].solver_region,
                                                   &solver_region_cell_count)) {
            return false;
        }
        clusters[i].solver_region_cell_count = solver_region_cell_count;
    }

    for (size_t i = 0u; i < raw_region_count; ++i) {
        for (size_t j = i + 1u; j < raw_region_count;) {
            if (!backend_3d_scaffold_regions_overlap(&clusters[i].solver_region,
                                                     &clusters[j].solver_region)) {
                j++;
                continue;
            }
            backend_3d_scaffold_region_union(&clusters[i].active_region, &clusters[j].active_region);
            backend_3d_scaffold_region_union(&clusters[i].solver_region, &clusters[j].solver_region);
            clusters[i].active_region_cell_count += clusters[j].active_region_cell_count;
            if (!backend_3d_scaffold_region_cell_count(&clusters[i].solver_region,
                                                       &clusters[i].solver_region_cell_count)) {
                return false;
            }
            clusters[j] = clusters[raw_region_count - 1u];
            raw_region_count--;
        }
    }

    state->runtime_last_solver_cluster_count = raw_region_count;
    for (size_t i = 0u; i < raw_region_count; ++i) {
        state->runtime_last_active_region_cell_count += clusters[i].active_region_cell_count;
        state->runtime_last_solver_region_cell_count += clusters[i].solver_region_cell_count;
        if (clusters[i].solver_region_cell_count > state->runtime_last_solver_max_cluster_cell_count) {
            state->runtime_last_solver_max_cluster_cell_count = clusters[i].solver_region_cell_count;
        }
    }

    for (size_t i = 0u; i < raw_region_count; ++i) {
        if (state->runtime_solver_region_cell_budget > 0u &&
            clusters[i].solver_region_cell_count > state->runtime_solver_region_cell_budget) {
            state->runtime_solver_region_guard_triggered = true;
            state->runtime_last_solver_skipped_cluster_count += 1u;
            state->runtime_last_solver_skipped_solver_cell_count +=
                clusters[i].solver_region_cell_count;
            continue;
        }
        if (!backend_3d_scaffold_prepare_solver_region(state, &clusters[i].solver_region)) {
            return false;
        }
        {
            SimRuntime3DSolverStepMetrics solver_metrics = {0};
            if (!sim_runtime_3d_solver_core_sim_step_first_pass(&state->solver_loop,
                                                                &state->solver_volume,
                                                                &state->solver_scratch,
                                                                state->solver_solid_mask,
                                                                &scene_up_axis,
                                                                cfg,
                                                                dt,
                                                                state->runtime_solver_max_velocity_displacement_cells_limit,
                                                                NULL,
                                                                &solver_metrics)) {
                return false;
            }
            state->runtime_last_solver_solved_cluster_count += 1u;
            state->runtime_solver_velocity_clamp_cell_count +=
                solver_metrics.velocity_clamp_cell_count;
            if (solver_metrics.max_velocity_magnitude_pre_clamp >
                state->runtime_solver_max_velocity_magnitude_pre_clamp) {
                state->runtime_solver_max_velocity_magnitude_pre_clamp =
                    solver_metrics.max_velocity_magnitude_pre_clamp;
            }
            if (solver_metrics.max_velocity_magnitude_post_clamp >
                state->runtime_solver_max_velocity_magnitude_post_clamp) {
                state->runtime_solver_max_velocity_magnitude_post_clamp =
                    solver_metrics.max_velocity_magnitude_post_clamp;
            }
            if (solver_metrics.max_velocity_displacement_cells_pre_clamp >
                state->runtime_solver_max_velocity_displacement_cells_pre_clamp) {
                state->runtime_solver_max_velocity_displacement_cells_pre_clamp =
                    solver_metrics.max_velocity_displacement_cells_pre_clamp;
            }
            if (solver_metrics.max_velocity_displacement_cells_post_clamp >
                state->runtime_solver_max_velocity_displacement_cells_post_clamp) {
                state->runtime_solver_max_velocity_displacement_cells_post_clamp =
                    solver_metrics.max_velocity_displacement_cells_post_clamp;
            }
            if (solver_metrics.max_abs_divergence_after_project >
                state->runtime_solver_max_abs_divergence_after_project) {
                state->runtime_solver_max_abs_divergence_after_project =
                    solver_metrics.max_abs_divergence_after_project;
            }
        }
        backend_3d_scaffold_apply_wind_carrier_transport(state, dt);
        if (!sim_runtime_3d_brick_store_commit_region(&state->brick_store,
                                                      &clusters[i].solver_region,
                                                      &state->solver_volume)) {
            return false;
        }
        if (!backend_3d_scaffold_sync_dense_mirror_region(state,
                                                          &clusters[i].solver_region,
                                                          &state->solver_volume)) {
            return false;
        }
        state->debug_volume_stats_dirty = true;
        state->export_volume_cache_dirty = true;
        state->fluid_slice_dirty = true;
    }
    return true;
}
