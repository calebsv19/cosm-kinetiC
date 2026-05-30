#include "app/sim_runtime_backend.h"

#include "app/scene_state.h"
#include "app/sim_runtime_backend_3d_runtime.h"
#include "app/sim_runtime_backend_3d_scaffold_internal.h"
#include "app/sim_runtime_3d_domain.h"
#include "app/sim_runtime_3d_solver.h"
#include "app/sim_runtime_obstacle.h"
#include "app/atmospheric/atmospheric_field.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static const float SCAFFOLD_BRUSH_DENSITY = 20.0f;
static const float SCAFFOLD_BRUSH_VEL_SCALE = 35.0f;
static const float SCAFFOLD_BRUSH_VELOCITY_DENSITY = 4.0f;
static const int SCAFFOLD_BRICK_SIZE = 8;
static const size_t SCAFFOLD_DENSE_MIRROR_MAX_CELLS = (size_t)5 * 1024 * 1024;

static SimRuntimeBackend3DScaffold *backend_3d_scaffold_state(SimRuntimeBackend *backend) {
    return backend ? (SimRuntimeBackend3DScaffold *)backend->impl : NULL;
}

static const SimRuntimeBackend3DScaffold *backend_3d_scaffold_state_const(
    const SimRuntimeBackend *backend) {
    return backend ? (const SimRuntimeBackend3DScaffold *)backend->impl : NULL;
}

typedef struct Backend3DScaffoldSparseStatsAccum {
    size_t active_density_cells;
    float max_density;
    float max_velocity_magnitude;
    bool scene_up_velocity_valid;
    float axis_x;
    float axis_y;
    float axis_z;
    double scene_up_velocity_weighted_sum;
    double scene_up_density_weight;
    float scene_up_velocity_peak;
} Backend3DScaffoldSparseStatsAccum;

static bool backend_3d_scaffold_accumulate_sparse_stats(int x,
                                                        int y,
                                                        int z,
                                                        float density,
                                                        float velocity_x,
                                                        float velocity_y,
                                                        float velocity_z,
                                                        float pressure,
                                                        void *user_data) {
    Backend3DScaffoldSparseStatsAccum *accum =
        (Backend3DScaffoldSparseStatsAccum *)user_data;
    const float density_threshold = 0.0001f;
    float speed = 0.0f;
    (void)x;
    (void)y;
    (void)z;
    (void)pressure;
    if (!accum) return false;
    speed = sqrtf(velocity_x * velocity_x +
                  velocity_y * velocity_y +
                  velocity_z * velocity_z);
    if (density > accum->max_density) {
        accum->max_density = density;
    }
    if (speed > accum->max_velocity_magnitude) {
        accum->max_velocity_magnitude = speed;
    }
    if (density > density_threshold) {
        accum->active_density_cells++;
        if (accum->scene_up_velocity_valid) {
            float scene_up_velocity = velocity_x * accum->axis_x +
                                      velocity_y * accum->axis_y +
                                      velocity_z * accum->axis_z;
            accum->scene_up_velocity_weighted_sum +=
                (double)scene_up_velocity * (double)density;
            accum->scene_up_density_weight += (double)density;
            if (scene_up_velocity > accum->scene_up_velocity_peak) {
                accum->scene_up_velocity_peak = scene_up_velocity;
            }
        }
    }
    return true;
}

static void backend_3d_scaffold_mark_fluid_dirty(SimRuntimeBackend3DScaffold *state) {
    if (!state) return;
    state->debug_volume_stats_dirty = true;
    state->export_volume_cache_dirty = true;
    state->fluid_slice_dirty = true;
}

bool backend_3d_scaffold_ensure_export_cache(SimRuntimeBackend3DScaffold *state) {
    if (!state) return false;
    if (!state->export_volume_cache.density) {
        state->export_volume_cache.desc = state->volume.desc;
        if (!sim_runtime_3d_volume_init(&state->export_volume_cache, &state->volume.desc)) {
            return false;
        }
    }
    if (!state->export_solid_mask_cache) {
        state->export_solid_mask_cache =
            (uint8_t *)calloc(state->volume.desc.cell_count, sizeof(uint8_t));
        if (!state->export_solid_mask_cache) return false;
        state->export_volume_cache_dirty = true;
    }
    if (!state->export_volume_cache_dirty) return true;
    if (!sim_runtime_3d_brick_store_materialize_full(&state->brick_store, &state->export_volume_cache)) {
        return false;
    }
    backend_3d_scaffold_runtime_note_export_cache_materialized(state);
    if (!backend_3d_scaffold_obstacle_materialize_full(state,
                                                       state->export_solid_mask_cache,
                                                       state->volume.desc.cell_count)) {
        return false;
    }
    state->export_volume_cache_dirty = false;
    return true;
}

static bool backend_3d_scaffold_get_domain_desc_3d(const SimRuntimeBackend *backend,
                                                   SimRuntime3DDomainDesc *out_desc) {
    const SimRuntimeBackend3DScaffold *state = backend_3d_scaffold_state_const(backend);
    if (!state || !out_desc) return false;
    *out_desc = state->volume.desc;
    return true;
}

