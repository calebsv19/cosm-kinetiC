#include "app/sim_runtime_backend.h"
#include "app/scene_state.h"
#include "app/atmospheric/atmospheric_field.h"
#include "sim_runtime_backend_3d_test_support.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static bool nearly_equal(float a, float b) {
    float diff = a - b;
    if (diff < 0.0f) diff = -diff;
    return diff < 0.0001f;
}

SimRuntimeBackend *sim_runtime_backend_2d_create(const AppConfig *cfg,
                                                 const FluidScenePreset *preset,
                                                 const SimModeRoute *mode_route,
                                                 const PhysicsSimRuntimeVisualBootstrap *runtime_visual) {
    (void)cfg;
    (void)preset;
    (void)mode_route;
    (void)runtime_visual;
    return NULL;
}

static bool test_3d_backend_reports_xyz_domain_and_compatibility_slice(void) {
    AppConfig cfg = {0};
    SimModeRoute route = {
        .backend_lane = SIM_BACKEND_CONTROLLED_3D,
        .requested_space_mode = SPACE_MODE_3D,
        .projection_space_mode = SPACE_MODE_2D,
    };
    PhysicsSimRuntimeVisualBootstrap visual = {0};
    SimRuntimeBackend *backend = NULL;
    SimRuntimeBackendReport report = {0};
    SceneFluidFieldView2D fluid = {0};

    cfg.quality_index = 1;
    cfg.grid_w = 128;
    cfg.grid_h = 128;
    cfg.grid_d = 0;
    cfg.window_w = 640;
    cfg.window_h = 480;

    visual.scene_domain.enabled = true;
    visual.scene_domain_authored = true;
    visual.scene_domain.min.x = -1.0;
    visual.scene_domain.min.y = -2.0;
    visual.scene_domain.min.z = -0.5;
    visual.scene_domain.max.x = 2.0;
    visual.scene_domain.max.y = 2.0;
    visual.scene_domain.max.z = 0.5;
    visual.scene_up.valid = true;
    visual.scene_up.direction = (CoreObjectVec3){0.0, 0.0, 1.0};
    visual.scene_up.source = PHYSICS_SIM_RUNTIME_SCENE_UP_FALLBACK_POSITIVE_Z;

    backend = sim_runtime_backend_create(&cfg, NULL, &route, &visual);
    if (!backend) return false;
    if (!sim_runtime_backend_get_report(backend, &report)) return false;
    if (report.kind != SIM_RUNTIME_BACKEND_KIND_FLUID_3D_SCAFFOLD) return false;
    if (report.requested_major_axis_cells != 128) return false;
    if (report.applied_major_axis_cells != 128) return false;
    if (report.requested_depth_cells != 0) return false;
    if (report.applied_depth_cells != 32) return false;
    if (report.domain_w != 96) return false;
    if (report.domain_h != 128) return false;
    if (report.domain_d != 32) return false;
    if (report.cell_count != (size_t)96 * (size_t)128 * (size_t)32) return false;
    if (!report.volumetric_emitters_free_live) return false;
    if (!report.volumetric_emitters_attached_live) return false;
    if (!report.volumetric_obstacles_live) return false;
    if (!report.full_3d_solver_live) return false;
    if (!report.world_bounds_valid) return false;
    if (!nearly_equal(report.world_min_x, -1.0f)) return false;
    if (!nearly_equal(report.world_max_y, 2.0f)) return false;
    if (!nearly_equal(report.world_max_z, 0.5f)) return false;
    if (!nearly_equal(report.voxel_size, 4.0f / 128.0f)) return false;
    if (!report.scene_up_valid) return false;
    if (!nearly_equal(report.scene_up_x, 0.0f)) return false;
    if (!nearly_equal(report.scene_up_y, 0.0f)) return false;
    if (!nearly_equal(report.scene_up_z, 1.0f)) return false;
    if (report.scene_up_source != PHYSICS_SIM_RUNTIME_SCENE_UP_FALLBACK_POSITIVE_Z) return false;
    if (!report.compatibility_view_2d_available) return false;
    if (!report.compatibility_view_2d_derived) return false;
    if (report.compatibility_slice_z != 16) return false;
    if (!report.secondary_debug_slice_stack_live) return false;
    if (report.secondary_debug_slice_stack_radius != 3) return false;
    if (!report.runtime_dense_mirror_live) return false;
    if (report.runtime_allocated_brick_count != 0u) return false;
    if (report.runtime_active_brick_count != 0u) return false;
    if (report.runtime_active_region_cell_count != 0u) return false;
    if (report.runtime_solver_region_cell_count != 0u) return false;
    if (report.runtime_solver_cluster_count != 0u) return false;
    if (report.runtime_solver_max_cluster_cell_count != 0u) return false;
    if (report.runtime_solver_solved_cluster_count != 0u) return false;
    if (report.runtime_solver_skipped_cluster_count != 0u) return false;
    if (report.runtime_solver_skipped_solver_cell_count != 0u) return false;
    if (report.runtime_export_cache_materialization_count != 0u) return false;
    if (report.runtime_solver_region_cell_budget == 0u) return false;
    if (report.runtime_solver_region_cell_budget_overridden) return false;
    if (!nearly_equal(report.runtime_solver_max_velocity_displacement_cells_limit,
                      app_config_3d_max_velocity_displacement_cells(&cfg))) {
        return false;
    }
    if (report.runtime_solver_max_velocity_displacement_cells_limit_overridden) return false;
    if (report.runtime_solver_velocity_clamp_cell_count != 0u) return false;
    if (report.runtime_solver_region_guard_triggered) return false;
    if (report.runtime_solver_cluster_limit_reached) return false;
    if (!nearly_equal(report.runtime_solver_max_velocity_magnitude_pre_clamp, 0.0f)) return false;
    if (!nearly_equal(report.runtime_solver_max_velocity_magnitude_post_clamp, 0.0f)) return false;
    if (!nearly_equal(report.runtime_solver_max_velocity_displacement_cells_pre_clamp, 0.0f)) return false;
    if (!nearly_equal(report.runtime_solver_max_velocity_displacement_cells_post_clamp, 0.0f)) return false;
    if (!nearly_equal(report.runtime_solver_max_abs_divergence_after_project, 0.0f)) return false;
    if (!report.debug_volume_view_3d_available) return false;
    if (report.debug_volume_solid_cells == 0) return false;
    if (report.debug_volume_active_density_cells != 0) return false;
    if (!nearly_equal(report.debug_volume_max_density, 0.0f)) return false;
    if (!nearly_equal(report.debug_volume_max_velocity_magnitude, 0.0f)) return false;
    if (!report.debug_volume_scene_up_velocity_valid) return false;
    if (!nearly_equal(report.debug_volume_scene_up_velocity_avg, 0.0f)) return false;
    if (!nearly_equal(report.debug_volume_scene_up_velocity_peak, 0.0f)) return false;
    if (report.initial_state_source != SIM_RUNTIME_INITIAL_STATE_SOURCE_BLANK) return false;
    if (!sim_runtime_backend_get_fluid_view_2d(backend, &fluid)) return false;
    if (fluid.width != report.domain_w) return false;
    if (fluid.height != report.domain_h) return false;
    if (fluid.cell_count != (size_t)report.domain_w * (size_t)report.domain_h) return false;

    sim_runtime_backend_destroy(backend);
    return true;
}

