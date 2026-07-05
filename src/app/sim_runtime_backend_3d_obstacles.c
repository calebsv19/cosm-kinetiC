#include "app/sim_runtime_backend_3d_scaffold_internal.h"
#include "app/scene_state.h"

#include <stdbool.h>
#include <math.h>
#include <string.h>

static size_t backend_3d_scaffold_obstacle_brick_index(const SimRuntimeBackend3DScaffold *state,
                                                       int brick_x,
                                                       int brick_y,
                                                       int brick_z) {
    return ((size_t)brick_z * (size_t)state->brick_store.bricks_h + (size_t)brick_y) *
               (size_t)state->brick_store.bricks_w +
           (size_t)brick_x;
}

static size_t backend_3d_scaffold_obstacle_brick_cell_index(
    const SimRuntimeBackend3DScaffold *state,
    int x,
    int y,
    int z) {
    size_t local_x = (size_t)(x % state->brick_store.brick_size);
    size_t local_y = (size_t)(y % state->brick_store.brick_size);
    size_t local_z = (size_t)(z % state->brick_store.brick_size);
    return (local_z * (size_t)state->brick_store.brick_size + local_y) *
               (size_t)state->brick_store.brick_size +
           local_x;
}

static uint8_t *backend_3d_scaffold_obstacle_brick_at(SimRuntimeBackend3DScaffold *state,
                                                      int brick_x,
                                                      int brick_y,
                                                      int brick_z) {
    size_t brick_index = 0u;
    if (!state || !state->obstacle_bricks) return NULL;
    if (brick_x < 0 || brick_x >= state->brick_store.bricks_w ||
        brick_y < 0 || brick_y >= state->brick_store.bricks_h ||
        brick_z < 0 || brick_z >= state->brick_store.bricks_d) {
        return NULL;
    }
    brick_index = backend_3d_scaffold_obstacle_brick_index(state, brick_x, brick_y, brick_z);
    return (uint8_t *)state->obstacle_bricks[brick_index];
}

static const uint8_t *backend_3d_scaffold_obstacle_brick_at_const(
    const SimRuntimeBackend3DScaffold *state,
    int brick_x,
    int brick_y,
    int brick_z) {
    size_t brick_index = 0u;
    if (!state || !state->obstacle_bricks) return NULL;
    if (brick_x < 0 || brick_x >= state->brick_store.bricks_w ||
        brick_y < 0 || brick_y >= state->brick_store.bricks_h ||
        brick_z < 0 || brick_z >= state->brick_store.bricks_d) {
        return NULL;
    }
    brick_index = backend_3d_scaffold_obstacle_brick_index(state, brick_x, brick_y, brick_z);
    return (const uint8_t *)state->obstacle_bricks[brick_index];
}

static bool backend_3d_scaffold_obstacle_brick_empty(const SimRuntimeBackend3DScaffold *state,
                                                     const uint8_t *brick_mask) {
    if (!state || !brick_mask) return true;
    for (size_t i = 0; i < state->brick_store.brick_cell_count; ++i) {
        if (brick_mask[i] != 0u) return false;
    }
    return true;
}

static void backend_3d_scaffold_destroy_obstacle_brick(SimRuntimeBackend3DScaffold *state,
                                                       int brick_x,
                                                       int brick_y,
                                                       int brick_z) {
    size_t brick_index = 0u;
    if (!state || !state->obstacle_bricks) return;
    brick_index = backend_3d_scaffold_obstacle_brick_index(state, brick_x, brick_y, brick_z);
    free(state->obstacle_bricks[brick_index]);
    state->obstacle_bricks[brick_index] = NULL;
    if (state->obstacle_brick_flags) {
        state->obstacle_brick_flags[brick_index] = 0u;
    }
}