static bool backend_3d_scaffold_debug_write_volume_cell_3d(SimRuntimeBackend *backend,
                                                           int x,
                                                           int y,
                                                           int z,
                                                           float density,
                                                           float velocity_x,
                                                           float velocity_y,
                                                           float velocity_z,
                                                           float pressure,
                                                           uint8_t solid) {
    SimRuntimeBackend3DScaffold *state = backend_3d_scaffold_state(backend);
    size_t idx = 0;
    if (!state) return false;
    if (x < 0 || x >= state->volume.desc.grid_w ||
        y < 0 || y >= state->volume.desc.grid_h ||
        z < 0 || z >= state->volume.desc.grid_d) {
        return false;
    }
    if (!sim_runtime_3d_brick_store_set_cell(&state->brick_store,
                                             x,
                                             y,
                                             z,
                                             density,
                                             velocity_x,
                                             velocity_y,
                                             velocity_z,
                                             pressure)) {
        return false;
    }
    idx = sim_runtime_3d_volume_index(&state->volume.desc, x, y, z);
    backend_3d_scaffold_set_obstacle_cell(state, x, y, z, solid != 0u);
    if (backend_3d_scaffold_dense_mirror_live(state)) {
        state->volume.density[idx] = density;
        state->volume.velocity_x[idx] = velocity_x;
        state->volume.velocity_y[idx] = velocity_y;
        state->volume.velocity_z[idx] = velocity_z;
        state->volume.pressure[idx] = pressure;
    }
    state->obstacle_volume_dirty = false;
    state->obstacle_slice_dirty = true;
    backend_3d_scaffold_mark_fluid_dirty(state);
    return true;
}

static bool backend_3d_scaffold_debug_reset_volume_truth_3d(SimRuntimeBackend *backend) {
    SimRuntimeBackend3DScaffold *state = backend_3d_scaffold_state(backend);
    if (!state) return false;
    sim_runtime_3d_brick_store_clear(&state->brick_store);
    backend_3d_scaffold_clear_obstacle_bricks(state);
    if (backend_3d_scaffold_dense_mirror_live(state)) {
        sim_runtime_3d_volume_clear(&state->volume);
    }
    if (state->export_volume_cache.density) {
        sim_runtime_3d_volume_clear(&state->export_volume_cache);
    }
    if (state->obstacle_occupancy) {
        memset(state->obstacle_occupancy, 0, state->volume.desc.cell_count * sizeof(uint8_t));
    }
    if (state->obstacle_brick_flags) {
        memset(state->obstacle_brick_flags, 0, state->brick_store.brick_count * sizeof(uint8_t));
    }
    if (state->export_solid_mask_cache) {
        memset(state->export_solid_mask_cache, 0, state->volume.desc.cell_count * sizeof(uint8_t));
    }
    state->obstacle_volume_dirty = false;
    state->obstacle_dense_cache_dirty = true;
    state->obstacle_slice_dirty = true;
    state->initial_state_source = SIM_RUNTIME_INITIAL_STATE_SOURCE_BLANK;
    state->atmospheric_seeded = false;
    state->atmospheric_seed = 0u;
    state->atmospheric_seeded_cell_count = 0u;
    state->atmospheric_seed_max_density = 0.0f;
    state->atmospheric_seed_max_velocity_magnitude = 0.0f;
    state->atmospheric_warm_start_loaded = false;
    state->atmospheric_warm_start_source_kind = 0;
    state->atmospheric_warm_start_w = 0;
    state->atmospheric_warm_start_h = 0;
    state->atmospheric_warm_start_d = 0;
    state->atmospheric_warm_start_cell_count = 0u;
    state->atmospheric_warm_start_active_density_cells = 0u;
    state->atmospheric_warm_start_solid_cells = 0u;
    state->atmospheric_warm_start_max_density = 0.0f;
    state->atmospheric_warm_start_max_velocity_magnitude = 0.0f;
    backend_3d_scaffold_mark_fluid_dirty(state);
    return true;
}

static bool backend_3d_scaffold_debug_note_atmospheric_warm_start_3d(
    SimRuntimeBackend *backend,
    int source_kind,
    int width,
    int height,
    int depth,
    size_t cell_count,
    size_t active_density_cells,
    size_t solid_cells,
    float max_density,
    float max_velocity_magnitude) {
    SimRuntimeBackend3DScaffold *state = backend_3d_scaffold_state(backend);
    if (!state) return false;
    state->initial_state_source = SIM_RUNTIME_INITIAL_STATE_SOURCE_ATMOSPHERIC_WARM_START;
    state->atmospheric_seeded = false;
    state->atmospheric_seed = 0u;
    state->atmospheric_seeded_cell_count = 0u;
    state->atmospheric_seed_max_density = 0.0f;
    state->atmospheric_seed_max_velocity_magnitude = 0.0f;
    state->atmospheric_warm_start_loaded = true;
    state->atmospheric_warm_start_source_kind = source_kind;
    state->atmospheric_warm_start_w = width;
    state->atmospheric_warm_start_h = height;
    state->atmospheric_warm_start_d = depth;
    state->atmospheric_warm_start_cell_count = cell_count;
    state->atmospheric_warm_start_active_density_cells = active_density_cells;
    state->atmospheric_warm_start_solid_cells = solid_cells;
    state->atmospheric_warm_start_max_density = max_density;
    state->atmospheric_warm_start_max_velocity_magnitude = max_velocity_magnitude;
    return true;
}