static bool test_3d_backend_seeds_atmospheric_preset_sparse_truth(void) {
    AppConfig cfg = {0};
    FluidScenePreset preset = {0};
    SimModeRoute route = {
        .backend_lane = SIM_BACKEND_CONTROLLED_3D,
        .requested_space_mode = SPACE_MODE_3D,
        .projection_space_mode = SPACE_MODE_2D,
    };
    SimRuntimeBackend *backend = NULL;
    SimRuntimeBackendReport report = {0};
    SceneDebugVolumeView3D volume = {0};
    bool has_fluid = false;

    cfg.grid_w = 48;
    cfg.grid_h = 48;
    cfg.grid_d = 24;
    cfg.window_w = 640;
    cfg.window_h = 480;
    cfg.space_mode = SPACE_MODE_3D;

    preset.domain = SCENE_DOMAIN_ATMOSPHERIC;
    preset.dimension_mode = SCENE_DIMENSION_MODE_3D;
    preset.domain_width = 1.0f;
    preset.domain_height = 1.0f;
    preset.atmosphere = atmospheric_preset_default_settings();
    preset.atmosphere.base_density = 0.1f;

    backend = sim_runtime_backend_create(&cfg, &preset, &route, NULL);
    if (!backend) return false;
    if (!sim_runtime_backend_get_report(backend, &report)) return false;
    if (!report.atmospheric_seeded) return false;
    if (report.initial_state_source !=
        SIM_RUNTIME_INITIAL_STATE_SOURCE_ATMOSPHERIC_STANDALONE) {
        return false;
    }
    if (report.atmospheric_seed != preset.atmosphere.seed) return false;
    if (report.atmospheric_seeded_cell_count == 0u) return false;
    if (report.atmospheric_seed_max_density <= 0.0f) return false;
    if (report.atmospheric_seed_max_velocity_magnitude <= 0.0f) return false;
    if (report.debug_volume_active_density_cells == 0u) return false;
    if (report.runtime_allocated_brick_count == 0u) return false;
    if (!sim_runtime_backend_get_debug_volume_view_3d(backend, &volume)) return false;
    if (!volume.density || volume.cell_count == 0u) return false;
    if (!sim_runtime_backend_get_compatibility_slice_activity(backend,
                                                              report.compatibility_slice_z,
                                                              &has_fluid,
                                                              NULL)) {
        return false;
    }
    if (!has_fluid) return false;

    sim_runtime_backend_destroy(backend);
    return true;
}

static bool test_3d_fluid_mode_can_opt_into_atmospheric_initial_state(void) {
    AppConfig cfg = {0};
    FluidScenePreset preset = {0};
    SimModeRoute route = {
        .simulation_mode = SIM_MODE_BOX,
        .backend_lane = SIM_BACKEND_CONTROLLED_3D,
        .requested_space_mode = SPACE_MODE_3D,
        .projection_space_mode = SPACE_MODE_2D,
    };
    SimRuntimeBackend *backend = NULL;
    SimRuntimeBackendReport report = {0};

    cfg.grid_w = 32;
    cfg.grid_h = 32;
    cfg.grid_d = 16;
    cfg.window_w = 640;
    cfg.window_h = 480;
    cfg.space_mode = SPACE_MODE_3D;
    cfg.sim_mode = SIM_MODE_BOX;

    preset.domain = SCENE_DOMAIN_BOX;
    preset.dimension_mode = SCENE_DIMENSION_MODE_3D;
    preset.domain_width = 1.0f;
    preset.domain_height = 1.0f;
    preset.atmospheric_initial_state_enabled = true;
    preset.atmosphere = atmospheric_preset_default_settings();
    preset.atmosphere.base_density = 0.1f;

    if (atmospheric_initial_state_source(&preset) !=
        ATMOSPHERIC_INITIAL_STATE_OPTIONAL_LAYER) {
        return false;
    }

    backend = sim_runtime_backend_create(&cfg, &preset, &route, NULL);
    if (!backend) return false;
    if (!sim_runtime_backend_get_report(backend, &report)) return false;
    if (!report.atmospheric_seeded) return false;
    if (report.initial_state_source !=
        SIM_RUNTIME_INITIAL_STATE_SOURCE_ATMOSPHERIC_OPTIONAL_LAYER) {
        return false;
    }
    if (report.atmospheric_seed != preset.atmosphere.seed) return false;
    if (report.atmospheric_seeded_cell_count == 0u) return false;
    if (report.atmospheric_seed_max_density <= 0.0f) return false;
    if (report.atmospheric_seed_max_velocity_magnitude <= 0.0f) return false;
    if (report.atmospheric_warm_start_loaded) return false;

    sim_runtime_backend_destroy(backend);
    return true;
}

static bool test_3d_fluid_mode_without_opt_in_stays_blank_initially(void) {
    AppConfig cfg = {0};
    FluidScenePreset preset = {0};
    SimModeRoute route = {
        .simulation_mode = SIM_MODE_BOX,
        .backend_lane = SIM_BACKEND_CONTROLLED_3D,
        .requested_space_mode = SPACE_MODE_3D,
        .projection_space_mode = SPACE_MODE_2D,
    };
    SimRuntimeBackend *backend = NULL;
    SimRuntimeBackendReport report = {0};

    cfg.grid_w = 32;
    cfg.grid_h = 32;
    cfg.grid_d = 16;
    cfg.window_w = 640;
    cfg.window_h = 480;
    cfg.space_mode = SPACE_MODE_3D;
    cfg.sim_mode = SIM_MODE_BOX;

    preset.domain = SCENE_DOMAIN_BOX;
    preset.dimension_mode = SCENE_DIMENSION_MODE_3D;
    preset.domain_width = 1.0f;
    preset.domain_height = 1.0f;
    preset.atmospheric_initial_state_enabled = false;
    preset.atmosphere = atmospheric_preset_default_settings();
    preset.atmosphere.base_density = 0.1f;

    if (atmospheric_initial_state_source(&preset) != ATMOSPHERIC_INITIAL_STATE_NONE) {
        return false;
    }

    backend = sim_runtime_backend_create(&cfg, &preset, &route, NULL);
    if (!backend) return false;
    if (!sim_runtime_backend_get_report(backend, &report)) return false;
    if (report.atmospheric_seeded) return false;
    if (report.initial_state_source != SIM_RUNTIME_INITIAL_STATE_SOURCE_BLANK) return false;
    if (report.atmospheric_seeded_cell_count != 0u) return false;
    if (report.debug_volume_active_density_cells != 0u) return false;

    sim_runtime_backend_destroy(backend);
    return true;
}

