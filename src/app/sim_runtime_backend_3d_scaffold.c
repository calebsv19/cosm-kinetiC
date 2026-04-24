#include "app/sim_runtime_backend.h"

#include "app/scene_state.h"
#include "app/sim_runtime_backend_3d_scaffold_internal.h"
#include "app/sim_runtime_3d_domain.h"
#include "app/sim_runtime_3d_solver.h"
#include "app/sim_runtime_obstacle.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static const float SCAFFOLD_BRUSH_DENSITY = 20.0f;
static const float SCAFFOLD_BRUSH_VEL_SCALE = 35.0f;
static const float SCAFFOLD_BRUSH_VELOCITY_DENSITY = 4.0f;

static SimRuntimeBackend3DScaffold *backend_3d_scaffold_state(SimRuntimeBackend *backend) {
    return backend ? (SimRuntimeBackend3DScaffold *)backend->impl : NULL;
}

static const SimRuntimeBackend3DScaffold *backend_3d_scaffold_state_const(
    const SimRuntimeBackend *backend) {
    return backend ? (const SimRuntimeBackend3DScaffold *)backend->impl : NULL;
}

static void backend_3d_scaffold_reset(SimRuntimeBackend3DScaffold *state) {
    if (!state) return;
    sim_runtime_3d_volume_clear(&state->volume);
    sim_runtime_3d_solver_scratch_clear(&state->solver_scratch);
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
    state->fluid_slice_dirty = true;
}