static uint8_t *backend_3d_scaffold_ensure_obstacle_brick(SimRuntimeBackend3DScaffold *state,
                                                          int brick_x,
                                                          int brick_y,
                                                          int brick_z) {
    size_t brick_index = 0u;
    size_t mask_bytes = 0u;
    uint8_t *brick_mask = NULL;
    if (!state || !state->obstacle_bricks) return NULL;
    brick_mask = backend_3d_scaffold_obstacle_brick_at(state, brick_x, brick_y, brick_z);
    if (brick_mask) return brick_mask;
    if (state->brick_store.brick_cell_count > SIZE_MAX / sizeof(uint8_t)) return NULL;
    mask_bytes = state->brick_store.brick_cell_count * sizeof(uint8_t);
    brick_mask = (uint8_t *)calloc(1, mask_bytes);
    if (!brick_mask) return NULL;
    brick_index = backend_3d_scaffold_obstacle_brick_index(state, brick_x, brick_y, brick_z);
    state->obstacle_bricks[brick_index] = brick_mask;
    if (state->obstacle_brick_flags) {
        state->obstacle_brick_flags[brick_index] = 1u;
    }
    return brick_mask;
}

void backend_3d_scaffold_clear_obstacle_bricks(SimRuntimeBackend3DScaffold *state) {
    if (!state || !state->obstacle_bricks) return;
    for (int brick_z = 0; brick_z < state->brick_store.bricks_d; ++brick_z) {
        for (int brick_y = 0; brick_y < state->brick_store.bricks_h; ++brick_y) {
            for (int brick_x = 0; brick_x < state->brick_store.bricks_w; ++brick_x) {
                backend_3d_scaffold_destroy_obstacle_brick(state, brick_x, brick_y, brick_z);
            }
        }
    }
}

void backend_3d_scaffold_mark_obstacle_cell_cache_dirty(SimRuntimeBackend3DScaffold *state) {
    if (!state) return;
    state->obstacle_dense_cache_dirty = true;
    state->obstacle_slice_dirty = true;
}

void backend_3d_scaffold_mark_obstacle_volume_rebuild_needed(
    SimRuntimeBackend3DScaffold *state) {
    if (!state) return;
    state->obstacle_volume_dirty = true;
    state->obstacle_slice_dirty = true;
    state->export_volume_cache_dirty = true;
}

void backend_3d_scaffold_mark_obstacle_volume_rebuilt(SimRuntimeBackend3DScaffold *state) {
    if (!state) return;
    state->obstacle_volume_dirty = false;
    state->obstacle_dense_cache_dirty = true;
    state->obstacle_slice_dirty = true;
    state->export_volume_cache_dirty = true;
}

static void backend_3d_scaffold_obstacle_brick_bounds(const SimRuntimeBackend3DScaffold *state,
                                                      int brick_x,
                                                      int brick_y,
                                                      int brick_z,
                                                      int *out_min_x,
                                                      int *out_min_y,
                                                      int *out_min_z,
                                                      int *out_max_x,
                                                      int *out_max_y,
                                                      int *out_max_z) {
    int min_x = 0;
    int min_y = 0;
    int min_z = 0;
    int max_x = 0;
    int max_y = 0;
    int max_z = 0;
    if (!state) return;
    min_x = brick_x * state->brick_store.brick_size;
    min_y = brick_y * state->brick_store.brick_size;
    min_z = brick_z * state->brick_store.brick_size;
    max_x = min_x + state->brick_store.brick_size - 1;
    max_y = min_y + state->brick_store.brick_size - 1;
    max_z = min_z + state->brick_store.brick_size - 1;
    if (max_x >= state->volume.desc.grid_w) max_x = state->volume.desc.grid_w - 1;
    if (max_y >= state->volume.desc.grid_h) max_y = state->volume.desc.grid_h - 1;
    if (max_z >= state->volume.desc.grid_d) max_z = state->volume.desc.grid_d - 1;
    if (out_min_x) *out_min_x = min_x;
    if (out_min_y) *out_min_y = min_y;
    if (out_min_z) *out_min_z = min_z;
    if (out_max_x) *out_max_x = max_x;
    if (out_max_y) *out_max_y = max_y;
    if (out_max_z) *out_max_z = max_z;
}

