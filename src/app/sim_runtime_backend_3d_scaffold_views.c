#include "app/sim_runtime_backend_3d_scaffold_internal.h"

static SimRuntimeBackend3DScaffold *backend_3d_scaffold_state(SimRuntimeBackend *backend) {
    return backend ? (SimRuntimeBackend3DScaffold *)backend->impl : NULL;
}

static const SimRuntimeBackend3DScaffold *backend_3d_scaffold_state_const(
    const SimRuntimeBackend *backend) {
    return backend ? (const SimRuntimeBackend3DScaffold *)backend->impl : NULL;
}

bool backend_3d_scaffold_get_debug_volume_view_3d(const SimRuntimeBackend *backend,
                                                  SceneDebugVolumeView3D *out_view) {
    SimRuntimeBackend3DScaffold *state = backend_3d_scaffold_state((SimRuntimeBackend *)backend);
    const SimRuntime3DDomainDesc *desc = NULL;
    const float *density = NULL;
    const uint8_t *solid_mask = NULL;
    if (!state || !out_view) return false;
    desc = &state->volume.desc;
    if (state->obstacle_volume_dirty) {
        backend_3d_scaffold_build_obstacles((SimRuntimeBackend *)backend, NULL);
    }
    backend_3d_scaffold_update_debug_volume_stats(state);
    if (backend_3d_scaffold_dense_mirror_live(state)) {
        if (!backend_3d_scaffold_ensure_obstacle_dense_cache(state)) return false;
        density = state->volume.density;
        solid_mask = state->obstacle_occupancy;
    } else {
        if (!backend_3d_scaffold_ensure_export_cache(state)) return false;
        density = state->export_volume_cache.density;
        solid_mask = state->export_solid_mask_cache;
    }
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
        .density = density,
        .solid_mask = solid_mask,
    };
    return true;
}

bool backend_3d_scaffold_get_volume_export_view_3d(const SimRuntimeBackend *backend,
                                                   SceneFluidVolumeExportView3D *out_view) {
    SimRuntimeBackend3DScaffold *state = backend_3d_scaffold_state((SimRuntimeBackend *)backend);
    const SimRuntime3DDomainDesc *desc = NULL;
    if (!state || !out_view) return false;
    desc = &state->volume.desc;
    if (state->obstacle_volume_dirty) {
        backend_3d_scaffold_build_obstacles((SimRuntimeBackend *)backend, NULL);
    }
    if (backend_3d_scaffold_dense_mirror_live(state)) {
        if (!backend_3d_scaffold_ensure_obstacle_dense_cache(state)) return false;
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
    if (!backend_3d_scaffold_ensure_export_cache(state)) return false;
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
        .density = state->export_volume_cache.density,
        .velocity_x = state->export_volume_cache.velocity_x,
        .velocity_y = state->export_volume_cache.velocity_y,
        .velocity_z = state->export_volume_cache.velocity_z,
        .pressure = state->export_volume_cache.pressure,
        .solid_mask = state->export_solid_mask_cache,
    };
    return true;
}