static bool test_3d_backend_warm_start_source_overrides_procedural_report(void) {
    AppConfig cfg = {0};
    FluidScenePreset preset = {0};
    SimModeRoute route = {
        .simulation_mode = SIM_MODE_ATMOSPHERIC,
        .backend_lane = SIM_BACKEND_CONTROLLED_3D,
        .requested_space_mode = SPACE_MODE_3D,
        .projection_space_mode = SPACE_MODE_2D,
    };
    SimRuntimeBackend *backend = NULL;
    SimRuntimeBackendReport report = {0};

    cfg.grid_w = 16;
    cfg.grid_h = 16;
    cfg.grid_d = 8;
    cfg.window_w = 640;
    cfg.window_h = 480;
    cfg.space_mode = SPACE_MODE_3D;
    cfg.sim_mode = SIM_MODE_ATMOSPHERIC;

    preset.domain = SCENE_DOMAIN_ATMOSPHERIC;
    preset.dimension_mode = SCENE_DIMENSION_MODE_3D;
    preset.domain_width = 1.0f;
    preset.domain_height = 1.0f;
    preset.atmosphere = atmospheric_preset_default_settings();
    preset.atmosphere.base_density = 0.1f;

    backend = sim_runtime_backend_create(&cfg, &preset, &route, NULL);
    if (!backend) return false;
    if (!sim_runtime_backend_get_report(backend, &report)) return false;
    if (report.initial_state_source !=
        SIM_RUNTIME_INITIAL_STATE_SOURCE_ATMOSPHERIC_STANDALONE) {
        return false;
    }

    if (!sim_runtime_backend_debug_note_atmospheric_warm_start_3d(
            backend,
            ATMOSPHERIC_WARM_START_SOURCE_VF3D_RAW,
            16,
            16,
            8,
            (size_t)16 * (size_t)16 * (size_t)8,
            12u,
            4u,
            2.0f,
            3.0f)) {
        return false;
    }
    if (!sim_runtime_backend_get_report(backend, &report)) return false;
    if (report.initial_state_source != SIM_RUNTIME_INITIAL_STATE_SOURCE_ATMOSPHERIC_WARM_START) {
        return false;
    }
    if (report.atmospheric_seeded) return false;
    if (!report.atmospheric_warm_start_loaded) return false;
    if (report.atmospheric_warm_start_source_kind != ATMOSPHERIC_WARM_START_SOURCE_VF3D_RAW) {
        return false;
    }
    if (report.atmospheric_warm_start_active_density_cells != 12u) return false;
    if (!nearly_equal(report.atmospheric_warm_start_max_density, 2.0f)) return false;
    if (strcmp(sim_runtime_initial_state_source_label(report.initial_state_source),
               "warm start") != 0) {
        return false;
    }

    sim_runtime_backend_destroy(backend);
    return true;
}

static bool test_3d_backend_step_uses_sparse_truth_even_when_dense_mirror_is_stale(void) {
    AppConfig cfg = {0};
    SimModeRoute route = {
        .backend_lane = SIM_BACKEND_CONTROLLED_3D,
        .requested_space_mode = SPACE_MODE_3D,
        .projection_space_mode = SPACE_MODE_2D,
    };
    PhysicsSimRuntimeVisualBootstrap visual = {0};
    SimRuntimeBackend *backend = NULL;
    SimRuntimeBackend3DScaffoldTestView impl = {0};
    SimRuntimeBackendReport report = {0};
    size_t live_density_cells = 0;

    cfg.quality_index = 5;
    cfg.grid_w = 64;
    cfg.grid_h = 64;
    cfg.window_w = 640;
    cfg.window_h = 480;
    cfg.fluid_solver_iterations = 4;
    cfg.velocity_damping = 0.0f;
    cfg.density_diffusion = 0.0f;
    cfg.density_decay = 0.0f;
    cfg.fluid_buoyancy_force = 0.0f;

    visual.scene_domain.enabled = true;
    visual.scene_domain_authored = true;
    visual.scene_domain.min.x = -1.0;
    visual.scene_domain.min.y = -1.0;
    visual.scene_domain.min.z = -1.0;
    visual.scene_domain.max.x = 1.0;
    visual.scene_domain.max.y = 1.0;
    visual.scene_domain.max.z = 1.0;

    backend = sim_runtime_backend_create(&cfg, NULL, &route, &visual);
    if (!backend) return false;
    if (!sim_runtime_backend_3d_test_write_cell(backend, 32, 32, 32, 2.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0u)) {
        return false;
    }
    if (!sim_runtime_backend_3d_test_zero_dense_mirror(backend)) return false;

    sim_runtime_backend_step(backend, NULL, &cfg, 0.1);
    if (!sim_runtime_backend_3d_test_view_refresh(backend, &impl)) return false;
    if (!sim_runtime_backend_get_report(backend, &report)) return false;

    for (size_t i = 0; i < impl.volume.desc.cell_count; ++i) {
        if (impl.volume.density[i] > 0.0001f) {
            live_density_cells += 1u;
        }
    }

    if (live_density_cells == 0u) return false;
    if (!report.runtime_dense_mirror_live) return false;
    if (report.runtime_active_region_cell_count == 0u) return false;
    if (report.runtime_solver_region_cell_count == 0u) return false;
    if (report.runtime_solver_cluster_count != 1u) return false;
    if (report.runtime_solver_max_cluster_cell_count != report.runtime_solver_region_cell_count) {
        return false;
    }
    if (report.runtime_solver_solved_cluster_count != 1u) return false;
    if (report.runtime_solver_skipped_cluster_count != 0u) return false;
    if (report.runtime_solver_skipped_solver_cell_count != 0u) return false;
    if (report.runtime_solver_region_cell_budget_overridden) return false;
    if (report.runtime_solver_max_velocity_displacement_cells_limit_overridden) return false;
    if (report.runtime_solver_region_guard_triggered) return false;
    if (report.runtime_solver_cluster_limit_reached) return false;

    sim_runtime_backend_destroy(backend);
    return true;
}