static void backend_3d_scaffold_refresh_obstacle_brick_flag(SimRuntimeBackend3DScaffold *state,
                                                            int brick_x,
                                                            int brick_y,
                                                            int brick_z) {
    uint8_t *brick_mask = NULL;
    size_t brick_index = 0;
    if (!state || !state->obstacle_brick_flags) return;
    if (brick_x < 0 || brick_x >= state->brick_store.bricks_w ||
        brick_y < 0 || brick_y >= state->brick_store.bricks_h ||
        brick_z < 0 || brick_z >= state->brick_store.bricks_d) {
        return;
    }
    brick_index = backend_3d_scaffold_obstacle_brick_index(state, brick_x, brick_y, brick_z);
    brick_mask = backend_3d_scaffold_obstacle_brick_at(state, brick_x, brick_y, brick_z);
    if (!brick_mask || backend_3d_scaffold_obstacle_brick_empty(state, brick_mask)) {
        backend_3d_scaffold_destroy_obstacle_brick(state, brick_x, brick_y, brick_z);
        state->obstacle_brick_flags[brick_index] = 0u;
        return;
    }
    state->obstacle_brick_flags[brick_index] = 1u;
}

void backend_3d_scaffold_set_obstacle_cell(SimRuntimeBackend3DScaffold *state,
                                           int x,
                                           int y,
                                           int z,
                                           bool solid) {
    int brick_x = 0;
    int brick_y = 0;
    int brick_z = 0;
    size_t brick_cell_index = 0u;
    uint8_t *brick_mask = NULL;
    if (!state) return;
    if (x < 0 || x >= state->volume.desc.grid_w ||
        y < 0 || y >= state->volume.desc.grid_h ||
        z < 0 || z >= state->volume.desc.grid_d) {
        return;
    }
    brick_x = x / state->brick_store.brick_size;
    brick_y = y / state->brick_store.brick_size;
    brick_z = z / state->brick_store.brick_size;
    brick_cell_index = backend_3d_scaffold_obstacle_brick_cell_index(state, x, y, z);
    if (solid) {
        brick_mask = backend_3d_scaffold_ensure_obstacle_brick(state, brick_x, brick_y, brick_z);
        if (!brick_mask) return;
        brick_mask[brick_cell_index] = 1u;
        if (state->obstacle_brick_flags) {
            size_t brick_index =
                backend_3d_scaffold_obstacle_brick_index(state, brick_x, brick_y, brick_z);
            state->obstacle_brick_flags[brick_index] = 1u;
        }
    } else {
        brick_mask = backend_3d_scaffold_obstacle_brick_at(state, brick_x, brick_y, brick_z);
        if (!brick_mask) return;
        brick_mask[brick_cell_index] = 0u;
        backend_3d_scaffold_refresh_obstacle_brick_flag(state, brick_x, brick_y, brick_z);
    }
    backend_3d_scaffold_mark_obstacle_cell_cache_dirty(state);
}

bool backend_3d_scaffold_obstacle_cell_solid(const SimRuntimeBackend3DScaffold *state,
                                             int x,
                                             int y,
                                             int z) {
    const uint8_t *brick_mask = NULL;
    int brick_x = 0;
    int brick_y = 0;
    int brick_z = 0;
    size_t brick_cell_index = 0u;
    if (!state) return false;
    if (x < 0 || x >= state->volume.desc.grid_w ||
        y < 0 || y >= state->volume.desc.grid_h ||
        z < 0 || z >= state->volume.desc.grid_d) {
        return false;
    }
    brick_x = x / state->brick_store.brick_size;
    brick_y = y / state->brick_store.brick_size;
    brick_z = z / state->brick_store.brick_size;
    brick_mask = backend_3d_scaffold_obstacle_brick_at_const(state, brick_x, brick_y, brick_z);
    if (!brick_mask) return false;
    brick_cell_index = backend_3d_scaffold_obstacle_brick_cell_index(state, x, y, z);
    return brick_mask[brick_cell_index] != 0u;
}

bool backend_3d_scaffold_obstacle_materialize_region(const SimRuntimeBackend3DScaffold *state,
                                                     const SimRuntime3DBrickRegion *region,
                                                     uint8_t *out_solid_mask,
                                                     size_t out_cell_count) {
    size_t region_w = 0u;
    size_t region_h = 0u;
    size_t region_d = 0u;
    if (!state || !region || !out_solid_mask) return false;
    region_w = (size_t)(region->max_x - region->min_x + 1);
    region_h = (size_t)(region->max_y - region->min_y + 1);
    region_d = (size_t)(region->max_z - region->min_z + 1);
    if (region_w * region_h * region_d != out_cell_count) return false;
    memset(out_solid_mask, 0, out_cell_count * sizeof(uint8_t));
    for (int z = region->min_z; z <= region->max_z; ++z) {
        for (int y = region->min_y; y <= region->max_y; ++y) {
            for (int x = region->min_x; x <= region->max_x; ++x) {
                size_t dst_idx = 0u;
                if (!backend_3d_scaffold_obstacle_cell_solid(state, x, y, z)) continue;
                dst_idx = ((size_t)(z - region->min_z) * region_h + (size_t)(y - region->min_y)) *
                              region_w +
                          (size_t)(x - region->min_x);
                out_solid_mask[dst_idx] = 1u;
            }
        }
    }
    return true;
}

