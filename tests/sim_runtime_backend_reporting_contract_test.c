#include "app/sim_runtime_backend.h"
#include "app/scene_state.h"
#include "app/sim_runtime_backend_3d_scaffold_internal.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>

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
    if (report.domain_w != 42) return false;
    if (report.domain_h != 56) return false;
    if (report.domain_d != 14) return false;
    if (report.cell_count != (size_t)42 * (size_t)56 * (size_t)14) return false;
    if (!report.volumetric_emitters_free_live) return false;
    if (!report.volumetric_emitters_attached_live) return false;
    if (!report.volumetric_obstacles_live) return false;
    if (!report.full_3d_solver_live) return false;
    if (!report.world_bounds_valid) return false;
    if (!nearly_equal(report.world_min_x, -1.0f)) return false;
    if (!nearly_equal(report.world_max_y, 2.0f)) return false;
    if (!nearly_equal(report.world_max_z, 0.5f)) return false;
    if (!nearly_equal(report.voxel_size, 4.0f / 56.0f)) return false;
    if (!report.scene_up_valid) return false;
    if (!nearly_equal(report.scene_up_x, 0.0f)) return false;
    if (!nearly_equal(report.scene_up_y, 0.0f)) return false;
    if (!nearly_equal(report.scene_up_z, 1.0f)) return false;
    if (report.scene_up_source != PHYSICS_SIM_RUNTIME_SCENE_UP_FALLBACK_POSITIVE_Z) return false;
    if (!report.compatibility_view_2d_available) return false;
    if (!report.compatibility_view_2d_derived) return false;
    if (report.compatibility_slice_z != 7) return false;
    if (!report.secondary_debug_slice_stack_live) return false;
    if (report.secondary_debug_slice_stack_radius != 2) return false;
    if (!report.debug_volume_view_3d_available) return false;
    if (report.debug_volume_solid_cells == 0) return false;
    if (report.debug_volume_active_density_cells != 0) return false;
    if (!nearly_equal(report.debug_volume_max_density, 0.0f)) return false;
    if (!nearly_equal(report.debug_volume_max_velocity_magnitude, 0.0f)) return false;
    if (!report.debug_volume_scene_up_velocity_valid) return false;
    if (!nearly_equal(report.debug_volume_scene_up_velocity_avg, 0.0f)) return false;
    if (!nearly_equal(report.debug_volume_scene_up_velocity_peak, 0.0f)) return false;
    if (!sim_runtime_backend_get_fluid_view_2d(backend, &fluid)) return false;
    if (fluid.width != report.domain_w) return false;
    if (fluid.height != report.domain_h) return false;
    if (fluid.cell_count != (size_t)report.domain_w * (size_t)report.domain_h) return false;

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
    if (volume.width != 16 || volume.height != 16 || volume.depth != 16) return false;
    if (volume.cell_count != (size_t)16 * (size_t)16 * (size_t)16) return false;
    if (!nearly_equal(volume.world_min_x, -1.0f)) return false;
    if (!nearly_equal(volume.world_max_z, 1.0f)) return false;
    if (!nearly_equal(volume.voxel_size, 0.125f)) return false;
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
    SimRuntimeBackend3DScaffold *impl = NULL;
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
    impl = (SimRuntimeBackend3DScaffold *)backend->impl;
    if (!impl) return false;

    sim_runtime_backend_build_obstacles(backend, NULL);
    idx = sim_runtime_3d_volume_index(&impl->volume.desc, 2, 3, 4);
    impl->volume.density[idx] = 1.25f;
    impl->volume.velocity_x[idx] = 2.0f;
    impl->volume.velocity_y[idx] = -3.0f;
    impl->volume.velocity_z[idx] = 4.5f;
    impl->volume.pressure[idx] = 6.0f;
    impl->obstacle_occupancy[idx] = 1u;

    if (!sim_runtime_backend_get_volume_export_view_3d(backend, &export_view)) return false;
    if (export_view.width != impl->volume.desc.grid_w) return false;
    if (export_view.height != impl->volume.desc.grid_h) return false;
    if (export_view.depth != impl->volume.desc.grid_d) return false;
    if (export_view.cell_count != impl->volume.desc.cell_count) return false;
    if (!nearly_equal(export_view.origin_x, impl->volume.desc.world_min_x)) return false;
    if (!nearly_equal(export_view.origin_y, impl->volume.desc.world_min_y)) return false;
    if (!nearly_equal(export_view.origin_z, impl->volume.desc.world_min_z)) return false;
    if (!nearly_equal(export_view.voxel_size, impl->volume.desc.voxel_size)) return false;
    if (!export_view.scene_up_valid) return false;
    if (!nearly_equal(export_view.scene_up_x, 0.0f)) return false;
    if (!nearly_equal(export_view.scene_up_y, 0.0f)) return false;
    if (!nearly_equal(export_view.scene_up_z, 1.0f)) return false;
    if (export_view.density != impl->volume.density) return false;
    if (export_view.velocity_x != impl->volume.velocity_x) return false;
    if (export_view.velocity_y != impl->volume.velocity_y) return false;
    if (export_view.velocity_z != impl->volume.velocity_z) return false;
    if (export_view.pressure != impl->volume.pressure) return false;
    if (export_view.solid_mask != impl->obstacle_occupancy) return false;
    if (export_view.density == impl->slice_density) return false;
    if (export_view.pressure == impl->slice_pressure) return false;
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
    if (report.compatibility_slice_z != 29) return false;
    if (!sim_runtime_backend_get_fluid_view_2d(backend, &fluid)) return false;
    for (size_t i = 0; i < fluid.cell_count; ++i) {
        if (fluid.density[i] > 0.0001f) return false;
    }

    if (!sim_runtime_backend_step_compatibility_slice(backend, -1)) return false;
    if (!sim_runtime_backend_get_report(backend, &report)) return false;
    if (report.compatibility_slice_z != 28) return false;
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
    if (!test_3d_backend_report_exposes_scene_up_velocity_truth()) {
        fprintf(stderr,
                "sim_runtime_backend_reporting_contract_test: scene-up velocity report failed\n");
        return 1;
    }
    fprintf(stdout, "sim_runtime_backend_reporting_contract_test: success\n");
    return 0;
}