static bool backend_3d_scaffold_debug_zero_dense_mirror_3d(SimRuntimeBackend *backend) {
    SimRuntimeBackend3DScaffold *state = backend_3d_scaffold_state(backend);
    if (!state) return false;
    if (!backend_3d_scaffold_dense_mirror_live(state)) return true;
    sim_runtime_3d_volume_clear(&state->volume);
    backend_3d_scaffold_mark_fluid_dirty(state);
    return true;
}

static bool backend_3d_scaffold_debug_zero_obstacle_dense_cache_3d(SimRuntimeBackend *backend) {
    SimRuntimeBackend3DScaffold *state = backend_3d_scaffold_state(backend);
    if (!state) return false;
    if (state->obstacle_occupancy) {
        memset(state->obstacle_occupancy, 0, state->volume.desc.cell_count * sizeof(uint8_t));
    }
    if (state->export_solid_mask_cache) {
        memset(state->export_solid_mask_cache, 0, state->volume.desc.cell_count * sizeof(uint8_t));
    }
    state->obstacle_dense_cache_dirty = true;
    state->obstacle_slice_dirty = true;
    state->export_volume_cache_dirty = true;
    state->debug_volume_stats_dirty = true;
    return true;
}

static void backend_3d_scaffold_reset(SimRuntimeBackend3DScaffold *state) {
    if (!state) return;
    sim_runtime_3d_brick_store_clear(&state->brick_store);
    sim_runtime_3d_volume_clear(&state->solver_volume);
    sim_runtime_3d_solver_scratch_clear(&state->solver_scratch);
    sim_runtime_3d_volume_clear(&state->export_volume_cache);
    if (backend_3d_scaffold_dense_mirror_live(state)) {
        sim_runtime_3d_volume_clear(&state->volume);
    }
    core_sim_loop_reset(&state->solver_loop);
    backend_3d_scaffold_reset_obstacles(state);
    state->emitter_step_emitters_applied = 0;
    state->emitter_step_free_emitters_applied = 0;
    state->emitter_step_attached_emitters_applied = 0;
    state->emitter_step_affected_cells = 0;
    state->emitter_step_last_footprint_cells = 0;
    state->emitter_step_density_delta = 0.0f;
    state->emitter_step_velocity_magnitude_delta = 0.0f;
    state->debug_volume_stats_dirty = true;
    state->debug_volume_active_density_cells = 0;
    state->debug_volume_solid_cells = 0;
    state->debug_volume_max_density = 0.0f;
    state->debug_volume_max_velocity_magnitude = 0.0f;
    state->debug_volume_scene_up_velocity_valid = false;
    state->debug_volume_scene_up_velocity_avg = 0.0f;
    state->debug_volume_scene_up_velocity_peak = 0.0f;
    state->initial_state_source = SIM_RUNTIME_INITIAL_STATE_SOURCE_BLANK;
    state->atmospheric_seeded = false;
    state->atmospheric_seed = 0u;
    state->atmospheric_seeded_cell_count = 0u;
    state->atmospheric_seed_max_density = 0.0f;
    state->atmospheric_seed_max_velocity_magnitude = 0.0f;
    state->atmospheric_warm_start_loaded = false;
    state->atmospheric_warm_start_source_kind = 0;
    state->atmospheric_warm_start_w = 0;
    state->atmospheric_warm_start_h = 0;
    state->atmospheric_warm_start_d = 0;
    state->atmospheric_warm_start_cell_count = 0u;
    state->atmospheric_warm_start_active_density_cells = 0u;
    state->atmospheric_warm_start_solid_cells = 0u;
    state->atmospheric_warm_start_max_density = 0.0f;
    state->atmospheric_warm_start_max_velocity_magnitude = 0.0f;
    backend_3d_scaffold_runtime_reset_metrics(state);
    backend_3d_scaffold_mark_fluid_dirty(state);
}