bool backend_3d_scaffold_obstacle_materialize_full(const SimRuntimeBackend3DScaffold *state,
                                                   uint8_t *out_solid_mask,
                                                   size_t out_cell_count) {
    if (!state || !out_solid_mask) return false;
    if (out_cell_count != state->volume.desc.cell_count) return false;
    memset(out_solid_mask, 0, out_cell_count * sizeof(uint8_t));
    for (int brick_z = 0; brick_z < state->brick_store.bricks_d; ++brick_z) {
        for (int brick_y = 0; brick_y < state->brick_store.bricks_h; ++brick_y) {
            for (int brick_x = 0; brick_x < state->brick_store.bricks_w; ++brick_x) {
                const uint8_t *brick_mask = backend_3d_scaffold_obstacle_brick_at_const(
                    state, brick_x, brick_y, brick_z);
                int min_x = 0;
                int min_y = 0;
                int min_z = 0;
                int max_x = 0;
                int max_y = 0;
                int max_z = 0;
                if (!brick_mask) continue;
                backend_3d_scaffold_obstacle_brick_bounds(state,
                                                          brick_x,
                                                          brick_y,
                                                          brick_z,
                                                          &min_x,
                                                          &min_y,
                                                          &min_z,
                                                          &max_x,
                                                          &max_y,
                                                          &max_z);
                for (int z = min_z; z <= max_z; ++z) {
                    for (int y = min_y; y <= max_y; ++y) {
                        for (int x = min_x; x <= max_x; ++x) {
                            size_t brick_cell_index =
                                backend_3d_scaffold_obstacle_brick_cell_index(state, x, y, z);
                            if (!brick_mask[brick_cell_index]) continue;
                            out_solid_mask[sim_runtime_3d_volume_index(&state->volume.desc, x, y, z)] = 1u;
                        }
                    }
                }
            }
        }
    }
    return true;
}

bool backend_3d_scaffold_obstacle_fill_slice_xy(const SimRuntimeBackend3DScaffold *state,
                                                int z,
                                                uint8_t *out_solid_mask,
                                                size_t out_cell_count) {
    int brick_z = 0;
    if (!state || !out_solid_mask) return false;
    if (z < 0 || z >= state->volume.desc.grid_d) return false;
    if (out_cell_count != state->volume.desc.slice_cell_count) return false;
    memset(out_solid_mask, 0, out_cell_count * sizeof(uint8_t));
    brick_z = z / state->brick_store.brick_size;
    for (int brick_y = 0; brick_y < state->brick_store.bricks_h; ++brick_y) {
        for (int brick_x = 0; brick_x < state->brick_store.bricks_w; ++brick_x) {
            const uint8_t *brick_mask = backend_3d_scaffold_obstacle_brick_at_const(
                state, brick_x, brick_y, brick_z);
            int min_x = 0;
            int min_y = 0;
            int min_z = 0;
            int max_x = 0;
            int max_y = 0;
            int max_z = 0;
            if (!brick_mask) continue;
            backend_3d_scaffold_obstacle_brick_bounds(state,
                                                      brick_x,
                                                      brick_y,
                                                      brick_z,
                                                      &min_x,
                                                      &min_y,
                                                      &min_z,
                                                      &max_x,
                                                      &max_y,
                                                      &max_z);
            (void)min_z;
            (void)max_z;
            for (int y = min_y; y <= max_y; ++y) {
                for (int x = min_x; x <= max_x; ++x) {
                    size_t slice_idx = (size_t)y * (size_t)state->volume.desc.grid_w + (size_t)x;
                    size_t brick_cell_index =
                        backend_3d_scaffold_obstacle_brick_cell_index(state, x, y, z);
                    if (!brick_mask[brick_cell_index]) continue;
                    out_solid_mask[slice_idx] = 1u;
                }
            }
        }
    }
    return true;
}