static bool test_3d_backend_reports_explicit_depth_contract(void) {
    AppConfig cfg = {0};
    SimModeRoute route = {
        .backend_lane = SIM_BACKEND_CONTROLLED_3D,
        .requested_space_mode = SPACE_MODE_3D,
        .projection_space_mode = SPACE_MODE_2D,
    };
    PhysicsSimRuntimeVisualBootstrap visual = {0};
    SimRuntimeBackend *backend = NULL;
    SimRuntimeBackendReport report = {0};

    cfg.grid_w = 192;
    cfg.grid_h = 192;
    cfg.grid_d = 32;
    cfg.window_w = 640;
    cfg.window_h = 480;

    visual.scene_domain.enabled = true;
    visual.scene_domain_authored = true;
    visual.scene_domain.min.x = -2.0;
    visual.scene_domain.min.y = -2.0;
    visual.scene_domain.min.z = -2.0;
    visual.scene_domain.max.x = 2.0;
    visual.scene_domain.max.y = 2.0;
    visual.scene_domain.max.z = 2.0;

    backend = sim_runtime_backend_create(&cfg, NULL, &route, &visual);
    if (!backend) return false;
    if (!sim_runtime_backend_get_report(backend, &report)) return false;
    if (report.requested_major_axis_cells != 192) return false;
    if (report.applied_major_axis_cells != 32) return false;
    if (report.requested_depth_cells != 32) return false;
    if (report.applied_depth_cells != 32) return false;
    if (report.depth_policy != SIM_RUNTIME_3D_DEPTH_POLICY_CONFIGURED_DEPTH_CELLS) return false;
    if (report.domain_w != 32) return false;
    if (report.domain_h != 32) return false;
    if (report.domain_d != 32) return false;
    if (!nearly_equal(report.voxel_size, 4.0f / 32.0f)) return false;

    sim_runtime_backend_destroy(backend);
    return true;
}

static bool test_3d_backend_report_exposes_guard_override_contract(void) {
    AppConfig cfg = {0};
    SimModeRoute route = {
        .backend_lane = SIM_BACKEND_CONTROLLED_3D,
        .requested_space_mode = SPACE_MODE_3D,
        .projection_space_mode = SPACE_MODE_2D,
    };
    PhysicsSimRuntimeVisualBootstrap visual = {0};
    SimRuntimeBackend *backend = NULL;
    SimRuntimeBackendReport report = {0};

    cfg.grid_w = 64;
    cfg.grid_h = 64;
    cfg.grid_d = 32;
    cfg.window_w = 640;
    cfg.window_h = 480;
    cfg.fluid_3d_solver_region_cell_budget = 12345;
    cfg.fluid_3d_max_velocity_displacement_cells = 0.75f;

    visual.scene_domain.enabled = true;
    visual.scene_domain_authored = true;
    visual.scene_domain.min.x = -1.0;
    visual.scene_domain.min.y = -1.0;
    visual.scene_domain.min.z = -1.0;
    visual.scene_domain.max.x = 1.0;
    visual.scene_domain.max.y = 1.0;
    visual.scene_domain.max.z = 1.0;

    backend = sim_runtime_backend_create(&cfg, NULL, &route, &visual);
    if (!backend) return false;
    if (!sim_runtime_backend_get_report(backend, &report)) return false;
    if (report.runtime_solver_region_cell_budget != 12345u) return false;
    if (!report.runtime_solver_region_cell_budget_overridden) return false;
    if (!nearly_equal(report.runtime_solver_max_velocity_displacement_cells_limit, 0.75f)) {
        return false;
    }
    if (!report.runtime_solver_max_velocity_displacement_cells_limit_overridden) return false;

    sim_runtime_backend_destroy(backend);
    return true;
}

static bool test_3d_backend_debug_volume_view_exposes_density_and_obstacle_truth(void) {
    AppConfig cfg = {0};
    SimModeRoute route = {
        .backend_lane = SIM_BACKEND_CONTROLLED_3D,
        .requested_space_mode = SPACE_MODE_3D,
        .projection_space_mode = SPACE_MODE_2D,
    };
    PhysicsSimRuntimeVisualBootstrap visual = {0};
    SimRuntimeBackend *backend = NULL;
    SceneDebugVolumeView3D volume = {0};
    StrokeSample sample = {0};
    size_t active_density_cells = 0;
    size_t solid_cells = 0;
    SimRuntimeBackendReport report = {0};

    cfg.quality_index = 5;
    cfg.grid_w = 64;
    cfg.grid_h = 64;
    cfg.window_w = 640;
    cfg.window_h = 480;

    visual.scene_domain.enabled = true;
    visual.scene_domain_authored = true;
    visual.scene_domain.min.x = -1.0;
    visual.scene_domain.min.y = -1.0;
    visual.scene_domain.min.z = -1.0;
    visual.scene_domain.max.x = 1.0;
    visual.scene_domain.max.y = 1.0;
    visual.scene_domain.max.z = 1.0;

    backend = sim_runtime_backend_create(&cfg, NULL, &route, &visual);
    if (!backend) return false;

    sample.x = cfg.window_w / 2;
    sample.y = cfg.window_h / 2;
    sample.mode = BRUSH_MODE_DENSITY;
    if (!sim_runtime_backend_apply_brush_sample(backend, &cfg, &sample)) return false;

    if (!sim_runtime_backend_get_debug_volume_view_3d(backend, &volume)) return false;
    if (volume.width != 64 || volume.height != 64 || volume.depth != 64) return false;
    if (volume.cell_count != (size_t)64 * (size_t)64 * (size_t)64) return false;
    if (!nearly_equal(volume.world_min_x, -1.0f)) return false;
    if (!nearly_equal(volume.world_max_z, 1.0f)) return false;
    if (!nearly_equal(volume.voxel_size, 0.03125f)) return false;
    if (!volume.density || !volume.solid_mask) return false;

    for (size_t i = 0; i < volume.cell_count; ++i) {
        if (volume.density[i] > 0.0001f) active_density_cells += 1;
        if (volume.solid_mask[i]) solid_cells += 1;
    }
    if (active_density_cells == 0) return false;
    if (solid_cells == 0) return false;
    if (!sim_runtime_backend_get_report(backend, &report)) return false;
    if (report.debug_volume_active_density_cells != active_density_cells) return false;
    if (report.debug_volume_solid_cells != solid_cells) return false;
    if (report.debug_volume_max_density <= 0.0f) return false;

    sim_runtime_backend_destroy(backend);
    return true;
}