static size_t backend_3d_scaffold_seed_atmosphere(SimRuntimeBackend3DScaffold *state,
                                                  const FluidScenePreset *preset) {
    AtmosphericInitialStateSource source = atmospheric_initial_state_source(preset);
    if (!state || source == ATMOSPHERIC_INITIAL_STATE_NONE) return 0;
    const SimRuntime3DDomainDesc *desc = &state->volume.desc;
    const float density_threshold = 0.0001f;
    float inv_w = (desc->grid_w > 1) ? 1.0f / (float)(desc->grid_w - 1) : 0.0f;
    float inv_h = (desc->grid_h > 1) ? 1.0f / (float)(desc->grid_h - 1) : 0.0f;
    float inv_d = (desc->grid_d > 1) ? 1.0f / (float)(desc->grid_d - 1) : 0.0f;
    size_t seeded = 0;
    float max_density = 0.0f;
    float max_velocity = 0.0f;

    state->initial_state_source =
        (source == ATMOSPHERIC_INITIAL_STATE_STANDALONE_MODE)
            ? SIM_RUNTIME_INITIAL_STATE_SOURCE_ATMOSPHERIC_STANDALONE
            : SIM_RUNTIME_INITIAL_STATE_SOURCE_ATMOSPHERIC_OPTIONAL_LAYER;

    for (int z = 0; z < desc->grid_d; ++z) {
        for (int y = 0; y < desc->grid_h; ++y) {
            for (int x = 0; x < desc->grid_w; ++x) {
                AtmosphericFieldSample sample =
                    atmospheric_field_sample_3d(&preset->atmosphere,
                                                (float)x * inv_w,
                                                (float)y * inv_h,
                                                (float)z * inv_d);
                size_t idx = 0;
                if (sample.density <= density_threshold) {
                    continue;
                }
                if (!sim_runtime_3d_brick_store_set_cell(&state->brick_store,
                                                         x,
                                                         y,
                                                         z,
                                                         sample.density,
                                                         sample.velocity_x,
                                                         sample.velocity_y,
                                                         sample.velocity_z,
                                                         0.0f)) {
                    continue;
                }
                if (backend_3d_scaffold_dense_mirror_live(state)) {
                    idx = sim_runtime_3d_volume_index(desc, x, y, z);
                    state->volume.density[idx] = sample.density;
                    state->volume.velocity_x[idx] = sample.velocity_x;
                    state->volume.velocity_y[idx] = sample.velocity_y;
                    state->volume.velocity_z[idx] = sample.velocity_z;
                    state->volume.pressure[idx] = 0.0f;
                }
                if (sample.density > max_density) {
                    max_density = sample.density;
                }
                {
                    float speed = sqrtf(sample.velocity_x * sample.velocity_x +
                                        sample.velocity_y * sample.velocity_y +
                                        sample.velocity_z * sample.velocity_z);
                    if (speed > max_velocity) max_velocity = speed;
                }
                seeded++;
            }
        }
    }

    state->atmospheric_seeded = seeded > 0;
    state->atmospheric_seed = preset->atmosphere.seed;
    state->atmospheric_seeded_cell_count = seeded;
    state->atmospheric_seed_max_density = max_density;
    state->atmospheric_seed_max_velocity_magnitude = max_velocity;
    if (seeded > 0) {
        backend_3d_scaffold_mark_fluid_dirty(state);
    }
    return seeded;
}

void backend_3d_scaffold_update_debug_volume_stats(SimRuntimeBackend3DScaffold *state) {
    const float density_threshold = 0.0001f;
    const SimRuntime3DVolume *stats_volume = NULL;
    Backend3DScaffoldSparseStatsAccum sparse_accum = {0};
    float axis_x = 0.0f;
    float axis_y = 0.0f;
    float axis_z = 0.0f;
    float axis_len = 0.0f;
    double scene_up_velocity_weighted_sum = 0.0;
    double scene_up_density_weight = 0.0;
    if (!state) return;
    if (!state->debug_volume_stats_dirty) return;
    if (!backend_3d_scaffold_dense_mirror_live(state)) {
        stats_volume = NULL;
    } else {
        if (!backend_3d_scaffold_ensure_obstacle_dense_cache(state)) return;
        stats_volume = &state->volume;
    }

    state->debug_volume_active_density_cells = 0;
    state->debug_volume_solid_cells = 0;
    state->debug_volume_max_density = 0.0f;
    state->debug_volume_max_velocity_magnitude = 0.0f;
    state->debug_volume_scene_up_velocity_valid = false;
    state->debug_volume_scene_up_velocity_avg = 0.0f;
    state->debug_volume_scene_up_velocity_peak = 0.0f;

    if (state->scene_up_valid) {
        axis_x = state->scene_up_x;
        axis_y = state->scene_up_y;
        axis_z = state->scene_up_z;
        axis_len = sqrtf(axis_x * axis_x + axis_y * axis_y + axis_z * axis_z);
        if (axis_len > 0.0001f) {
            axis_x /= axis_len;
            axis_y /= axis_len;
            axis_z /= axis_len;
            state->debug_volume_scene_up_velocity_valid = true;
        }
    }

    if (!stats_volume) {
        sparse_accum.scene_up_velocity_valid = state->debug_volume_scene_up_velocity_valid;
        sparse_accum.axis_x = axis_x;
        sparse_accum.axis_y = axis_y;
        sparse_accum.axis_z = axis_z;
        state->debug_volume_solid_cells = backend_3d_scaffold_obstacle_solid_cell_count(state);
        if (!sim_runtime_3d_brick_store_visit_active_cells(&state->brick_store,
                                                           backend_3d_scaffold_accumulate_sparse_stats,
                                                           &sparse_accum)) {
            return;
        }
        state->debug_volume_active_density_cells = sparse_accum.active_density_cells;
        state->debug_volume_max_density = sparse_accum.max_density;
        state->debug_volume_max_velocity_magnitude = sparse_accum.max_velocity_magnitude;
        if (state->debug_volume_scene_up_velocity_valid &&
            sparse_accum.scene_up_density_weight > 0.0) {
            state->debug_volume_scene_up_velocity_avg =
                (float)(sparse_accum.scene_up_velocity_weighted_sum /
                        sparse_accum.scene_up_density_weight);
        }
        if (state->debug_volume_scene_up_velocity_valid) {
            state->debug_volume_scene_up_velocity_peak =
                sparse_accum.scene_up_velocity_peak;
        }
        state->debug_volume_stats_dirty = false;
        return;
    }

    for (size_t i = 0; i < stats_volume->desc.cell_count; ++i) {
        float density = stats_volume->density[i];
        float velocity_x = stats_volume->velocity_x[i];
        float velocity_y = stats_volume->velocity_y[i];
        float velocity_z = stats_volume->velocity_z[i];
        float speed = sqrtf(velocity_x * velocity_x +
                            velocity_y * velocity_y +
                            velocity_z * velocity_z);
        if (state->obstacle_occupancy[i]) {
            state->debug_volume_solid_cells++;
        }
        if (density > state->debug_volume_max_density) {
            state->debug_volume_max_density = density;
        }
        if (speed > state->debug_volume_max_velocity_magnitude) {
            state->debug_volume_max_velocity_magnitude = speed;
        }
        if (density > density_threshold) {
            state->debug_volume_active_density_cells++;
            if (state->debug_volume_scene_up_velocity_valid) {
                float scene_up_velocity = velocity_x * axis_x +
                                          velocity_y * axis_y +
                                          velocity_z * axis_z;
                scene_up_velocity_weighted_sum += (double)scene_up_velocity * (double)density;
                scene_up_density_weight += (double)density;
                if (scene_up_velocity > state->debug_volume_scene_up_velocity_peak) {
                    state->debug_volume_scene_up_velocity_peak = scene_up_velocity;
                }
            }
        }
    }

    if (state->debug_volume_scene_up_velocity_valid && scene_up_density_weight > 0.0) {
        state->debug_volume_scene_up_velocity_avg =
            (float)(scene_up_velocity_weighted_sum / scene_up_density_weight);
    }
    state->debug_volume_stats_dirty = false;
}