size_t backend_3d_scaffold_obstacle_solid_cell_count(const SimRuntimeBackend3DScaffold *state) {
    size_t solid_count = 0u;
    if (!state) return 0u;
    for (int brick_z = 0; brick_z < state->brick_store.bricks_d; ++brick_z) {
        for (int brick_y = 0; brick_y < state->brick_store.bricks_h; ++brick_y) {
            for (int brick_x = 0; brick_x < state->brick_store.bricks_w; ++brick_x) {
                const uint8_t *brick_mask = backend_3d_scaffold_obstacle_brick_at_const(
                    state, brick_x, brick_y, brick_z);
                int min_x = 0;
                int min_y = 0;
                int min_z = 0;
                int max_x = 0;
                int max_y = 0;
                int max_z = 0;
                if (!brick_mask) continue;
                backend_3d_scaffold_obstacle_brick_bounds(state,
                                                          brick_x,
                                                          brick_y,
                                                          brick_z,
                                                          &min_x,
                                                          &min_y,
                                                          &min_z,
                                                          &max_x,
                                                          &max_y,
                                                          &max_z);
                for (int z = min_z; z <= max_z; ++z) {
                    for (int y = min_y; y <= max_y; ++y) {
                        for (int x = min_x; x <= max_x; ++x) {
                            size_t brick_cell_index =
                                backend_3d_scaffold_obstacle_brick_cell_index(state, x, y, z);
                            if (brick_mask[brick_cell_index]) {
                                solid_count++;
                            }
                        }
                    }
                }
            }
        }
    }
    return solid_count;
}

bool backend_3d_scaffold_ensure_obstacle_dense_cache(SimRuntimeBackend3DScaffold *state) {
    if (!state || !state->obstacle_occupancy) return false;
    if (!state->obstacle_dense_cache_dirty) return true;
    if (!backend_3d_scaffold_obstacle_materialize_full(state,
                                                       state->obstacle_occupancy,
                                                       state->volume.desc.cell_count)) {
        return false;
    }
    state->obstacle_dense_cache_dirty = false;
    return true;
}

static void backend_3d_scaffold_zero_obstacle_slice(SimRuntimeBackend3DScaffold *state) {
    size_t slice_cells = 0;
    if (!state) return;
    slice_cells = state->volume.desc.slice_cell_count;
    if (slice_cells == 0) return;
    if (state->slice_solid_mask) memset(state->slice_solid_mask, 0, slice_cells * sizeof(uint8_t));
    if (state->slice_obstacle_velocity_x) {
        memset(state->slice_obstacle_velocity_x, 0, slice_cells * sizeof(float));
    }
    if (state->slice_obstacle_velocity_y) {
        memset(state->slice_obstacle_velocity_y, 0, slice_cells * sizeof(float));
    }
    if (state->slice_obstacle_distance) {
        for (size_t i = 0; i < slice_cells; ++i) {
            state->slice_obstacle_distance[i] = 1.0f;
        }
    }
}

static void backend_3d_scaffold_mark_bounds_solid(SimRuntimeBackend3DScaffold *state,
                                                  const SimRuntimeObstacleBounds3D *bounds) {
    if (!state || !bounds) return;
    for (int z = bounds->min_z; z <= bounds->max_z; ++z) {
        for (int y = bounds->min_y; y <= bounds->max_y; ++y) {
            for (int x = bounds->min_x; x <= bounds->max_x; ++x) {
                backend_3d_scaffold_set_obstacle_cell(state, x, y, z, true);
            }
        }
    }
}