static bool test_3d_backend_volume_export_view_exposes_authoritative_xyz_fields(void) {
    AppConfig cfg = {0};
    SimModeRoute route = {
        .backend_lane = SIM_BACKEND_CONTROLLED_3D,
        .requested_space_mode = SPACE_MODE_3D,
        .projection_space_mode = SPACE_MODE_2D,
    };
    PhysicsSimRuntimeVisualBootstrap visual = {0};
    SimRuntimeBackend *backend = NULL;
    SimRuntimeBackend3DScaffoldTestView impl = {0};
    SceneFluidVolumeExportView3D export_view = {0};
    size_t idx = 0;

    cfg.quality_index = 5;
    cfg.grid_w = 64;
    cfg.grid_h = 64;
    cfg.window_w = 640;
    cfg.window_h = 480;

    visual.scene_domain.enabled = true;
    visual.scene_domain_authored = true;
    visual.scene_domain.min.x = -1.0;
    visual.scene_domain.min.y = -2.0;
    visual.scene_domain.min.z = -3.0;
    visual.scene_domain.max.x = 1.0;
    visual.scene_domain.max.y = 2.0;
    visual.scene_domain.max.z = 3.0;
    visual.scene_up.valid = true;
    visual.scene_up.direction = (CoreObjectVec3){0.0, 0.0, 1.0};
    visual.scene_up.source = PHYSICS_SIM_RUNTIME_SCENE_UP_FALLBACK_POSITIVE_Z;

    backend = sim_runtime_backend_create(&cfg, NULL, &route, &visual);
    if (!backend) return false;

    sim_runtime_backend_build_obstacles(backend, NULL);
    if (!sim_runtime_backend_3d_test_view_refresh(backend, &impl)) return false;
    idx = sim_runtime_3d_volume_index(&impl.volume.desc, 2, 3, 4);
    if (!sim_runtime_backend_3d_test_write_cell(backend, 2, 3, 4, 1.25f, 2.0f, -3.0f, 4.5f, 6.0f, 1u)) {
        return false;
    }

    if (!sim_runtime_backend_get_volume_export_view_3d(backend, &export_view)) return false;
    if (export_view.width != impl.volume.desc.grid_w) return false;
    if (export_view.height != impl.volume.desc.grid_h) return false;
    if (export_view.depth != impl.volume.desc.grid_d) return false;
    if (export_view.cell_count != impl.volume.desc.cell_count) return false;
    if (!nearly_equal(export_view.origin_x, impl.volume.desc.world_min_x)) return false;
    if (!nearly_equal(export_view.origin_y, impl.volume.desc.world_min_y)) return false;
    if (!nearly_equal(export_view.origin_z, impl.volume.desc.world_min_z)) return false;
    if (!nearly_equal(export_view.voxel_size, impl.volume.desc.voxel_size)) return false;
    if (!export_view.scene_up_valid) return false;
    if (!nearly_equal(export_view.scene_up_x, 0.0f)) return false;
    if (!nearly_equal(export_view.scene_up_y, 0.0f)) return false;
    if (!nearly_equal(export_view.scene_up_z, 1.0f)) return false;
    if (!export_view.density || !export_view.velocity_x || !export_view.velocity_y ||
        !export_view.velocity_z || !export_view.pressure || !export_view.solid_mask) {
        return false;
    }
    if (!nearly_equal(export_view.density[idx], 1.25f)) return false;
    if (!nearly_equal(export_view.velocity_x[idx], 2.0f)) return false;
    if (!nearly_equal(export_view.velocity_y[idx], -3.0f)) return false;
    if (!nearly_equal(export_view.velocity_z[idx], 4.5f)) return false;
    if (!nearly_equal(export_view.pressure[idx], 6.0f)) return false;
    if (export_view.solid_mask[idx] != 1u) return false;

    sim_runtime_backend_destroy(backend);
    return true;
}

static bool test_3d_backend_report_exposes_scene_up_velocity_truth(void) {
    AppConfig cfg = {0};
    FluidScenePreset preset = {0};
    SimModeRoute route = {
        .backend_lane = SIM_BACKEND_CONTROLLED_3D,
        .requested_space_mode = SPACE_MODE_3D,
        .projection_space_mode = SPACE_MODE_2D,
    };
    PhysicsSimRuntimeVisualBootstrap visual = {0};
    SimRuntimeBackend *backend = NULL;
    SimRuntimeBackendReport report = {0};
    SceneState scene = {0};

    cfg.quality_index = 5;
    cfg.grid_w = 64;
    cfg.grid_h = 64;
    cfg.window_w = 640;
    cfg.window_h = 480;
    cfg.fluid_solver_iterations = 8;
    cfg.velocity_damping = 0.000006f;
    cfg.density_diffusion = 0.0f;
    cfg.density_decay = 0.0f;
    cfg.fluid_buoyancy_force = 1.0f;
    cfg.emitter_density_multiplier = 1.0f;

    visual.scene_domain.enabled = true;
    visual.scene_domain_authored = true;
    visual.scene_domain.min.x = -1.0;
    visual.scene_domain.min.y = -1.0;
    visual.scene_domain.min.z = -1.0;
    visual.scene_domain.max.x = 1.0;
    visual.scene_domain.max.y = 1.0;
    visual.scene_domain.max.z = 1.0;
    visual.scene_up.valid = true;
    visual.scene_up.direction = (CoreObjectVec3){0.0, 0.0, 1.0};
    visual.scene_up.source = PHYSICS_SIM_RUNTIME_SCENE_UP_FALLBACK_POSITIVE_Z;

    preset.dimension_mode = SCENE_DIMENSION_MODE_3D;
    preset.emitter_count = 1;
    preset.emitters[0] = (FluidEmitter){
        .type = EMITTER_DENSITY_SOURCE,
        .position_x = 0.50f,
        .position_y = 0.50f,
        .position_z = 0.30f,
        .radius = 0.06f,
        .strength = 8.0f,
        .dir_x = 0.0f,
        .dir_y = 0.0f,
        .dir_z = 1.0f,
        .attached_object = -1,
        .attached_import = -1,
    };

    backend = sim_runtime_backend_create(&cfg, &preset, &route, &visual);
    if (!backend) return false;

    scene.backend = backend;
    scene.preset = &preset;
    scene.config = &cfg;
    scene.emitters_enabled = true;
    scene.runtime_visual = visual;

    sim_runtime_backend_apply_emitters(backend, &scene, 0.1);
    for (int i = 0; i < 4; ++i) {
        sim_runtime_backend_step(backend, &scene, &cfg, 0.25);
    }

    if (!sim_runtime_backend_get_report(backend, &report)) return false;
    if (!report.debug_volume_view_3d_available) return false;
    if (!report.debug_volume_scene_up_velocity_valid) return false;
    if (report.debug_volume_active_density_cells == 0) return false;
    if (report.debug_volume_max_density <= 0.0f) return false;
    if (report.debug_volume_max_velocity_magnitude <= 0.0f) return false;
    if (report.debug_volume_scene_up_velocity_avg <= 0.0f) return false;
    if (report.debug_volume_scene_up_velocity_peak <= 0.0f) return false;

    sim_runtime_backend_destroy(backend);
    return true;
}