static bool backend_3d_scaffold_sync_fluid_slice(SimRuntimeBackend3DScaffold *state) {
    const SimRuntime3DDomainDesc *desc = NULL;
    if (!state) return false;
    desc = &state->volume.desc;
    if (!state->fluid_slice_dirty) return true;
    if (!state->slice_density ||
        !state->slice_velocity_x ||
        !state->slice_velocity_y ||
        !state->slice_pressure ||
        desc->slice_cell_count == 0 ||
        state->compatibility_slice_z < 0 ||
        state->compatibility_slice_z >= desc->grid_d) {
        return false;
    }
    if (backend_3d_scaffold_dense_mirror_live(state)) {
        size_t slice_start = (size_t)state->compatibility_slice_z * desc->slice_cell_count;
        memcpy(state->slice_density,
               state->volume.density + slice_start,
               desc->slice_cell_count * sizeof(float));
        memcpy(state->slice_velocity_x,
               state->volume.velocity_x + slice_start,
               desc->slice_cell_count * sizeof(float));
        memcpy(state->slice_velocity_y,
               state->volume.velocity_y + slice_start,
               desc->slice_cell_count * sizeof(float));
        memcpy(state->slice_pressure,
               state->volume.pressure + slice_start,
               desc->slice_cell_count * sizeof(float));
    } else if (!sim_runtime_3d_brick_store_fill_slice_xy(&state->brick_store,
                                                         state->compatibility_slice_z,
                                                         state->slice_density,
                                                         state->slice_velocity_x,
                                                         state->slice_velocity_y,
                                                         state->slice_pressure)) {
        return false;
    }
    state->fluid_slice_dirty = false;
    return true;
}

static bool backend_3d_scaffold_set_slice_z(SimRuntimeBackend3DScaffold *state, int next_z) {
    const SimRuntime3DDomainDesc *desc = NULL;
    if (!state) return false;
    desc = &state->volume.desc;
    if (desc->grid_d <= 0) return false;
    if (next_z < 0) next_z = 0;
    if (next_z >= desc->grid_d) next_z = desc->grid_d - 1;
    if (state->compatibility_slice_z == next_z) return false;
    state->compatibility_slice_z = next_z;
    state->fluid_slice_dirty = true;
    state->obstacle_slice_dirty = true;
    return true;
}

static void backend_3d_scaffold_destroy(SimRuntimeBackend *backend) {
    SimRuntimeBackend3DScaffold *state = backend_3d_scaffold_state(backend);
    if (state) {
        backend_3d_scaffold_clear_obstacle_bricks(state);
        sim_runtime_3d_brick_store_destroy(&state->brick_store);
        sim_runtime_3d_volume_destroy(&state->volume);
        sim_runtime_3d_volume_destroy(&state->solver_volume);
        sim_runtime_3d_volume_destroy(&state->export_volume_cache);
        sim_runtime_3d_solver_scratch_destroy(&state->solver_scratch);
        free(state->solver_solid_mask);
        free(state->export_solid_mask_cache);
        free(state->obstacle_bricks);
        free(state->obstacle_brick_flags);
        free(state->slice_density);
        free(state->slice_velocity_x);
        free(state->slice_velocity_y);
        free(state->slice_pressure);
        free(state->obstacle_occupancy);
        free(state->slice_solid_mask);
        free(state->slice_obstacle_velocity_x);
        free(state->slice_obstacle_velocity_y);
        free(state->slice_obstacle_distance);
        free(state);
    }
    free(backend);
}

static bool backend_3d_scaffold_valid(const SimRuntimeBackend *backend) {
    const SimRuntimeBackend3DScaffold *state = backend_3d_scaffold_state_const(backend);
    return state &&
           state->volume.desc.grid_w > 0 &&
           state->volume.desc.grid_h > 0 &&
           state->volume.desc.grid_d > 0 &&
           state->volume.desc.cell_count > 0 &&
           state->brick_store.bricks &&
           state->obstacle_bricks &&
           state->obstacle_occupancy &&
           state->obstacle_brick_flags &&
           state->slice_density &&
           state->slice_velocity_x &&
           state->slice_velocity_y &&
           state->slice_pressure &&
           state->slice_solid_mask &&
           state->slice_obstacle_velocity_x &&
           state->slice_obstacle_velocity_y &&
           state->slice_obstacle_distance;
}