static int backend_3d_scaffold_clamp_i(int value, int lo, int hi) {
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

static void backend_3d_scaffold_water_object_axis_bounds(int cells,
                                                         float min_fraction,
                                                         float max_fraction,
                                                         int *out_min,
                                                         int *out_max) {
    int lo = 0;
    int hi = 0;
    int interior_min = cells > 2 ? 1 : 0;
    int interior_max = cells > 2 ? cells - 2 : cells - 1;
    if (!out_min || !out_max || cells <= 0) return;
    lo = (int)floorf((float)cells * min_fraction);
    hi = (int)ceilf((float)cells * max_fraction) - 1;
    lo = backend_3d_scaffold_clamp_i(lo, interior_min, interior_max);
    hi = backend_3d_scaffold_clamp_i(hi, interior_min, interior_max);
    if (hi < lo) hi = lo;
    if (hi == lo && hi < interior_max) hi += 1;
    *out_min = lo;
    *out_max = hi;
}

static void backend_3d_scaffold_stamp_water_object_fixture(
    SimRuntimeBackend3DScaffold *state,
    const struct SceneState *scene) {
    int min_x = 0;
    int max_x = 0;
    int min_y = 0;
    int max_y = 0;
    int min_z = 0;
    int max_z = 0;
    if (!state || !scene || !scene->config || !scene->config->water_object_fixture) return;
    backend_3d_scaffold_water_object_axis_bounds(state->volume.desc.grid_w,
                                                 0.42f,
                                                 0.58f,
                                                 &min_x,
                                                 &max_x);
    backend_3d_scaffold_water_object_axis_bounds(state->volume.desc.grid_h,
                                                 0.20f,
                                                 0.70f,
                                                 &min_y,
                                                 &max_y);
    backend_3d_scaffold_water_object_axis_bounds(state->volume.desc.grid_d,
                                                 0.42f,
                                                 0.58f,
                                                 &min_z,
                                                 &max_z);
    for (int z = min_z; z <= max_z; ++z) {
        for (int y = min_y; y <= max_y; ++y) {
            for (int x = min_x; x <= max_x; ++x) {
                backend_3d_scaffold_set_obstacle_cell(state, x, y, z, true);
            }
        }
    }
}

static void backend_3d_scaffold_rebuild_obstacle_volume(SimRuntimeBackend3DScaffold *state,
                                                        const struct SceneState *scene) {
    SimRuntimeObstacleBounds3D bounds = {0};
    if (!state || state->volume.desc.cell_count == 0) return;
    if (scene) {
        state->scene_ref = scene;
    } else {
        scene = state->scene_ref;
    }

    backend_3d_scaffold_clear_obstacle_bricks(state);
    if (state->obstacle_brick_flags) {
        memset(state->obstacle_brick_flags, 0, state->brick_store.brick_count * sizeof(uint8_t));
    }
    if (state->obstacle_occupancy) {
        memset(state->obstacle_occupancy, 0, state->volume.desc.cell_count * sizeof(uint8_t));
    }
    if (state->export_solid_mask_cache) {
        memset(state->export_solid_mask_cache, 0, state->volume.desc.cell_count * sizeof(uint8_t));
    }
    for (int face = 0; face < SIM_RUNTIME_BOUNDARY_FACE_COUNT; ++face) {
        if (!state->obstacle_contract.domain_walls_enabled[face]) continue;
        if (!sim_runtime_obstacle_domain_face_bounds(&state->volume.desc,
                                                     (SimRuntimeBoundaryFace)face,
                                                     &bounds)) {
            continue;
        }
        backend_3d_scaffold_mark_bounds_solid(state, &bounds);
    }
    backend_3d_scaffold_stamp_water_object_fixture(state, scene);
    backend_3d_scaffold_rasterize_retained_object_obstacles(state, scene);
    backend_3d_scaffold_rasterize_retained_import_obstacles(state, scene);
    backend_3d_scaffold_rasterize_runtime_mesh_asset_obstacles(state, scene);

    backend_3d_scaffold_mark_obstacle_volume_rebuilt(state);
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak))
#endif
void backend_3d_scaffold_rasterize_runtime_mesh_asset_obstacles(
    SimRuntimeBackend3DScaffold *state,
    const struct SceneState *scene) {
    (void)state;
    (void)scene;
}