static bool test_3d_backend_slice_activity_query_reports_fluid_and_obstacles(void) {
    AppConfig cfg = {0};
    SimModeRoute route = {
        .backend_lane = SIM_BACKEND_CONTROLLED_3D,
        .requested_space_mode = SPACE_MODE_3D,
        .projection_space_mode = SPACE_MODE_2D,
    };
    PhysicsSimRuntimeVisualBootstrap visual = {0};
    SimRuntimeBackend *backend = NULL;
    SimRuntimeBackendReport report = {0};
    StrokeSample sample = {0};
    bool has_fluid = false;
    bool has_obstacles = false;

    cfg.quality_index = 1;
    cfg.grid_w = 128;
    cfg.grid_h = 128;
    cfg.window_w = 640;
    cfg.window_h = 480;

    visual.scene_domain.enabled = true;
    visual.scene_domain_authored = true;
    visual.scene_domain.min.x = -1.0;
    visual.scene_domain.min.y = -1.0;
    visual.scene_domain.min.z = -1.0;
    visual.scene_domain.max.x = 1.0;
    visual.scene_domain.max.y = 1.0;
    visual.scene_domain.max.z = 1.0;

    backend = sim_runtime_backend_create(&cfg, NULL, &route, &visual);
    if (!backend) return false;
    if (!sim_runtime_backend_get_report(backend, &report)) return false;

    sample.x = cfg.window_w / 2;
    sample.y = cfg.window_h / 2;
    sample.mode = BRUSH_MODE_DENSITY;
    if (!sim_runtime_backend_apply_brush_sample(backend, &cfg, &sample)) return false;

    if (!sim_runtime_backend_get_compatibility_slice_activity(
            backend, report.compatibility_slice_z, &has_fluid, &has_obstacles)) {
        return false;
    }
    if (!has_fluid) return false;

    has_fluid = false;
    has_obstacles = false;
    if (!sim_runtime_backend_get_compatibility_slice_activity(backend, 0, &has_fluid, &has_obstacles)) {
        return false;
    }
    if (!has_obstacles) return false;

    sim_runtime_backend_destroy(backend);
    return true;
}

static bool test_3d_backend_sparse_report_tracks_clustered_solver_regions(void) {
    AppConfig cfg = {0};
    SimModeRoute route = {
        .backend_lane = SIM_BACKEND_CONTROLLED_3D,
        .requested_space_mode = SPACE_MODE_3D,
        .projection_space_mode = SPACE_MODE_2D,
    };
    PhysicsSimRuntimeVisualBootstrap visual = {0};
    SimRuntimeBackend *backend = NULL;
    SimRuntime3DDomainDesc desc = {0};
    SceneFluidVolumeExportView3D export_view = {0};
    SimRuntimeBackendReport report = {0};
    size_t first_idx = 0;
    size_t last_idx = 0;

    cfg.grid_w = 192;
    cfg.grid_h = 192;
    cfg.grid_d = 192;
    cfg.window_w = 640;
    cfg.window_h = 480;
    cfg.fluid_solver_iterations = 4;
    cfg.velocity_damping = 0.0f;
    cfg.density_diffusion = 0.0f;
    cfg.density_decay = 0.0f;
    cfg.fluid_buoyancy_force = 0.0f;

    visual.scene_domain.enabled = true;
    visual.scene_domain_authored = true;
    visual.scene_domain.min.x = -2.0;
    visual.scene_domain.min.y = -2.0;
    visual.scene_domain.min.z = -2.0;
    visual.scene_domain.max.x = 2.0;
    visual.scene_domain.max.y = 2.0;
    visual.scene_domain.max.z = 2.0;

    backend = sim_runtime_backend_create(&cfg, NULL, &route, &visual);
    if (!backend) return false;
    if (!sim_runtime_backend_get_domain_desc_3d(backend, &desc)) return false;
    if (!sim_runtime_backend_get_report(backend, &report)) return false;
    if (report.runtime_dense_mirror_live) return false;
    if (desc.cell_count <= report.runtime_solver_region_cell_budget) return false;
    if (report.runtime_export_cache_materialization_count != 0u) return false;

    if (!sim_runtime_backend_3d_test_write_cell(backend, 0, 0, 0, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0u)) {
        return false;
    }
    if (!sim_runtime_backend_3d_test_write_cell(backend,
                                                desc.grid_w - 1,
                                                desc.grid_h - 1,
                                                desc.grid_d - 1,
                                                2.0f,
                                                0.0f,
                                                0.0f,
                                                0.0f,
                                                0.0f,
                                                0u)) {
        return false;
    }

    if (!sim_runtime_backend_get_report(backend, &report)) return false;
    if (report.runtime_export_cache_materialization_count != 0u) return false;
    if (!sim_runtime_backend_get_volume_export_view_3d(backend, &export_view)) return false;
    if (!sim_runtime_backend_get_report(backend, &report)) return false;
    if (report.runtime_export_cache_materialization_count != 1u) return false;

    sim_runtime_backend_step(backend, NULL, &cfg, 0.1);
    if (!sim_runtime_backend_get_report(backend, &report)) return false;
    if (report.runtime_solver_region_guard_triggered) return false;
    if (report.runtime_solver_cluster_count != 2u) return false;
    if (report.runtime_solver_cluster_limit_reached) return false;
    if (report.runtime_active_region_cell_count >= desc.cell_count) return false;
    if (report.runtime_solver_region_cell_count >= desc.cell_count) return false;
    if (report.runtime_solver_max_cluster_cell_count == 0u) return false;
    if (report.runtime_solver_max_cluster_cell_count >= desc.cell_count) return false;
    if (report.runtime_solver_solved_cluster_count != 2u) return false;
    if (report.runtime_solver_skipped_cluster_count != 0u) return false;
    if (report.runtime_solver_skipped_solver_cell_count != 0u) return false;
    if (report.runtime_allocated_brick_count < 2u) return false;
    if (report.runtime_export_cache_materialization_count != 1u) return false;
    if (!sim_runtime_backend_get_volume_export_view_3d(backend, &export_view)) return false;
    if (!sim_runtime_backend_get_report(backend, &report)) return false;
    if (report.runtime_export_cache_materialization_count != 2u) return false;

    first_idx = sim_runtime_3d_volume_index(&desc, 0, 0, 0);
    last_idx = sim_runtime_3d_volume_index(&desc,
                                           desc.grid_w - 1,
                                           desc.grid_h - 1,
                                           desc.grid_d - 1);
    if (!nearly_equal(export_view.density[first_idx], 1.0f)) return false;
    if (!nearly_equal(export_view.density[last_idx], 2.0f)) return false;

    sim_runtime_backend_destroy(backend);
    return true;
}