static void backend_3d_scaffold_clear(SimRuntimeBackend *backend) {
    backend_3d_scaffold_reset(backend_3d_scaffold_state(backend));
}

static void backend_3d_scaffold_window_to_grid(const SimRuntimeBackend3DScaffold *state,
                                               const AppConfig *cfg,
                                               int win_x,
                                               int win_y,
                                               int *out_gx,
                                               int *out_gy) {
    const SimRuntime3DDomainDesc *desc = &state->volume.desc;
    float sx = (float)win_x / (float)(cfg->window_w > 0 ? cfg->window_w : 1);
    float sy = (float)win_y / (float)(cfg->window_h > 0 ? cfg->window_h : 1);
    int gx = (int)(sx * (float)desc->grid_w);
    int gy = (int)(sy * (float)desc->grid_h);

    if (gx < 0) gx = 0;
    if (gx >= desc->grid_w) gx = desc->grid_w - 1;
    if (gy < 0) gy = 0;
    if (gy >= desc->grid_h) gy = desc->grid_h - 1;

    *out_gx = gx;
    *out_gy = gy;
}

static bool backend_3d_scaffold_apply_brush_sample(SimRuntimeBackend *backend,
                                                   const AppConfig *cfg,
                                                   const StrokeSample *sample) {
    SimRuntimeBackend3DScaffold *state = backend_3d_scaffold_state(backend);
    int gx = 0;
    int gy = 0;
    float inv_w = 0.0f;
    float inv_h = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    if (!state || !cfg || !sample || state->volume.desc.cell_count == 0) return false;

    backend_3d_scaffold_window_to_grid(state, cfg, sample->x, sample->y, &gx, &gy);
    inv_w = (float)(cfg->window_w > 0 ? cfg->window_w : 1);
    inv_h = (float)(cfg->window_h > 0 ? cfg->window_h : 1);
    vx = (sample->vx / inv_w) * SCAFFOLD_BRUSH_VEL_SCALE;
    vy = (sample->vy / inv_h) * SCAFFOLD_BRUSH_VEL_SCALE;

    switch (sample->mode) {
    case BRUSH_MODE_VELOCITY:
        if (!sim_runtime_3d_brick_store_add_cell(&state->brick_store,
                                                 gx,
                                                 gy,
                                                 state->compatibility_slice_z,
                                                 SCAFFOLD_BRUSH_VELOCITY_DENSITY,
                                                 vx,
                                                 vy,
                                                 0.0f,
                                                 0.0f)) {
            return false;
        }
        if (backend_3d_scaffold_dense_mirror_live(state)) {
            size_t idx = sim_runtime_3d_volume_index(&state->volume.desc,
                                                     gx,
                                                     gy,
                                                     state->compatibility_slice_z);
            state->volume.velocity_x[idx] += vx;
            state->volume.velocity_y[idx] += vy;
            state->volume.density[idx] += SCAFFOLD_BRUSH_VELOCITY_DENSITY;
        }
        break;
    case BRUSH_MODE_DENSITY:
    default:
        if (!sim_runtime_3d_brick_store_add_cell(&state->brick_store,
                                                 gx,
                                                 gy,
                                                 state->compatibility_slice_z,
                                                 SCAFFOLD_BRUSH_DENSITY,
                                                 vx * 0.25f,
                                                 vy * 0.25f,
                                                 0.0f,
                                                 0.0f)) {
            return false;
        }
        if (backend_3d_scaffold_dense_mirror_live(state)) {
            size_t idx = sim_runtime_3d_volume_index(&state->volume.desc,
                                                     gx,
                                                     gy,
                                                     state->compatibility_slice_z);
            state->volume.density[idx] += SCAFFOLD_BRUSH_DENSITY;
            state->volume.velocity_x[idx] += vx * 0.25f;
            state->volume.velocity_y[idx] += vy * 0.25f;
        }
        break;
    }

    backend_3d_scaffold_mark_fluid_dirty(state);
    return true;
}

static void backend_3d_scaffold_build_emitter_masks(SimRuntimeBackend *backend,
                                                    struct SceneState *scene) {
    (void)backend;
    (void)scene;
}

static void backend_3d_scaffold_mark_emitters_dirty(SimRuntimeBackend *backend) {
    (void)backend;
}

static void backend_3d_scaffold_apply_boundary_flows(SimRuntimeBackend *backend,
                                                     struct SceneState *scene,
                                                     double dt) {
    (void)backend;
    (void)scene;
    (void)dt;
}

static void backend_3d_scaffold_step(SimRuntimeBackend *backend,
                                     struct SceneState *scene,
                                     const AppConfig *cfg,
                                     double dt) {
    (void)backend_3d_scaffold_runtime_step(backend, scene, cfg, dt);
}

static void backend_3d_scaffold_inject_object_motion(SimRuntimeBackend *backend,
                                                     const struct SceneState *scene) {
    (void)backend;
    (void)scene;
}

static void backend_3d_scaffold_reset_transient_state(SimRuntimeBackend *backend) {
    backend_3d_scaffold_reset(backend_3d_scaffold_state(backend));
}