static void backend_3d_scaffold_sync_obstacle_slice(SimRuntimeBackend3DScaffold *state) {
    const SimRuntime3DDomainDesc *desc = NULL;
    float max_distance = 1.0f;
    if (!state) return;
    desc = &state->volume.desc;
    if (state->obstacle_volume_dirty) {
        backend_3d_scaffold_rebuild_obstacle_volume(state, NULL);
    }
    if (!state->obstacle_slice_dirty) return;
    if (!state->slice_solid_mask ||
        !state->slice_obstacle_velocity_x ||
        !state->slice_obstacle_velocity_y ||
        !state->slice_obstacle_distance ||
        state->compatibility_slice_z < 0 ||
        state->compatibility_slice_z >= desc->grid_d) {
        return;
    }

    backend_3d_scaffold_zero_obstacle_slice(state);
    if (!backend_3d_scaffold_obstacle_fill_slice_xy(state,
                                                    state->compatibility_slice_z,
                                                    state->slice_solid_mask,
                                                    desc->slice_cell_count)) {
        return;
    }

    if (desc->grid_w > 2 && desc->grid_h > 2) {
        int shorter = (desc->grid_w < desc->grid_h) ? desc->grid_w : desc->grid_h;
        max_distance = (float)(shorter / 2);
        if (max_distance < 1.0f) max_distance = 1.0f;
    }

    for (int y = 0; y < desc->grid_h; ++y) {
        for (int x = 0; x < desc->grid_w; ++x) {
            size_t slice_idx = (size_t)y * (size_t)desc->grid_w + (size_t)x;
            if (state->slice_solid_mask[slice_idx]) {
                state->slice_obstacle_distance[slice_idx] = 0.0f;
                continue;
            }

            {
                int dist_x_min = x;
                int dist_x_max = (desc->grid_w - 1) - x;
                int dist_y_min = y;
                int dist_y_max = (desc->grid_h - 1) - y;
                int dist_cells = dist_x_min;
                if (dist_x_max < dist_cells) dist_cells = dist_x_max;
                if (dist_y_min < dist_cells) dist_cells = dist_y_min;
                if (dist_y_max < dist_cells) dist_cells = dist_y_max;
                if (dist_cells < 0) dist_cells = 0;
                state->slice_obstacle_distance[slice_idx] = (float)dist_cells / max_distance;
                if (state->slice_obstacle_distance[slice_idx] > 1.0f) {
                    state->slice_obstacle_distance[slice_idx] = 1.0f;
                }
            }
        }
    }

    state->obstacle_slice_dirty = false;
}

static void backend_3d_scaffold_apply_obstacle_enforcement(SimRuntimeBackend3DScaffold *state) {
    if (!state) return;
    if (state->obstacle_volume_dirty) {
        backend_3d_scaffold_rebuild_obstacle_volume(state, NULL);
    }
    if (!state->obstacle_brick_flags) return;

    for (int brick_z = 0; brick_z < state->brick_store.bricks_d; ++brick_z) {
        for (int brick_y = 0; brick_y < state->brick_store.bricks_h; ++brick_y) {
            for (int brick_x = 0; brick_x < state->brick_store.bricks_w; ++brick_x) {
                const uint8_t *brick_mask = NULL;
                int min_x = 0;
                int min_y = 0;
                int min_z = 0;
                int max_x = 0;
                int max_y = 0;
                int max_z = 0;
                size_t brick_index =
                    backend_3d_scaffold_obstacle_brick_index(state, brick_x, brick_y, brick_z);
                if (!state->obstacle_brick_flags[brick_index]) continue;
                brick_mask = backend_3d_scaffold_obstacle_brick_at_const(state, brick_x, brick_y, brick_z);
                if (!brick_mask) continue;
                backend_3d_scaffold_obstacle_brick_bounds(state,
                                                          brick_x,
                                                          brick_y,
                                                          brick_z,
                                                          &min_x,
                                                          &min_y,
                                                          &min_z,
                                                          &max_x,
                                                          &max_y,
                                                          &max_z);
                for (int z = min_z; z <= max_z; ++z) {
                    for (int y = min_y; y <= max_y; ++y) {
                        for (int x = min_x; x <= max_x; ++x) {
                            size_t brick_cell_index =
                                backend_3d_scaffold_obstacle_brick_cell_index(state, x, y, z);
                            size_t idx = sim_runtime_3d_volume_index(&state->volume.desc, x, y, z);
                            if (!brick_mask[brick_cell_index]) continue;
                            sim_runtime_3d_brick_store_zero_cell(&state->brick_store, x, y, z);
                            if (backend_3d_scaffold_dense_mirror_live(state)) {
                                state->volume.density[idx] = 0.0f;
                                state->volume.velocity_x[idx] = 0.0f;
                                state->volume.velocity_y[idx] = 0.0f;
                                state->volume.velocity_z[idx] = 0.0f;
                                state->volume.pressure[idx] = 0.0f;
                            }
                        }
                    }
                }
            }
        }
    }

    backend_3d_scaffold_mark_obstacle_cell_cache_dirty(state);
    backend_3d_scaffold_mark_fluid_dirty(state);
}

