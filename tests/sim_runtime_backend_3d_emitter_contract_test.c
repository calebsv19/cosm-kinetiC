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

static void configure_tiny3d_domain(AppConfig *cfg,
                                    PhysicsSimRuntimeVisualBootstrap *visual,
                                    double span_x,
                                    double span_y,
                                    double span_z) {
    if (!cfg || !visual) return;
    cfg->quality_index = 5;
    cfg->grid_w = 64;
    cfg->grid_h = 64;
    visual->scene_domain.enabled = true;
    visual->scene_domain_authored = true;
    visual->scene_domain.min.x = 0.0;
    visual->scene_domain.min.y = 0.0;
    visual->scene_domain.min.z = 0.0;
    visual->scene_domain.max.x = span_x;
    visual->scene_domain.max.y = span_y;
    visual->scene_domain.max.z = span_z;
}

static float total_density_sum(const SimRuntimeBackend3DScaffoldTestView *impl) {
    float total = 0.0f;
    if (!impl) return 0.0f;
    for (size_t i = 0; i < impl->volume.desc.cell_count; ++i) {
        total += impl->volume.density[i];
    }
    return total;
}

static double density_center_x(const SimRuntimeBackend3DScaffoldTestView *impl) {
    double weighted = 0.0;
    double total = 0.0;
    if (!impl) return 0.0;
    for (int z = 0; z < impl->volume.desc.grid_d; ++z) {
        for (int y = 0; y < impl->volume.desc.grid_h; ++y) {
            for (int x = 0; x < impl->volume.desc.grid_w; ++x) {
                size_t idx = sim_runtime_3d_volume_index(&impl->volume.desc, x, y, z);
                double density = impl->volume.density[idx];
                if (density <= 0.0001) continue;
                weighted += cell_center_world(impl->volume.desc.world_min_x,
                                              impl->volume.desc.voxel_size,
                                              x) * density;
                total += density;
            }
        }
    }
    return (total > 0.0) ? (weighted / total) : 0.0;
}