static void backend_3d_scaffold_seed_uniform_velocity_2d(SimRuntimeBackend *backend,
                                                         float velocity_x,
                                                         float velocity_y) {
    SimRuntimeBackend3DScaffold *state = backend_3d_scaffold_state(backend);
    if (!state || state->volume.desc.cell_count == 0) return;
    for (int z = 0; z < state->volume.desc.grid_d; ++z) {
        for (int y = 0; y < state->volume.desc.grid_h; ++y) {
            for (int x = 0; x < state->volume.desc.grid_w; ++x) {
                sim_runtime_3d_brick_store_set_cell(&state->brick_store,
                                                    x,
                                                    y,
                                                    z,
                                                    0.0f,
                                                    velocity_x,
                                                    velocity_y,
                                                    0.0f,
                                                    0.0f);
            }
        }
    }
    if (backend_3d_scaffold_dense_mirror_live(state)) {
        for (size_t i = 0; i < state->volume.desc.cell_count; ++i) {
            state->volume.density[i] = 0.0f;
            state->volume.velocity_x[i] = velocity_x;
            state->volume.velocity_y[i] = velocity_y;
            state->volume.velocity_z[i] = 0.0f;
            state->volume.pressure[i] = 0.0f;
        }
    }
    backend_3d_scaffold_mark_fluid_dirty(state);
}

static bool backend_3d_scaffold_export_snapshot(const SimRuntimeBackend *backend,
                                                double time,
                                                const char *path) {
    (void)backend;
    (void)time;
    (void)path;
    return false;
}

static bool backend_3d_scaffold_step_compatibility_slice(SimRuntimeBackend *backend, int delta_z) {
    SimRuntimeBackend3DScaffold *state = backend_3d_scaffold_state(backend);
    if (!state || delta_z == 0) return false;
    return backend_3d_scaffold_set_slice_z(state, state->compatibility_slice_z + delta_z);
}

static bool backend_3d_scaffold_get_fluid_view_2d(const SimRuntimeBackend *backend,
                                                  SceneFluidFieldView2D *out_view) {
    const SimRuntimeBackend3DScaffold *state = backend_3d_scaffold_state_const(backend);
    if (!state || !out_view) return false;
    if (!backend_3d_scaffold_sync_fluid_slice((SimRuntimeBackend3DScaffold *)state)) return false;
    out_view->width = state->volume.desc.grid_w;
    out_view->height = state->volume.desc.grid_h;
    out_view->cell_count = state->volume.desc.slice_cell_count;
    out_view->density = state->slice_density;
    out_view->velocity_x = state->slice_velocity_x;
    out_view->velocity_y = state->slice_velocity_y;
    out_view->pressure = state->slice_pressure;
    return true;
}

static const SimRuntimeBackendOps g_backend_3d_scaffold_ops = {
    .destroy = backend_3d_scaffold_destroy,
    .valid = backend_3d_scaffold_valid,
    .clear = backend_3d_scaffold_clear,
    .apply_brush_sample = backend_3d_scaffold_apply_brush_sample,
    .build_static_obstacles = backend_3d_scaffold_build_static_obstacles,
    .build_emitter_masks = backend_3d_scaffold_build_emitter_masks,
    .mark_emitters_dirty = backend_3d_scaffold_mark_emitters_dirty,
    .build_obstacles = backend_3d_scaffold_build_obstacles,
    .mark_obstacles_dirty = backend_3d_scaffold_mark_obstacles_dirty,
    .rasterize_dynamic_obstacles = backend_3d_scaffold_rasterize_dynamic_obstacles,
    .apply_emitters = backend_3d_scaffold_apply_emitters,
    .apply_boundary_flows = backend_3d_scaffold_apply_boundary_flows,
    .enforce_boundary_flows = backend_3d_scaffold_enforce_boundary_flows,
    .enforce_obstacles = backend_3d_scaffold_enforce_obstacles,
    .step = backend_3d_scaffold_step,
    .inject_object_motion = backend_3d_scaffold_inject_object_motion,
    .reset_transient_state = backend_3d_scaffold_reset_transient_state,
    .seed_uniform_velocity_2d = backend_3d_scaffold_seed_uniform_velocity_2d,
    .export_snapshot = backend_3d_scaffold_export_snapshot,
    .get_fluid_view_2d = backend_3d_scaffold_get_fluid_view_2d,
    .get_obstacle_view_2d = backend_3d_scaffold_get_obstacle_view_2d,
    .get_debug_volume_view_3d = backend_3d_scaffold_get_debug_volume_view_3d,
    .get_volume_export_view_3d = backend_3d_scaffold_get_volume_export_view_3d,
    .get_report = backend_3d_scaffold_get_report,
    .get_compatibility_slice_activity = backend_3d_scaffold_get_compatibility_slice_activity,
    .step_compatibility_slice = backend_3d_scaffold_step_compatibility_slice,
    .get_domain_desc_3d = backend_3d_scaffold_get_domain_desc_3d,
    .debug_zero_dense_mirror_3d = backend_3d_scaffold_debug_zero_dense_mirror_3d,
    .debug_zero_obstacle_dense_cache_3d = backend_3d_scaffold_debug_zero_obstacle_dense_cache_3d,
    .debug_write_volume_cell_3d = backend_3d_scaffold_debug_write_volume_cell_3d,
    .debug_reset_volume_truth_3d = backend_3d_scaffold_debug_reset_volume_truth_3d,
    .debug_note_atmospheric_warm_start_3d =
        backend_3d_scaffold_debug_note_atmospheric_warm_start_3d,
};

