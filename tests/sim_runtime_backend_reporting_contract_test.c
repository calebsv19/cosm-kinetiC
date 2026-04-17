#include "app/sim_runtime_backend.h"

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
    if (!report.compatibility_view_2d_available) return false;
    if (!report.compatibility_view_2d_derived) return false;
    if (report.compatibility_slice_z != 7) return false;
    if (!report.secondary_debug_slice_stack_live) return false;
    if (report.secondary_debug_slice_stack_radius != 2) return false;
    if (!sim_runtime_backend_get_fluid_view_2d(backend, &fluid)) return false;
    if (fluid.width != report.domain_w) return false;
    if (fluid.height != report.domain_h) return false;
    if (fluid.cell_count != (size_t)report.domain_w * (size_t)report.domain_h) return false;

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
    if (!test_3d_backend_slice_activity_query_reports_fluid_and_obstacles()) {
        fprintf(stderr,
                "sim_runtime_backend_reporting_contract_test: slice activity query failed\n");
        return 1;
    }
    fprintf(stdout, "sim_runtime_backend_reporting_contract_test: success\n");
    return 0;
}