static void backend_3d_scaffold_update_debug_volume_stats(SimRuntimeBackend3DScaffold *state) {
    const float density_threshold = 0.0001f;
    float axis_x = 0.0f;
    float axis_y = 0.0f;
    float axis_z = 0.0f;
    float axis_len = 0.0f;
    double scene_up_velocity_weighted_sum = 0.0;
    double scene_up_density_weight = 0.0;
    if (!state) return;
    if (!state->debug_volume_stats_dirty) return;

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

    for (size_t i = 0; i < state->volume.desc.cell_count; ++i) {
        float density = state->volume.density[i];
        float velocity_x = state->volume.velocity_x[i];
        float velocity_y = state->volume.velocity_y[i];
        float velocity_z = state->volume.velocity_z[i];
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
    size_t slice_start = 0;
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
    slice_start = (size_t)state->compatibility_slice_z * desc->slice_cell_count;
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
        sim_runtime_3d_volume_destroy(&state->volume);
        sim_runtime_3d_solver_scratch_destroy(&state->solver_scratch);
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
           state->volume.density &&
           state->volume.velocity_x &&
           state->volume.velocity_y &&
           state->volume.velocity_z &&
           state->volume.pressure &&
           state->solver_scratch.density_prev &&
           state->solver_scratch.velocity_x_prev &&
           state->solver_scratch.velocity_y_prev &&
           state->solver_scratch.velocity_z_prev &&
           state->solver_scratch.pressure_prev &&
           state->solver_scratch.divergence &&
           state->obstacle_occupancy &&
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
    size_t idx = 0;
    int gx = 0;
    int gy = 0;
    float inv_w = 0.0f;
    float inv_h = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    if (!state || !cfg || !sample || state->volume.desc.cell_count == 0) return false;

    backend_3d_scaffold_window_to_grid(state, cfg, sample->x, sample->y, &gx, &gy);
    idx = sim_runtime_3d_volume_index(&state->volume.desc, gx, gy, state->compatibility_slice_z);
    inv_w = (float)(cfg->window_w > 0 ? cfg->window_w : 1);
    inv_h = (float)(cfg->window_h > 0 ? cfg->window_h : 1);
    vx = (sample->vx / inv_w) * SCAFFOLD_BRUSH_VEL_SCALE;
    vy = (sample->vy / inv_h) * SCAFFOLD_BRUSH_VEL_SCALE;

    switch (sample->mode) {
    case BRUSH_MODE_VELOCITY:
        state->volume.velocity_x[idx] += vx;
        state->volume.velocity_y[idx] += vy;
        state->volume.density[idx] += SCAFFOLD_BRUSH_VELOCITY_DENSITY;
        break;
    case BRUSH_MODE_DENSITY:
    default:
        state->volume.density[idx] += SCAFFOLD_BRUSH_DENSITY;
        state->volume.velocity_x[idx] += vx * 0.25f;
        state->volume.velocity_y[idx] += vy * 0.25f;
        break;
    }

    state->debug_volume_stats_dirty = true;
    state->fluid_slice_dirty = true;
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
    SimRuntimeBackend3DScaffold *state = backend_3d_scaffold_state(backend);
    SimRuntime3DForceAxis scene_up_axis = {0};
    if (state && state->obstacle_volume_dirty) {
        backend_3d_scaffold_build_obstacles(backend, scene);
    }
    if (!state || !cfg || dt <= 0.0) return;
    if (state->scene_up_valid) {
        scene_up_axis.valid = true;
        scene_up_axis.x = state->scene_up_x;
        scene_up_axis.y = state->scene_up_y;
        scene_up_axis.z = state->scene_up_z;
    }
    if (!sim_runtime_3d_solver_step_first_pass(&state->volume,
                                               &state->solver_scratch,
                                               state->obstacle_occupancy,
                                               &scene_up_axis,
                                               cfg,
                                               dt)) {
        return;
    }
    state->debug_volume_stats_dirty = true;
    state->fluid_slice_dirty = true;
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
    for (size_t i = 0; i < state->volume.desc.cell_count; ++i) {
        state->volume.velocity_x[i] = velocity_x;
        state->volume.velocity_y[i] = velocity_y;
    }
    state->debug_volume_stats_dirty = true;
    state->fluid_slice_dirty = true;
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

static bool backend_3d_scaffold_get_debug_volume_view_3d(const SimRuntimeBackend *backend,
                                                         SceneDebugVolumeView3D *out_view) {
    SimRuntimeBackend3DScaffold *state = backend_3d_scaffold_state((SimRuntimeBackend *)backend);
    const SimRuntime3DDomainDesc *desc = NULL;
    if (!state || !out_view) return false;
    desc = &state->volume.desc;
    if (state->obstacle_volume_dirty) {
        backend_3d_scaffold_build_obstacles((SimRuntimeBackend *)backend, NULL);
    }
    backend_3d_scaffold_update_debug_volume_stats(state);
    *out_view = (SceneDebugVolumeView3D){
        .width = desc->grid_w,
        .height = desc->grid_h,
        .depth = desc->grid_d,
        .cell_count = desc->cell_count,
        .world_min_x = desc->world_min_x,
        .world_min_y = desc->world_min_y,
        .world_min_z = desc->world_min_z,
        .world_max_x = desc->world_max_x,
        .world_max_y = desc->world_max_y,
        .world_max_z = desc->world_max_z,
        .voxel_size = desc->voxel_size,
        .density = state->volume.density,
        .solid_mask = state->obstacle_occupancy,
    };
    return true;
}

static bool backend_3d_scaffold_get_volume_export_view_3d(const SimRuntimeBackend *backend,
                                                          SceneFluidVolumeExportView3D *out_view) {
    SimRuntimeBackend3DScaffold *state = backend_3d_scaffold_state((SimRuntimeBackend *)backend);
    const SimRuntime3DDomainDesc *desc = NULL;
    if (!state || !out_view) return false;
    desc = &state->volume.desc;
    if (state->obstacle_volume_dirty) {
        backend_3d_scaffold_build_obstacles((SimRuntimeBackend *)backend, NULL);
    }
    *out_view = (SceneFluidVolumeExportView3D){
        .width = desc->grid_w,
        .height = desc->grid_h,
        .depth = desc->grid_d,
        .cell_count = desc->cell_count,
        .origin_x = desc->world_min_x,
        .origin_y = desc->world_min_y,
        .origin_z = desc->world_min_z,
        .voxel_size = desc->voxel_size,
        .scene_up_valid = state->scene_up_valid,
        .scene_up_x = state->scene_up_x,
        .scene_up_y = state->scene_up_y,
        .scene_up_z = state->scene_up_z,
        .density = state->volume.density,
        .velocity_x = state->volume.velocity_x,
        .velocity_y = state->volume.velocity_y,
        .velocity_z = state->volume.velocity_z,
        .pressure = state->volume.pressure,
        .solid_mask = state->obstacle_occupancy,
    };
    return true;
}

static bool backend_3d_scaffold_get_report(const SimRuntimeBackend *backend,
                                           SimRuntimeBackendReport *out_report) {
    const SimRuntimeBackend3DScaffold *state = backend_3d_scaffold_state_const(backend);
    const SimRuntime3DDomainDesc *desc = NULL;
    if (!state || !out_report) return false;
    desc = &state->volume.desc;
    if (state->obstacle_volume_dirty) {
        backend_3d_scaffold_build_obstacles((SimRuntimeBackend *)backend, NULL);
    }
    backend_3d_scaffold_update_debug_volume_stats((SimRuntimeBackend3DScaffold *)state);

    *out_report = (SimRuntimeBackendReport){
        .kind = SIM_RUNTIME_BACKEND_KIND_FLUID_3D_SCAFFOLD,
        .domain_w = desc->grid_w,
        .domain_h = desc->grid_h,
        .domain_d = desc->grid_d,
        .cell_count = desc->cell_count,
        .volumetric_emitters_free_live = true,
        .volumetric_emitters_attached_live = true,
        .volumetric_obstacles_live = true,
        .full_3d_solver_live = true,
        .world_bounds_valid = true,
        .world_min_x = desc->world_min_x,
        .world_min_y = desc->world_min_y,
        .world_min_z = desc->world_min_z,
        .world_max_x = desc->world_max_x,
        .world_max_y = desc->world_max_y,
        .world_max_z = desc->world_max_z,
        .voxel_size = desc->voxel_size,
        .scene_up_valid = state->scene_up_valid,
        .scene_up_x = state->scene_up_x,
        .scene_up_y = state->scene_up_y,
        .scene_up_z = state->scene_up_z,
        .scene_up_source = state->scene_up_source,
        .compatibility_view_2d_available = true,
        .compatibility_view_2d_derived = true,
        .compatibility_slice_z = state->compatibility_slice_z,
        .secondary_debug_slice_stack_live = true,
        .secondary_debug_slice_stack_radius = 2,
        .debug_volume_view_3d_available = true,
        .debug_volume_active_density_cells = state->debug_volume_active_density_cells,
        .debug_volume_solid_cells = state->debug_volume_solid_cells,
        .debug_volume_max_density = state->debug_volume_max_density,
        .debug_volume_max_velocity_magnitude = state->debug_volume_max_velocity_magnitude,
        .debug_volume_scene_up_velocity_valid = state->debug_volume_scene_up_velocity_valid,
        .debug_volume_scene_up_velocity_avg = state->debug_volume_scene_up_velocity_avg,
        .debug_volume_scene_up_velocity_peak = state->debug_volume_scene_up_velocity_peak,
        .emitter_step_emitters_applied = state->emitter_step_emitters_applied,
        .emitter_step_free_emitters_applied = state->emitter_step_free_emitters_applied,
        .emitter_step_attached_emitters_applied = state->emitter_step_attached_emitters_applied,
        .emitter_step_affected_cells = state->emitter_step_affected_cells,
        .emitter_step_last_footprint_cells = state->emitter_step_last_footprint_cells,
        .emitter_step_density_delta = state->emitter_step_density_delta,
        .emitter_step_velocity_magnitude_delta = state->emitter_step_velocity_magnitude_delta,
    };
    return true;
}

static bool backend_3d_scaffold_get_compatibility_slice_activity(const SimRuntimeBackend *backend,
                                                                 int slice_z,
                                                                 bool *out_has_fluid,
                                                                 bool *out_has_obstacles) {
    SimRuntimeBackend3DScaffold *state = backend_3d_scaffold_state((SimRuntimeBackend *)backend);
    const SimRuntime3DDomainDesc *desc = NULL;
    size_t slice_start = 0;
    bool has_fluid = false;
    bool has_obstacles = false;
    if (!state) return false;
    desc = &state->volume.desc;
    if (slice_z < 0 || slice_z >= desc->grid_d) return false;
    if (state->obstacle_volume_dirty) {
        backend_3d_scaffold_build_obstacles((SimRuntimeBackend *)backend, NULL);
    }
    slice_start = (size_t)slice_z * desc->slice_cell_count;
    for (size_t i = 0; i < desc->slice_cell_count; ++i) {
        if (!has_fluid && state->volume.density[slice_start + i] > 0.0001f) {
            has_fluid = true;
        }
        if (!has_obstacles && state->obstacle_occupancy[slice_start + i]) {
            has_obstacles = true;
        }
        if (has_fluid && has_obstacles) break;
    }
    if (out_has_fluid) *out_has_fluid = has_fluid;
    if (out_has_obstacles) *out_has_obstacles = has_obstacles;
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

    if (!sim_runtime_3d_volume_init(&state->volume, &desc)) {
        backend_3d_scaffold_destroy(backend);
        return NULL;
    }
    if (!sim_runtime_3d_solver_scratch_init(&state->solver_scratch, &desc)) {
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
    state->slice_density = (float *)calloc(slice_cells, sizeof(float));
    state->slice_velocity_x = (float *)calloc(slice_cells, sizeof(float));
    state->slice_velocity_y = (float *)calloc(slice_cells, sizeof(float));
    state->slice_pressure = (float *)calloc(slice_cells, sizeof(float));
    state->obstacle_occupancy = (uint8_t *)calloc(desc.cell_count, sizeof(uint8_t));
    state->slice_solid_mask = (uint8_t *)calloc(slice_cells, sizeof(uint8_t));
    state->slice_obstacle_velocity_x = (float *)calloc(slice_cells, sizeof(float));
    state->slice_obstacle_velocity_y = (float *)calloc(slice_cells, sizeof(float));
    state->slice_obstacle_distance = (float *)calloc(slice_cells, sizeof(float));
    if (!state->slice_density ||
        !state->slice_velocity_x ||
        !state->slice_velocity_y ||
        !state->slice_pressure ||
        !state->obstacle_occupancy ||
        !state->slice_solid_mask ||
        !state->slice_obstacle_velocity_x ||
        !state->slice_obstacle_velocity_y ||
        !state->slice_obstacle_distance) {
        backend_3d_scaffold_destroy(backend);
        return NULL;
    }

    backend_3d_scaffold_reset(state);

    backend->kind = SIM_RUNTIME_BACKEND_KIND_FLUID_3D_SCAFFOLD;
    backend->impl = state;
    backend->ops = &g_backend_3d_scaffold_ops;
    return backend;
}
