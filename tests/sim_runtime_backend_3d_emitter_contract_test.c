#include "app/scene_state.h"
#include "app/sim_runtime_backend.h"
#include "app/sim_runtime_emitter.h"

#include <stdbool.h>
#include <math.h>
#include <stdio.h>

typedef struct SimRuntimeBackend3DScaffoldTestView {
    SimRuntime3DVolume volume;
    int compatibility_slice_z;
    bool fluid_slice_dirty;
} SimRuntimeBackend3DScaffoldTestView;

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

static double cell_center_world(double world_min, double voxel_size, int index) {
    return world_min + ((double)index + 0.5) * voxel_size;
}

static bool collect_density_world_bounds(const SimRuntimeBackend3DScaffoldTestView *impl,
                                         double *out_min_x,
                                         double *out_max_x,
                                         double *out_min_y,
                                         double *out_max_y,
                                         double *out_min_z,
                                         double *out_max_z,
                                         size_t *out_active_cells) {
    bool have = false;
    size_t active_cells = 0;
    if (!impl || !out_min_x || !out_max_x || !out_min_y || !out_max_y || !out_min_z || !out_max_z ||
        !out_active_cells) {
        return false;
    }
    for (int z = 0; z < impl->volume.desc.grid_d; ++z) {
        for (int y = 0; y < impl->volume.desc.grid_h; ++y) {
            for (int x = 0; x < impl->volume.desc.grid_w; ++x) {
                size_t idx = sim_runtime_3d_volume_index(&impl->volume.desc, x, y, z);
                double world_x = 0.0;
                double world_y = 0.0;
                double world_z = 0.0;
                if (impl->volume.density[idx] <= 0.0f &&
                    impl->volume.velocity_x[idx] == 0.0f &&
                    impl->volume.velocity_y[idx] == 0.0f &&
                    impl->volume.velocity_z[idx] == 0.0f) {
                    continue;
                }
                world_x = cell_center_world(impl->volume.desc.world_min_x, impl->volume.desc.voxel_size, x);
                world_y = cell_center_world(impl->volume.desc.world_min_y, impl->volume.desc.voxel_size, y);
                world_z = cell_center_world(impl->volume.desc.world_min_z, impl->volume.desc.voxel_size, z);
                if (!have) {
                    *out_min_x = *out_max_x = world_x;
                    *out_min_y = *out_max_y = world_y;
                    *out_min_z = *out_max_z = world_z;
                    have = true;
                } else {
                    if (world_x < *out_min_x) *out_min_x = world_x;
                    if (world_x > *out_max_x) *out_max_x = world_x;
                    if (world_y < *out_min_y) *out_min_y = world_y;
                    if (world_y > *out_max_y) *out_max_y = world_y;
                    if (world_z < *out_min_z) *out_min_z = world_z;
                    if (world_z > *out_max_z) *out_max_z = world_z;
                }
                active_cells++;
            }
        }
    }
    *out_active_cells = active_cells;
    return have;
}