static bool test_3d_backend_sparse_report_guards_one_oversized_connected_cluster(void) {
    AppConfig cfg = {0};
    SimModeRoute route = {
        .backend_lane = SIM_BACKEND_CONTROLLED_3D,
        .requested_space_mode = SPACE_MODE_3D,
        .projection_space_mode = SPACE_MODE_2D,
    };
    PhysicsSimRuntimeVisualBootstrap visual = {0};
    SimRuntimeBackend *backend = NULL;
    SimRuntime3DDomainDesc desc = {0};
    SceneFluidVolumeExportView3D export_view = {0};
    SimRuntimeBackendReport report = {0};

    cfg.grid_w = 192;
    cfg.grid_h = 192;
    cfg.grid_d = 192;
    cfg.window_w = 640;
    cfg.window_h = 480;
    cfg.fluid_solver_iterations = 4;
    cfg.velocity_damping = 0.0f;
    cfg.density_diffusion = 0.0f;
    cfg.density_decay = 0.0f;
    cfg.fluid_buoyancy_force = 0.0f;

    visual.scene_domain.enabled = true;
    visual.scene_domain_authored = true;
    visual.scene_domain.min.x = -2.0;
    visual.scene_domain.min.y = -2.0;
    visual.scene_domain.min.z = -2.0;
    visual.scene_domain.max.x = 2.0;
    visual.scene_domain.max.y = 2.0;
    visual.scene_domain.max.z = 2.0;

    backend = sim_runtime_backend_create(&cfg, NULL, &route, &visual);
    if (!backend) return false;
    if (!sim_runtime_backend_get_domain_desc_3d(backend, &desc)) return false;
    if (!sim_runtime_backend_get_report(backend, &report)) return false;
    if (desc.cell_count <= report.runtime_solver_region_cell_budget) return false;

    for (int z = 0; z < desc.grid_d; z += 8) {
        for (int y = 0; y < desc.grid_h; y += 8) {
            for (int x = 0; x < desc.grid_w; x += 8) {
                if (!sim_runtime_backend_3d_test_write_cell(
                        backend, x, y, z, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0u)) {
                    return false;
                }
            }
        }
    }

    sim_runtime_backend_step(backend, NULL, &cfg, 0.1);
    if (!sim_runtime_backend_get_report(backend, &report)) return false;
    if (!report.runtime_solver_region_guard_triggered) return false;
    if (report.runtime_solver_cluster_count != 1u) return false;
    if (report.runtime_active_region_cell_count != desc.cell_count) return false;
    if (report.runtime_solver_region_cell_count != desc.cell_count) return false;
    if (report.runtime_solver_max_cluster_cell_count != desc.cell_count) return false;
    if (report.runtime_solver_solved_cluster_count != 0u) return false;
    if (report.runtime_solver_skipped_cluster_count != 1u) return false;
    if (report.runtime_solver_skipped_solver_cell_count != desc.cell_count) return false;

    if (!sim_runtime_backend_get_volume_export_view_3d(backend, &export_view)) return false;
    if (!nearly_equal(export_view.density[0], 1.0f)) return false;

    sim_runtime_backend_destroy(backend);
    return true;
}

static bool test_3d_backend_report_tracks_velocity_safety_guard_metrics(void) {
    AppConfig cfg = {0};
    SimModeRoute route = {
        .backend_lane = SIM_BACKEND_CONTROLLED_3D,
        .requested_space_mode = SPACE_MODE_3D,
        .projection_space_mode = SPACE_MODE_2D,
    };
    PhysicsSimRuntimeVisualBootstrap visual = {0};
    SimRuntimeBackend *backend = NULL;
    SimRuntimeBackendReport report = {0};

    cfg.quality_index = 5;
    cfg.grid_w = 64;
    cfg.grid_h = 64;
    cfg.window_w = 640;
    cfg.window_h = 480;
    cfg.fluid_solver_iterations = 4;
    cfg.velocity_damping = 0.0f;
    cfg.density_diffusion = 0.0f;
    cfg.density_decay = 0.0f;
    cfg.fluid_buoyancy_force = 0.0f;

    visual.scene_domain.enabled = true;
    visual.scene_domain_authored = true;
    visual.scene_domain.min.x = -1.0;
    visual.scene_domain.min.y = -1.0;
    visual.scene_domain.min.z = -1.0;
    visual.scene_domain.max.x = 1.0;
    visual.scene_domain.max.y = 1.0;
    visual.scene_domain.max.z = 1.0;

    backend = sim_runtime_backend_create(&cfg, NULL, &route, &visual);
    if (!backend) return false;
    if (!sim_runtime_backend_3d_test_write_cell(
            backend, 32, 32, 32, 0.0f, 100.0f, 0.0f, 0.0f, 0.0f, 0u)) {
        return false;
    }

    sim_runtime_backend_step(backend, NULL, &cfg, 1.0);
    if (!sim_runtime_backend_get_report(backend, &report)) return false;
    if (report.runtime_solver_solved_cluster_count != 1u) return false;
    if (report.runtime_solver_velocity_clamp_cell_count == 0u) return false;
    if (report.runtime_solver_max_velocity_displacement_cells_pre_clamp <=
        report.runtime_solver_max_velocity_displacement_cells_post_clamp) {
        return false;
    }
    if (report.runtime_solver_max_velocity_displacement_cells_post_clamp >
        report.runtime_solver_max_velocity_displacement_cells_limit + 0.0001f) {
        return false;
    }
    if (report.runtime_solver_max_velocity_magnitude_pre_clamp <=
        report.runtime_solver_max_velocity_magnitude_post_clamp) {
        return false;
    }
    if (report.runtime_solver_max_abs_divergence_after_project < 0.0f) return false;

    sim_runtime_backend_destroy(backend);
    return true;
}

