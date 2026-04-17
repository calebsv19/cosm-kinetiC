#include "app/sim_runtime_backend.h"
#include "app/sim_runtime_backend_3d_scaffold_internal.h"

#include <stdbool.h>
#include <stdio.h>

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

static bool test_domain_walls_populate_occupancy_and_compatibility_slice(void) {
    AppConfig cfg = {0};
    SimModeRoute route = {
        .backend_lane = SIM_BACKEND_CONTROLLED_3D,
        .requested_space_mode = SPACE_MODE_3D,
        .projection_space_mode = SPACE_MODE_2D,
    };
    PhysicsSimRuntimeVisualBootstrap visual = {0};
    SimRuntimeBackend *backend = NULL;
    SimRuntimeBackend3DScaffold *impl = NULL;
    SceneObstacleFieldView2D obstacle = {0};
    int center_x = 0;
    int center_y = 0;
    size_t slice_center = 0;

    cfg.quality_index = 0;
    cfg.grid_w = 64;
    cfg.grid_h = 64;

    visual.scene_domain.enabled = true;
    visual.scene_domain_authored = true;
    visual.scene_domain.min.x = 0.0;
    visual.scene_domain.min.y = 0.0;
    visual.scene_domain.min.z = 0.0;
    visual.scene_domain.max.x = 1.0;
    visual.scene_domain.max.y = 1.0;
    visual.scene_domain.max.z = 0.5;

    backend = sim_runtime_backend_create(&cfg, NULL, &route, &visual);
    if (!backend) return false;
    impl = (SimRuntimeBackend3DScaffold *)backend->impl;
    if (!impl) return false;

    sim_runtime_backend_build_obstacles(backend, NULL);

    center_x = impl->volume.desc.grid_w / 2;
    center_y = impl->volume.desc.grid_h / 2;
    if (!impl->obstacle_occupancy[sim_runtime_3d_volume_index(&impl->volume.desc, center_x, center_y, 0)]) {
        return false;
    }
    if (!impl->obstacle_occupancy[sim_runtime_3d_volume_index(&impl->volume.desc,
                                                               center_x,
                                                               center_y,
                                                               impl->volume.desc.grid_d - 1)]) {
        return false;
    }

    if (!sim_runtime_backend_get_obstacle_view_2d(backend, &obstacle)) return false;
    if (obstacle.width != impl->volume.desc.grid_w) return false;
    if (obstacle.height != impl->volume.desc.grid_h) return false;
    slice_center = (size_t)center_y * (size_t)obstacle.width + (size_t)center_x;
    if (obstacle.solid_mask[0] == 0u) return false;
    if (obstacle.solid_mask[slice_center] != 0u) return false;
    if (obstacle.distance[slice_center] <= 0.0f) return false;

    sim_runtime_backend_destroy(backend);
    return true;
}

static bool test_boundary_enforcement_zeros_only_boundary_cells(void) {
    AppConfig cfg = {0};
    SimModeRoute route = {
        .backend_lane = SIM_BACKEND_CONTROLLED_3D,
        .requested_space_mode = SPACE_MODE_3D,
        .projection_space_mode = SPACE_MODE_2D,
    };
    PhysicsSimRuntimeVisualBootstrap visual = {0};
    SimRuntimeBackend *backend = NULL;
    SimRuntimeBackend3DScaffold *impl = NULL;
    int center_x = 0;
    int center_y = 0;
    int center_z = 0;
    size_t boundary_idx = 0;
    size_t interior_idx = 0;

    cfg.quality_index = 0;
    cfg.grid_w = 64;
    cfg.grid_h = 64;

    visual.scene_domain.enabled = true;
    visual.scene_domain_authored = true;
    visual.scene_domain.min.x = 0.0;
    visual.scene_domain.min.y = 0.0;
    visual.scene_domain.min.z = 0.0;
    visual.scene_domain.max.x = 1.0;
    visual.scene_domain.max.y = 1.0;
    visual.scene_domain.max.z = 1.0;

    backend = sim_runtime_backend_create(&cfg, NULL, &route, &visual);
    if (!backend) return false;
    impl = (SimRuntimeBackend3DScaffold *)backend->impl;
    if (!impl) return false;

    sim_runtime_backend_build_obstacles(backend, NULL);

    center_x = impl->volume.desc.grid_w / 2;
    center_y = impl->volume.desc.grid_h / 2;
    center_z = impl->volume.desc.grid_d / 2;
    boundary_idx = sim_runtime_3d_volume_index(&impl->volume.desc, center_x, center_y, 0);
    interior_idx = sim_runtime_3d_volume_index(&impl->volume.desc, center_x, center_y, center_z);

    impl->volume.density[boundary_idx] = 5.0f;
    impl->volume.velocity_x[boundary_idx] = 2.0f;
    impl->volume.velocity_y[boundary_idx] = 3.0f;
    impl->volume.velocity_z[boundary_idx] = 4.0f;
    impl->volume.pressure[boundary_idx] = 1.0f;

    impl->volume.density[interior_idx] = 7.0f;
    impl->volume.velocity_x[interior_idx] = 8.0f;
    impl->volume.velocity_y[interior_idx] = 9.0f;
    impl->volume.velocity_z[interior_idx] = 10.0f;
    impl->volume.pressure[interior_idx] = 11.0f;

    sim_runtime_backend_enforce_obstacles(backend, NULL);

    if (impl->volume.density[boundary_idx] != 0.0f) return false;
    if (impl->volume.velocity_x[boundary_idx] != 0.0f) return false;
    if (impl->volume.velocity_y[boundary_idx] != 0.0f) return false;
    if (impl->volume.velocity_z[boundary_idx] != 0.0f) return false;
    if (impl->volume.pressure[boundary_idx] != 0.0f) return false;

    if (impl->volume.density[interior_idx] != 7.0f) return false;
    if (impl->volume.velocity_x[interior_idx] != 8.0f) return false;
    if (impl->volume.velocity_y[interior_idx] != 9.0f) return false;
    if (impl->volume.velocity_z[interior_idx] != 10.0f) return false;
    if (impl->volume.pressure[interior_idx] != 11.0f) return false;

    sim_runtime_backend_destroy(backend);
    return true;
}