static bool test_free_emitter_writes_multiple_z_layers_and_updates_compat_slice(void) {
    AppConfig cfg = {0};
    FluidScenePreset preset = {0};
    SimModeRoute route = {
        .backend_lane = SIM_BACKEND_CONTROLLED_3D,
        .requested_space_mode = SPACE_MODE_3D,
        .projection_space_mode = SPACE_MODE_2D,
    };
    PhysicsSimRuntimeVisualBootstrap visual = {0};
    SimRuntimeBackend *backend = NULL;
    SimRuntimeBackend3DScaffoldTestView *impl = NULL;
    SceneState scene = {0};
    SceneFluidFieldView2D fluid = {0};
    SimRuntimeEmitterResolved emitter = {0};
    SimRuntimeEmitterPlacement3D placement = {0};
    size_t z_hits = 0;

    cfg.quality_index = 1;
    cfg.grid_w = 128;
    cfg.grid_h = 128;
    cfg.emitter_velocity_multiplier = 1.0f;

    visual.scene_domain.enabled = true;
    visual.scene_domain_authored = true;
    visual.scene_domain.min.x = 0.0;
    visual.scene_domain.min.y = 0.0;
    visual.scene_domain.min.z = 0.0;
    visual.scene_domain.max.x = 1.0;
    visual.scene_domain.max.y = 1.0;
    visual.scene_domain.max.z = 1.0;

    preset.emitter_count = 1;
    preset.emitters[0] = (FluidEmitter){
        .type = EMITTER_VELOCITY_JET,
        .position_x = 0.5f,
        .position_y = 0.5f,
        .position_z = 0.5f,
        .radius = 0.05f,
        .strength = 10.0f,
        .dir_x = 0.0f,
        .dir_y = 0.0f,
        .dir_z = 1.0f,
        .attached_object = -1,
        .attached_import = -1,
    };

    backend = sim_runtime_backend_create(&cfg, &preset, &route, &visual);
    if (!backend) return false;
    impl = (SimRuntimeBackend3DScaffoldTestView *)backend->impl;
    if (!impl) return false;

    scene.backend = backend;
    scene.preset = &preset;
    scene.config = &cfg;
    scene.emitters_enabled = true;

    sim_runtime_backend_apply_emitters(backend, &scene, 0.1);

    if (!sim_runtime_emitter_resolve(&preset, 0, &emitter)) return false;
    if (!sim_runtime_emitter_resolve_3d_placement(&impl->volume.desc, &emitter, &placement)) return false;

    for (int z = 0; z < impl->volume.desc.grid_d; ++z) {
        size_t idx = sim_runtime_3d_volume_index(&impl->volume.desc,
                                                 placement.center_x,
                                                 placement.center_y,
                                                 z);
        if (impl->volume.density[idx] > 0.0f || impl->volume.velocity_z[idx] > 0.0f) {
            ++z_hits;
        }
    }
    if (z_hits < 2) return false;

    {
        int outside_z = placement.max_z + 1;
        if (outside_z < impl->volume.desc.grid_d) {
            size_t idx = sim_runtime_3d_volume_index(&impl->volume.desc,
                                                     placement.center_x,
                                                     placement.center_y,
                                                     outside_z);
            if (impl->volume.density[idx] != 0.0f) return false;
            if (impl->volume.velocity_z[idx] != 0.0f) return false;
        }
    }

    if (!sim_runtime_backend_get_fluid_view_2d(backend, &fluid)) return false;
    {
        size_t slice_idx = (size_t)placement.center_y * (size_t)fluid.width + (size_t)placement.center_x;
        if (fluid.density[slice_idx] <= 0.0f) return false;
        if (fluid.velocity_x[slice_idx] != 0.0f) return false;
        if (fluid.velocity_y[slice_idx] != 0.0f) return false;
    }

    sim_runtime_backend_destroy(backend);
    return true;
}