static bool test_3d_backend_live_slice_selection_changes_report_and_view(void) {
    AppConfig cfg = {0};
    SimModeRoute route = {
        .backend_lane = SIM_BACKEND_CONTROLLED_3D,
        .requested_space_mode = SPACE_MODE_3D,
        .projection_space_mode = SPACE_MODE_2D,
    };
    PhysicsSimRuntimeVisualBootstrap visual = {0};
    SimRuntimeBackend *backend = NULL;
    SimRuntimeBackendReport report = {0};
    SceneFluidFieldView2D fluid = {0};
    StrokeSample sample = {0};
    size_t active_cells = 0;

    cfg.quality_index = 1;
    cfg.grid_w = 128;
    cfg.grid_h = 128;
    cfg.window_w = 640;
    cfg.window_h = 480;

    visual.scene_domain.enabled = true;
    visual.scene_domain_authored = true;
    visual.scene_domain.min.x = -1.0;
    visual.scene_domain.min.y = -1.0;
    visual.scene_domain.min.z = -1.0;
    visual.scene_domain.max.x = 1.0;
    visual.scene_domain.max.y = 1.0;
    visual.scene_domain.max.z = 1.0;

    backend = sim_runtime_backend_create(&cfg, NULL, &route, &visual);
    if (!backend) return false;

    sample.x = cfg.window_w / 2;
    sample.y = cfg.window_h / 2;
    sample.mode = BRUSH_MODE_DENSITY;
    if (!sim_runtime_backend_apply_brush_sample(backend, &cfg, &sample)) return false;
    if (!sim_runtime_backend_get_fluid_view_2d(backend, &fluid)) return false;
    for (size_t i = 0; i < fluid.cell_count; ++i) {
        if (fluid.density[i] > 0.0001f) {
            active_cells += 1;
        }
    }
    if (active_cells == 0) return false;

    if (!sim_runtime_backend_step_compatibility_slice(backend, 1)) return false;
    if (!sim_runtime_backend_get_report(backend, &report)) return false;
    if (report.compatibility_slice_z != 65) return false;
    if (!sim_runtime_backend_get_fluid_view_2d(backend, &fluid)) return false;
    for (size_t i = 0; i < fluid.cell_count; ++i) {
        if (fluid.density[i] > 0.0001f) return false;
    }

    if (!sim_runtime_backend_step_compatibility_slice(backend, -1)) return false;
    if (!sim_runtime_backend_get_report(backend, &report)) return false;
    if (report.compatibility_slice_z != 64) return false;
    if (!sim_runtime_backend_get_fluid_view_2d(backend, &fluid)) return false;
    active_cells = 0;
    for (size_t i = 0; i < fluid.cell_count; ++i) {
        if (fluid.density[i] > 0.0001f) {
            active_cells += 1;
        }
    }
    if (active_cells == 0) return false;

    sim_runtime_backend_destroy(backend);
    return true;
}

int main(void) {
    if (!test_3d_backend_reports_xyz_domain_and_compatibility_slice()) {
        fprintf(stderr,
                "sim_runtime_backend_reporting_contract_test: 3D report/compatibility failed\n");
        return 1;
    }
    if (!test_3d_backend_seeds_atmospheric_preset_sparse_truth()) {
        fprintf(stderr,
                "sim_runtime_backend_reporting_contract_test: atmospheric seed failed\n");
        return 1;
    }
    if (!test_3d_fluid_mode_can_opt_into_atmospheric_initial_state()) {
        fprintf(stderr,
                "sim_runtime_backend_reporting_contract_test: atmospheric opt-in seed failed\n");
        return 1;
    }
    if (!test_3d_fluid_mode_without_opt_in_stays_blank_initially()) {
        fprintf(stderr,
                "sim_runtime_backend_reporting_contract_test: atmospheric opt-out seed failed\n");
        return 1;
    }
    if (!test_3d_backend_warm_start_source_overrides_procedural_report()) {
        fprintf(stderr,
                "sim_runtime_backend_reporting_contract_test: initial-state source report failed\n");
        return 1;
    }
    if (!test_3d_backend_reports_explicit_depth_contract()) {
        fprintf(stderr,
                "sim_runtime_backend_reporting_contract_test: explicit depth report failed\n");
        return 1;
    }
    if (!test_3d_backend_report_exposes_guard_override_contract()) {
        fprintf(stderr,
                "sim_runtime_backend_reporting_contract_test: guard override report failed\n");
        return 1;
    }
    if (!test_3d_backend_live_slice_selection_changes_report_and_view()) {
        fprintf(stderr,
                "sim_runtime_backend_reporting_contract_test: live slice selection failed\n");
        return 1;
    }
    if (!test_3d_backend_debug_volume_view_exposes_density_and_obstacle_truth()) {
        fprintf(stderr,
                "sim_runtime_backend_reporting_contract_test: debug volume view failed\n");
        return 1;
    }
    if (!test_3d_backend_volume_export_view_exposes_authoritative_xyz_fields()) {
        fprintf(stderr,
                "sim_runtime_backend_reporting_contract_test: volume export view failed\n");
        return 1;
    }
    if (!test_3d_backend_slice_activity_query_reports_fluid_and_obstacles()) {
        fprintf(stderr,
                "sim_runtime_backend_reporting_contract_test: slice activity query failed\n");
        return 1;
    }
    if (!test_3d_backend_step_uses_sparse_truth_even_when_dense_mirror_is_stale()) {
        fprintf(stderr,
                "sim_runtime_backend_reporting_contract_test: sparse truth cutover failed\n");
        return 1;
    }
    if (!test_3d_backend_sparse_report_tracks_clustered_solver_regions()) {
        fprintf(stderr,
                "sim_runtime_backend_reporting_contract_test: clustered sparse region metrics failed\n");
        return 1;
    }
    if (!test_3d_backend_sparse_report_guards_one_oversized_connected_cluster()) {
        fprintf(stderr,
                "sim_runtime_backend_reporting_contract_test: oversized sparse cluster guard failed\n");
        return 1;
    }
    if (!test_3d_backend_report_tracks_velocity_safety_guard_metrics()) {
        fprintf(stderr,
                "sim_runtime_backend_reporting_contract_test: velocity safety guard metrics failed\n");
        return 1;
    }
    if (!test_3d_backend_report_exposes_scene_up_velocity_truth()) {
        fprintf(stderr,
                "sim_runtime_backend_reporting_contract_test: scene-up velocity report failed\n");
        return 1;
    }
    fprintf(stdout, "sim_runtime_backend_reporting_contract_test: success\n");
    return 0;
}