SimRuntimeBackend *sim_runtime_backend_3d_scaffold_create(const AppConfig *cfg,
                                                          const FluidScenePreset *preset,
                                                          const SimModeRoute *mode_route,
                                                          const PhysicsSimRuntimeVisualBootstrap *runtime_visual) {
    SimRuntimeBackend *backend = NULL;
    SimRuntimeBackend3DScaffold *state = NULL;
    SimRuntime3DDomainDesc desc = {0};
    size_t slice_cells = 0;

    (void)mode_route;

    if (!cfg) return NULL;
    if (!sim_runtime_3d_domain_desc_resolve(cfg, preset, runtime_visual, &desc)) return NULL;

    backend = (SimRuntimeBackend *)calloc(1, sizeof(*backend));
    state = (SimRuntimeBackend3DScaffold *)calloc(1, sizeof(*state));
    if (!backend || !state) {
        free(state);
        free(backend);
        return NULL;
    }
    backend->impl = state;

    state->volume.desc = desc;
    state->solver_volume.desc = desc;
    state->export_volume_cache.desc = desc;
    if (!sim_runtime_3d_brick_store_init(&state->brick_store, &desc, SCAFFOLD_BRICK_SIZE)) {
        backend_3d_scaffold_destroy(backend);
        return NULL;
    }
    if (!core_sim_loop_init(&state->solver_loop, NULL)) {
        backend_3d_scaffold_destroy(backend);
        return NULL;
    }
    if (desc.cell_count <= SCAFFOLD_DENSE_MIRROR_MAX_CELLS &&
        !sim_runtime_3d_volume_init(&state->volume, &desc)) {
        backend_3d_scaffold_destroy(backend);
        return NULL;
    }

    sim_runtime_obstacle_contract_default(&state->obstacle_contract);
    if (runtime_visual && runtime_visual->scene_up.valid) {
        state->scene_up_valid = true;
        state->scene_up_x = (float)runtime_visual->scene_up.direction.x;
        state->scene_up_y = (float)runtime_visual->scene_up.direction.y;
        state->scene_up_z = (float)runtime_visual->scene_up.direction.z;
        state->scene_up_source = runtime_visual->scene_up.source;
    } else {
        state->scene_up_source = PHYSICS_SIM_RUNTIME_SCENE_UP_NONE;
    }
    slice_cells = desc.slice_cell_count;
    state->compatibility_slice_z = desc.grid_d / 2;
    state->fluid_slice_dirty = true;
    state->obstacle_volume_dirty = true;
    state->obstacle_slice_dirty = true;
    state->export_volume_cache_dirty = true;
    state->slice_density = (float *)calloc(slice_cells, sizeof(float));
    state->slice_velocity_x = (float *)calloc(slice_cells, sizeof(float));
    state->slice_velocity_y = (float *)calloc(slice_cells, sizeof(float));
    state->slice_pressure = (float *)calloc(slice_cells, sizeof(float));
    state->obstacle_occupancy = (uint8_t *)calloc(desc.cell_count, sizeof(uint8_t));
    state->obstacle_bricks = (void **)calloc(state->brick_store.brick_count, sizeof(void *));
    state->obstacle_brick_flags =
        (uint8_t *)calloc(state->brick_store.brick_count, sizeof(uint8_t));
    state->slice_solid_mask = (uint8_t *)calloc(slice_cells, sizeof(uint8_t));
    state->slice_obstacle_velocity_x = (float *)calloc(slice_cells, sizeof(float));
    state->slice_obstacle_velocity_y = (float *)calloc(slice_cells, sizeof(float));
    state->slice_obstacle_distance = (float *)calloc(slice_cells, sizeof(float));
    if (!state->slice_density ||
        !state->slice_velocity_x ||
        !state->slice_velocity_y ||
        !state->slice_pressure ||
        !state->obstacle_occupancy ||
        !state->obstacle_bricks ||
        !state->obstacle_brick_flags ||
        !state->slice_solid_mask ||
        !state->slice_obstacle_velocity_x ||
        !state->slice_obstacle_velocity_y ||
        !state->slice_obstacle_distance) {
        backend_3d_scaffold_destroy(backend);
        return NULL;
    }

    backend_3d_scaffold_reset(state);
    (void)backend_3d_scaffold_seed_atmosphere(state, preset);
    state->runtime_solver_region_cell_budget = app_config_3d_solver_region_cell_budget(cfg);
    state->runtime_solver_region_cell_budget_overridden =
        app_config_3d_solver_region_cell_budget_overridden(cfg);
    state->runtime_solver_max_velocity_displacement_cells_limit =
        app_config_3d_max_velocity_displacement_cells(cfg);
    state->runtime_solver_max_velocity_displacement_cells_limit_overridden =
        app_config_3d_max_velocity_displacement_cells_overridden(cfg);

    backend->kind = SIM_RUNTIME_BACKEND_KIND_FLUID_3D_SCAFFOLD;
    backend->impl = state;
    backend->ops = &g_backend_3d_scaffold_ops;
    return backend;
}