void backend_3d_scaffold_reset_obstacles(SimRuntimeBackend3DScaffold *state) {
    if (!state) return;
    backend_3d_scaffold_clear_obstacle_bricks(state);
    if (state->obstacle_occupancy && state->volume.desc.cell_count > 0) {
        memset(state->obstacle_occupancy, 0, state->volume.desc.cell_count * sizeof(uint8_t));
    }
    if (state->obstacle_brick_flags && state->brick_store.brick_count > 0) {
        memset(state->obstacle_brick_flags, 0, state->brick_store.brick_count * sizeof(uint8_t));
    }
    backend_3d_scaffold_zero_obstacle_slice(state);
    backend_3d_scaffold_mark_obstacle_cell_cache_dirty(state);
    backend_3d_scaffold_mark_obstacle_volume_rebuild_needed(state);
}

void backend_3d_scaffold_build_static_obstacles(SimRuntimeBackend *backend,
                                                struct SceneState *scene) {
    SimRuntimeBackend3DScaffold *state = backend ? (SimRuntimeBackend3DScaffold *)backend->impl : NULL;
    if (!state) return;
    backend_3d_scaffold_rebuild_obstacle_volume(state, scene);
    backend_3d_scaffold_sync_obstacle_slice(state);
}

void backend_3d_scaffold_build_obstacles(SimRuntimeBackend *backend,
                                         struct SceneState *scene) {
    SimRuntimeBackend3DScaffold *state = backend ? (SimRuntimeBackend3DScaffold *)backend->impl : NULL;
    if (!state) return;
    backend_3d_scaffold_rebuild_obstacle_volume(state, scene);
    backend_3d_scaffold_sync_obstacle_slice(state);
}

void backend_3d_scaffold_mark_obstacles_dirty(SimRuntimeBackend *backend) {
    SimRuntimeBackend3DScaffold *state = backend ? (SimRuntimeBackend3DScaffold *)backend->impl : NULL;
    if (!state) return;
    backend_3d_scaffold_mark_obstacle_volume_rebuild_needed(state);
}

void backend_3d_scaffold_rasterize_dynamic_obstacles(SimRuntimeBackend *backend,
                                                     struct SceneState *scene) {
    SimRuntimeBackend3DScaffold *state = backend ? (SimRuntimeBackend3DScaffold *)backend->impl : NULL;
    if (!state) return;
    backend_3d_scaffold_rebuild_obstacle_volume(state, scene);
    backend_3d_scaffold_sync_obstacle_slice(state);
}

void backend_3d_scaffold_enforce_boundary_flows(SimRuntimeBackend *backend,
                                                struct SceneState *scene) {
    (void)scene;
    backend_3d_scaffold_apply_obstacle_enforcement(
        backend ? (SimRuntimeBackend3DScaffold *)backend->impl : NULL);
}

void backend_3d_scaffold_enforce_obstacles(SimRuntimeBackend *backend,
                                           struct SceneState *scene) {
    (void)scene;
    backend_3d_scaffold_apply_obstacle_enforcement(
        backend ? (SimRuntimeBackend3DScaffold *)backend->impl : NULL);
}

bool backend_3d_scaffold_get_obstacle_view_2d(const SimRuntimeBackend *backend,
                                              SceneObstacleFieldView2D *out_view) {
    SimRuntimeBackend3DScaffold *state = backend ? (SimRuntimeBackend3DScaffold *)backend->impl : NULL;
    if (!state || !out_view) return false;
    backend_3d_scaffold_sync_obstacle_slice(state);
    out_view->width = state->volume.desc.grid_w;
    out_view->height = state->volume.desc.grid_h;
    out_view->cell_count = state->volume.desc.slice_cell_count;
    out_view->solid_mask = state->slice_solid_mask;
    out_view->velocity_x = state->slice_obstacle_velocity_x;
    out_view->velocity_y = state->slice_obstacle_velocity_y;
    out_view->distance = state->slice_obstacle_distance;
    return true;
}