static double density_center_z(const SimRuntimeBackend3DScaffoldTestView *impl) {
    double weighted = 0.0;
    double total = 0.0;
    if (!impl) return 0.0;
    for (int z = 0; z < impl->volume.desc.grid_d; ++z) {
        for (int y = 0; y < impl->volume.desc.grid_h; ++y) {
            for (int x = 0; x < impl->volume.desc.grid_w; ++x) {
                size_t idx = sim_runtime_3d_volume_index(&impl->volume.desc, x, y, z);
                double density = impl->volume.density[idx];
                if (density <= 0.0001) continue;
                weighted += cell_center_world(impl->volume.desc.world_min_z,
                                              impl->volume.desc.voxel_size,
                                              z) * density;
                total += density;
            }
        }
    }
    return (total > 0.0) ? (weighted / total) : 0.0;
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
    SimRuntimeBackendReport report = {0};
    SimRuntimeEmitterResolved emitter = {0};
    SimRuntimeEmitterPlacement3D placement = {0};
    size_t z_hits = 0;
    size_t written_cells = 0;

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

    for (size_t i = 0; i < impl->volume.desc.cell_count; ++i) {
        if (impl->volume.density[i] > 0.0f ||
            impl->volume.velocity_x[i] != 0.0f ||
            impl->volume.velocity_y[i] != 0.0f ||
            impl->volume.velocity_z[i] != 0.0f) {
            ++written_cells;
        }
    }
    if (!sim_runtime_backend_get_report(backend, &report)) return false;
    if (report.emitter_step_emitters_applied != 1) return false;
    if (report.emitter_step_free_emitters_applied != 1) return false;
    if (report.emitter_step_attached_emitters_applied != 0) return false;
    if (report.emitter_step_affected_cells == 0) return false;
    if (report.emitter_step_affected_cells != written_cells) return false;
    if (report.emitter_step_last_footprint_cells != written_cells) return false;
    if (report.emitter_step_density_delta <= 0.0f) return false;
    if (report.emitter_step_velocity_magnitude_delta <= 0.0f) return false;

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

static bool test_tiny3d_free_emitter_advects_density_downstream(void) {
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
    double center_before = 0.0;
    double center_after = 0.0;

    configure_tiny3d_domain(&cfg, &visual, 4.0, 2.0, 2.0);
    cfg.fluid_solver_iterations = 8;
    cfg.velocity_damping = 0.000006f;
    cfg.density_diffusion = 0.0f;
    cfg.density_decay = 0.0f;
    cfg.fluid_buoyancy_force = 0.0f;
    cfg.emitter_velocity_multiplier = 1.0f;

    preset.emitter_count = 1;
    preset.emitters[0] = (FluidEmitter){
        .type = EMITTER_VELOCITY_JET,
        .position_x = 0.30f,
        .position_y = 0.50f,
        .position_z = 0.50f,
        .radius = 0.06f,
        .strength = 10.0f,
        .dir_x = 1.0f,
        .dir_y = 0.0f,
        .dir_z = 0.0f,
        .attached_object = -1,
        .attached_import = -1,
    };

    backend = sim_runtime_backend_create(&cfg, &preset, &route, &visual);
    if (!backend) return false;
    impl = (SimRuntimeBackend3DScaffoldTestView *)backend->impl;
    if (!impl) return false;
    if (impl->volume.desc.grid_w != 16 || impl->volume.desc.grid_h != 8 || impl->volume.desc.grid_d != 8) {
        return false;
    }

    scene.backend = backend;
    scene.preset = &preset;
    scene.config = &cfg;
    scene.emitters_enabled = true;

    sim_runtime_backend_apply_emitters(backend, &scene, 0.1);
    center_before = density_center_x(impl);

    for (int i = 0; i < 3; ++i) {
        sim_runtime_backend_step(backend, &scene, &cfg, 0.25);
    }
    center_after = density_center_x(impl);

    sim_runtime_backend_destroy(backend);
    return center_after > center_before + 0.01;
}

static bool test_tiny3d_density_source_rises_along_scene_up_z(void) {
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
    double center_before = 0.0;
    double center_after = 0.0;

    configure_tiny3d_domain(&cfg, &visual, 4.0, 2.0, 2.0);
    cfg.fluid_solver_iterations = 8;
    cfg.velocity_damping = 0.000006f;
    cfg.density_diffusion = 0.0f;
    cfg.density_decay = 0.0f;
    cfg.fluid_buoyancy_force = 1.0f;
    cfg.emitter_density_multiplier = 1.0f;

    visual.scene_up.valid = true;
    visual.scene_up.direction = (CoreObjectVec3){0.0, 0.0, 1.0};
    visual.scene_up.source = PHYSICS_SIM_RUNTIME_SCENE_UP_FALLBACK_POSITIVE_Z;

    preset.dimension_mode = SCENE_DIMENSION_MODE_3D;
    preset.emitter_count = 1;
    preset.emitters[0] = (FluidEmitter){
        .type = EMITTER_DENSITY_SOURCE,
        .position_x = 0.50f,
        .position_y = 0.50f,
        .position_z = 0.35f,
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
    impl = (SimRuntimeBackend3DScaffoldTestView *)backend->impl;
    if (!impl) return false;

    scene.backend = backend;
    scene.preset = &preset;
    scene.config = &cfg;
    scene.emitters_enabled = true;
    scene.runtime_visual = visual;

    sim_runtime_backend_apply_emitters(backend, &scene, 0.1);
    center_before = density_center_z(impl);

    for (int i = 0; i < 4; ++i) {
        sim_runtime_backend_step(backend, &scene, &cfg, 0.25);
    }
    center_after = density_center_z(impl);

    sim_runtime_backend_destroy(backend);
    return center_after > center_before + 0.01;
}

static bool test_tiny3d_source_and_sink_reduce_net_density(void) {
    FluidScenePreset source_only = {0};
    FluidScenePreset source_and_sink = {0};
    SimModeRoute route = {
        .backend_lane = SIM_BACKEND_CONTROLLED_3D,
        .requested_space_mode = SPACE_MODE_3D,
        .projection_space_mode = SPACE_MODE_2D,
    };
    PhysicsSimRuntimeVisualBootstrap visual = {0};
    AppConfig cfg = {0};
    SceneState source_scene = {0};
    SceneState pair_scene = {0};
    SimRuntimeBackend *source_backend = NULL;
    SimRuntimeBackend *pair_backend = NULL;
    SimRuntimeBackend3DScaffoldTestView *source_impl = NULL;
    SimRuntimeBackend3DScaffoldTestView *pair_impl = NULL;
    SimRuntimeBackendReport pair_report = {0};
    float source_density = 0.0f;
    float pair_density = 0.0f;

    configure_tiny3d_domain(&cfg, &visual, 4.0, 2.0, 2.0);
    cfg.emitter_density_multiplier = 1.0f;
    cfg.emitter_sink_multiplier = 1.0f;

    source_only.emitter_count = 1;
    source_only.emitters[0] = (FluidEmitter){
        .type = EMITTER_DENSITY_SOURCE,
        .position_x = 0.35f,
        .position_y = 0.50f,
        .position_z = 0.50f,
        .radius = 0.06f,
        .strength = 8.0f,
        .dir_x = 0.0f,
        .dir_y = 0.0f,
        .dir_z = 0.0f,
        .attached_object = -1,
        .attached_import = -1,
    };

    source_and_sink = source_only;
    source_and_sink.emitter_count = 2;
    source_and_sink.emitters[1] = (FluidEmitter){
        .type = EMITTER_SINK,
        .position_x = 0.60f,
        .position_y = 0.50f,
        .position_z = 0.50f,
        .radius = 0.06f,
        .strength = 8.0f,
        .dir_x = 0.0f,
        .dir_y = 0.0f,
        .dir_z = 0.0f,
        .attached_object = -1,
        .attached_import = -1,
    };

    source_backend = sim_runtime_backend_create(&cfg, &source_only, &route, &visual);
    pair_backend = sim_runtime_backend_create(&cfg, &source_and_sink, &route, &visual);
    if (!source_backend || !pair_backend) return false;
    source_impl = (SimRuntimeBackend3DScaffoldTestView *)source_backend->impl;
    pair_impl = (SimRuntimeBackend3DScaffoldTestView *)pair_backend->impl;
    if (!source_impl || !pair_impl) return false;

    source_scene.backend = source_backend;
    source_scene.preset = &source_only;
    source_scene.config = &cfg;
    source_scene.emitters_enabled = true;

    pair_scene.backend = pair_backend;
    pair_scene.preset = &source_and_sink;
    pair_scene.config = &cfg;
    pair_scene.emitters_enabled = true;

    sim_runtime_backend_apply_emitters(source_backend, &source_scene, 0.1);
    sim_runtime_backend_apply_emitters(pair_backend, &pair_scene, 0.1);

    source_density = total_density_sum(source_impl);
    pair_density = total_density_sum(pair_impl);
    if (!sim_runtime_backend_get_report(pair_backend, &pair_report)) return false;

    sim_runtime_backend_destroy(source_backend);
    sim_runtime_backend_destroy(pair_backend);

    return source_density > 0.0f &&
           pair_density > 0.0f &&
           pair_density < source_density &&
           pair_report.emitter_step_emitters_applied == 2 &&
           pair_report.emitter_step_density_delta < source_density;
}

int main(void) {
    if (!test_free_emitter_writes_multiple_z_layers_and_updates_compat_slice()) {
        fprintf(stderr,
                "sim_runtime_backend_3d_emitter_contract_test: free emitter volumetric write failed\n");
        return 1;
    }
    if (!test_tiny3d_free_emitter_advects_density_downstream()) {
        fprintf(stderr,
                "sim_runtime_backend_3d_emitter_contract_test: tiny free-emitter transport failed\n");
        return 1;
    }
    if (!test_tiny3d_density_source_rises_along_scene_up_z()) {
        fprintf(stderr,
                "sim_runtime_backend_3d_emitter_contract_test: scene-up z rise failed\n");
        return 1;
    }
    if (!test_tiny3d_source_and_sink_reduce_net_density()) {
        fprintf(stderr,
                "sim_runtime_backend_3d_emitter_contract_test: tiny source/sink interaction failed\n");
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