static bool test_live_solver_step_respects_boundary_occupancy(void) {
    AppConfig cfg = {0};
    SimModeRoute route = {
        .backend_lane = SIM_BACKEND_CONTROLLED_3D,
        .requested_space_mode = SPACE_MODE_3D,
        .projection_space_mode = SPACE_MODE_2D,
    };
    PhysicsSimRuntimeVisualBootstrap visual = {0};
    SimRuntimeBackend *backend = NULL;
    SimRuntimeBackend3DScaffold *impl = NULL;
    size_t near_wall_idx = 0;
    size_t wall_idx = 0;

    cfg.quality_index = 0;
    cfg.grid_w = 64;
    cfg.grid_h = 64;
    cfg.fluid_solver_iterations = 8;
    cfg.velocity_damping = 0.99f;
    cfg.density_diffusion = 0.02f;
    cfg.density_decay = 0.0f;
    cfg.fluid_buoyancy_force = 0.0f;

    visual.scene_domain.enabled = true;
    visual.scene_domain_authored = true;
    visual.scene_domain.min.x = 0.0;
    visual.scene_domain.min.y = 0.0;
    visual.scene_domain.min.z = 0.0;
    visual.scene_domain.max.x = 1.0;
    visual.scene_domain.max.y = 1.0;
    visual.scene_domain.max.z = 1.0;

    backend = sim_runtime_backend_create(&cfg, NULL, &route, &visual);
    if (!backend) return false;
    impl = (SimRuntimeBackend3DScaffold *)backend->impl;
    if (!impl) return false;

    sim_runtime_backend_build_obstacles(backend, NULL);

    near_wall_idx = sim_runtime_3d_volume_index(&impl->volume.desc, 1,
                                                impl->volume.desc.grid_h / 2,
                                                impl->volume.desc.grid_d / 2);
    wall_idx = sim_runtime_3d_volume_index(&impl->volume.desc, 0,
                                           impl->volume.desc.grid_h / 2,
                                           impl->volume.desc.grid_d / 2);

    impl->volume.density[near_wall_idx] = 10.0f;
    impl->volume.velocity_x[near_wall_idx] = -3.0f;

    sim_runtime_backend_step(backend, NULL, &cfg, 0.25);

    if (impl->obstacle_occupancy[wall_idx] == 0u) return false;
    if (impl->volume.density[wall_idx] != 0.0f) return false;
    if (impl->volume.velocity_x[wall_idx] != 0.0f) return false;
    if (impl->volume.density[near_wall_idx] <= 0.0f) return false;

    sim_runtime_backend_destroy(backend);
    return true;
}

int main(void) {
    if (!test_domain_walls_populate_occupancy_and_compatibility_slice()) {
        fprintf(stderr,
                "sim_runtime_backend_3d_obstacle_contract_test: occupancy/slice sync failed\n");
        return 1;
    }
    if (!test_boundary_enforcement_zeros_only_boundary_cells()) {
        fprintf(stderr,
                "sim_runtime_backend_3d_obstacle_contract_test: boundary enforcement failed\n");
        return 1;
    }
    if (!test_live_solver_step_respects_boundary_occupancy()) {
        fprintf(stderr,
                "sim_runtime_backend_3d_obstacle_contract_test: live solver boundary coupling failed\n");
        return 1;
    }
    fprintf(stdout, "sim_runtime_backend_3d_obstacle_contract_test: success\n");
    return 0;
}