static bool test_free_emitter_world_footprint_stays_stable_across_quality_levels(void) {
    FluidScenePreset preset = {0};
    SimModeRoute route = {
        .backend_lane = SIM_BACKEND_CONTROLLED_3D,
        .requested_space_mode = SPACE_MODE_3D,
        .projection_space_mode = SPACE_MODE_2D,
    };
    PhysicsSimRuntimeVisualBootstrap visual = {0};
    AppConfig coarse_cfg = {0};
    AppConfig dense_cfg = {0};
    SceneState coarse_scene = {0};
    SceneState dense_scene = {0};
    SimRuntimeBackend *coarse_backend = NULL;
    SimRuntimeBackend *dense_backend = NULL;
    SimRuntimeBackend3DScaffoldTestView *coarse_impl = NULL;
    SimRuntimeBackend3DScaffoldTestView *dense_impl = NULL;
    double coarse_min_x = 0.0;
    double coarse_max_x = 0.0;
    double coarse_min_y = 0.0;
    double coarse_max_y = 0.0;
    double coarse_min_z = 0.0;
    double coarse_max_z = 0.0;
    double dense_min_x = 0.0;
    double dense_max_x = 0.0;
    double dense_min_y = 0.0;
    double dense_max_y = 0.0;
    double dense_min_z = 0.0;
    double dense_max_z = 0.0;
    size_t coarse_cells = 0;
    size_t dense_cells = 0;
    double tolerance = 0.0;

    coarse_cfg.quality_index = 0;
    coarse_cfg.grid_w = 128;
    coarse_cfg.grid_h = 128;
    coarse_cfg.emitter_velocity_multiplier = 1.0f;
    dense_cfg = coarse_cfg;
    dense_cfg.quality_index = 3;

    visual.scene_domain.enabled = true;
    visual.scene_domain_authored = true;
    visual.scene_domain.min.x = -5.0;
    visual.scene_domain.min.y = -2.0;
    visual.scene_domain.min.z = -1.0;
    visual.scene_domain.max.x = 5.0;
    visual.scene_domain.max.y = 2.0;
    visual.scene_domain.max.z = 1.0;

    preset.emitter_count = 1;
    preset.emitters[0] = (FluidEmitter){
        .type = EMITTER_VELOCITY_JET,
        .position_x = 0.5f,
        .position_y = 0.5f,
        .position_z = 0.5f,
        .radius = 0.10f,
        .strength = 10.0f,
        .dir_x = 0.0f,
        .dir_y = 0.0f,
        .dir_z = 1.0f,
        .attached_object = -1,
        .attached_import = -1,
    };

    coarse_backend = sim_runtime_backend_create(&coarse_cfg, &preset, &route, &visual);
    dense_backend = sim_runtime_backend_create(&dense_cfg, &preset, &route, &visual);
    if (!coarse_backend || !dense_backend) goto fail;
    coarse_impl = (SimRuntimeBackend3DScaffoldTestView *)coarse_backend->impl;
    dense_impl = (SimRuntimeBackend3DScaffoldTestView *)dense_backend->impl;
    if (!coarse_impl || !dense_impl) goto fail;

    coarse_scene.backend = coarse_backend;
    coarse_scene.preset = &preset;
    coarse_scene.config = &coarse_cfg;
    coarse_scene.emitters_enabled = true;
    dense_scene.backend = dense_backend;
    dense_scene.preset = &preset;
    dense_scene.config = &dense_cfg;
    dense_scene.emitters_enabled = true;

    sim_runtime_backend_apply_emitters(coarse_backend, &coarse_scene, 0.1);
    sim_runtime_backend_apply_emitters(dense_backend, &dense_scene, 0.1);

    if (!collect_density_world_bounds(coarse_impl,
                                      &coarse_min_x,
                                      &coarse_max_x,
                                      &coarse_min_y,
                                      &coarse_max_y,
                                      &coarse_min_z,
                                      &coarse_max_z,
                                      &coarse_cells)) {
        goto fail;
    }
    if (!collect_density_world_bounds(dense_impl,
                                      &dense_min_x,
                                      &dense_max_x,
                                      &dense_min_y,
                                      &dense_max_y,
                                      &dense_min_z,
                                      &dense_max_z,
                                      &dense_cells)) {
        goto fail;
    }
    if (dense_cells <= coarse_cells) goto fail;

    tolerance = coarse_impl->volume.desc.voxel_size + dense_impl->volume.desc.voxel_size;
    if (fabs(coarse_min_x - dense_min_x) > tolerance) goto fail;
    if (fabs(coarse_max_x - dense_max_x) > tolerance) goto fail;
    if (fabs(coarse_min_y - dense_min_y) > tolerance) goto fail;
    if (fabs(coarse_max_y - dense_max_y) > tolerance) goto fail;
    if (fabs(coarse_min_z - dense_min_z) > tolerance) goto fail;
    if (fabs(coarse_max_z - dense_max_z) > tolerance) goto fail;

    sim_runtime_backend_destroy(coarse_backend);
    sim_runtime_backend_destroy(dense_backend);
    return true;

fail:
    if (coarse_backend) sim_runtime_backend_destroy(coarse_backend);
    if (dense_backend) sim_runtime_backend_destroy(dense_backend);
    return false;
}

int main(void) {
    if (!test_free_emitter_writes_multiple_z_layers_and_updates_compat_slice()) {
        fprintf(stderr,
                "sim_runtime_backend_3d_emitter_contract_test: free emitter volumetric write failed\n");
        return 1;
    }
    if (!test_free_emitter_world_footprint_stays_stable_across_quality_levels()) {
        fprintf(stderr,
                "sim_runtime_backend_3d_emitter_contract_test: world footprint stability failed\n");
        return 1;
    }
    fprintf(stdout, "sim_runtime_backend_3d_emitter_contract_test: success\n");
    return 0;
}