bool backend_3d_scaffold_get_report(const SimRuntimeBackend *backend,
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
        .requested_major_axis_cells = desc->requested_major_axis_cells,
        .applied_major_axis_cells = desc->applied_major_axis_cells,
        .requested_depth_cells = desc->requested_depth_cells,
        .applied_depth_cells = desc->applied_depth_cells,
        .depth_policy = desc->depth_policy,
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
        .secondary_debug_slice_stack_radius = 3,
        .initial_state_source = state->initial_state_source,
        .atmospheric_seeded = state->atmospheric_seeded,
        .atmospheric_seed = state->atmospheric_seed,
        .atmospheric_seeded_cell_count = state->atmospheric_seeded_cell_count,
        .atmospheric_seed_max_density = state->atmospheric_seed_max_density,
        .atmospheric_seed_max_velocity_magnitude =
            state->atmospheric_seed_max_velocity_magnitude,
        .atmospheric_warm_start_loaded = state->atmospheric_warm_start_loaded,
        .atmospheric_warm_start_source_kind = state->atmospheric_warm_start_source_kind,
        .atmospheric_warm_start_w = state->atmospheric_warm_start_w,
        .atmospheric_warm_start_h = state->atmospheric_warm_start_h,
        .atmospheric_warm_start_d = state->atmospheric_warm_start_d,
        .atmospheric_warm_start_cell_count = state->atmospheric_warm_start_cell_count,
        .atmospheric_warm_start_active_density_cells =
            state->atmospheric_warm_start_active_density_cells,
        .atmospheric_warm_start_solid_cells = state->atmospheric_warm_start_solid_cells,
        .atmospheric_warm_start_max_density = state->atmospheric_warm_start_max_density,
        .atmospheric_warm_start_max_velocity_magnitude =
            state->atmospheric_warm_start_max_velocity_magnitude,
        .runtime_allocated_brick_count = state->brick_store.allocated_brick_count,
        .runtime_active_brick_count = state->brick_store.allocated_brick_count,
        .runtime_active_region_cell_count = state->runtime_last_active_region_cell_count,
        .runtime_solver_region_cell_count = state->runtime_last_solver_region_cell_count,
        .runtime_solver_cluster_count = state->runtime_last_solver_cluster_count,
        .runtime_solver_max_cluster_cell_count = state->runtime_last_solver_max_cluster_cell_count,
        .runtime_solver_solved_cluster_count = state->runtime_last_solver_solved_cluster_count,
        .runtime_solver_skipped_cluster_count = state->runtime_last_solver_skipped_cluster_count,
        .runtime_solver_skipped_solver_cell_count = state->runtime_last_solver_skipped_solver_cell_count,
        .runtime_export_cache_materialization_count = state->runtime_export_cache_materialization_count,
        .runtime_solver_region_cell_budget = state->runtime_solver_region_cell_budget,
        .runtime_solver_region_cell_budget_overridden =
            state->runtime_solver_region_cell_budget_overridden,
        .runtime_solver_max_velocity_displacement_cells_limit =
            state->runtime_solver_max_velocity_displacement_cells_limit,
        .runtime_solver_max_velocity_displacement_cells_limit_overridden =
            state->runtime_solver_max_velocity_displacement_cells_limit_overridden,
        .runtime_solver_velocity_clamp_cell_count = state->runtime_solver_velocity_clamp_cell_count,
        .runtime_dense_mirror_live = backend_3d_scaffold_dense_mirror_live(state),
        .runtime_solver_region_guard_triggered = state->runtime_solver_region_guard_triggered,
        .runtime_solver_cluster_limit_reached = state->runtime_solver_cluster_limit_reached,
        .runtime_solver_max_velocity_magnitude_pre_clamp =
            state->runtime_solver_max_velocity_magnitude_pre_clamp,
        .runtime_solver_max_velocity_magnitude_post_clamp =
            state->runtime_solver_max_velocity_magnitude_post_clamp,
        .runtime_solver_max_velocity_displacement_cells_pre_clamp =
            state->runtime_solver_max_velocity_displacement_cells_pre_clamp,
        .runtime_solver_max_velocity_displacement_cells_post_clamp =
            state->runtime_solver_max_velocity_displacement_cells_post_clamp,
        .runtime_solver_max_abs_divergence_after_project =
            state->runtime_solver_max_abs_divergence_after_project,
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

bool backend_3d_scaffold_get_compatibility_slice_activity(const SimRuntimeBackend *backend,
                                                          int slice_z,
                                                          bool *out_has_fluid,
                                                          bool *out_has_obstacles) {
    SimRuntimeBackend3DScaffold *state = backend_3d_scaffold_state((SimRuntimeBackend *)backend);
    const SimRuntime3DDomainDesc *desc = NULL;
    bool has_fluid = false;
    bool has_obstacles = false;
    if (!state) return false;
    desc = &state->volume.desc;
    if (slice_z < 0 || slice_z >= desc->grid_d) return false;
    if (state->obstacle_volume_dirty) {
        backend_3d_scaffold_build_obstacles((SimRuntimeBackend *)backend, NULL);
    }
    if (backend_3d_scaffold_dense_mirror_live(state)) {
        if (!backend_3d_scaffold_ensure_obstacle_dense_cache(state)) return false;
        size_t slice_start = (size_t)slice_z * desc->slice_cell_count;
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
    for (int y = 0; y < desc->grid_h; ++y) {
        for (int x = 0; x < desc->grid_w; ++x) {
            float density = 0.0f;
            sim_runtime_3d_brick_store_get_cell(&state->brick_store,
                                                x,
                                                y,
                                                slice_z,
                                                &density,
                                                NULL,
                                                NULL,
                                                NULL,
                                                NULL);
            if (!has_fluid && density > 0.0001f) {
                has_fluid = true;
            }
            if (!has_obstacles &&
                backend_3d_scaffold_obstacle_cell_solid(state, x, y, slice_z)) {
                has_obstacles = true;
            }
            if (has_fluid && has_obstacles) break;
        }
        if (has_fluid && has_obstacles) break;
    }
    if (out_has_fluid) *out_has_fluid = has_fluid;
    if (out_has_obstacles) *out_has_obstacles = has_obstacles;
    return true;
}
