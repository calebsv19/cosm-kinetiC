#include "app/scene_state.h"
#include "app/sim_runtime_backend.h"
#include "app/sim_runtime_3d_domain.h"
#include "sim_runtime_backend_3d_test_support.h"

#include <stdbool.h>
#include <math.h>
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

static double density_center_y(const SimRuntimeBackend3DScaffoldTestView *impl) {
    double weighted = 0.0;
    double total = 0.0;
    if (!impl) return 0.0;
    for (int z = 0; z < impl->volume.desc.grid_d; ++z) {
        for (int y = 0; y < impl->volume.desc.grid_h; ++y) {
            for (int x = 0; x < impl->volume.desc.grid_w; ++x) {
                size_t idx = sim_runtime_3d_volume_index(&impl->volume.desc, x, y, z);
                double density = impl->volume.density[idx];
                if (density <= 0.0001) continue;
                weighted += cell_center_world(impl->volume.desc.world_min_y,
                                              impl->volume.desc.voxel_size,
                                              y) * density;
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

static bool collect_center_column_density_band(const SimRuntimeBackend3DScaffoldTestView *impl,
                                               int *out_min_z,
                                               int *out_max_z,
                                               size_t *out_hits) {
    bool have = false;
    int min_z = 0;
    int max_z = 0;
    size_t hits = 0;
    int center_x = 0;
    int center_y = 0;
    if (!impl || !out_min_z || !out_max_z || !out_hits) return false;
    center_x = impl->volume.desc.grid_w / 2;
    center_y = impl->volume.desc.grid_h / 2;
    for (int z = 0; z < impl->volume.desc.grid_d; ++z) {
        size_t idx = sim_runtime_3d_volume_index(&impl->volume.desc, center_x, center_y, z);
        if (impl->volume.density[idx] <= 0.0f) continue;
        if (!have) {
            min_z = max_z = z;
            have = true;
        } else {
            max_z = z;
        }
        ++hits;
    }
    if (!have) return false;
    *out_min_z = min_z;
    *out_max_z = max_z;
    *out_hits = hits;
    return true;
}

static bool all_density_zero(const SimRuntimeBackend3DScaffoldTestView *impl) {
    if (!impl) return false;
    for (size_t i = 0; i < impl->volume.desc.cell_count; ++i) {
        if (fabsf(impl->volume.density[i]) > 1e-6f) return false;
    }
    return true;
}

static bool any_velocity_non_zero(const SimRuntimeBackend3DScaffoldTestView *impl) {
    if (!impl) return false;
    for (size_t i = 0; i < impl->volume.desc.cell_count; ++i) {
        if (fabsf(impl->volume.velocity_x[i]) > 1e-6f ||
            fabsf(impl->volume.velocity_y[i]) > 1e-6f ||
            fabsf(impl->volume.velocity_z[i]) > 1e-6f) {
            return true;
        }
    }
    return false;
}

static float max_abs_density_value(const SimRuntimeBackend3DScaffoldTestView *impl) {
    float max_abs = 0.0f;
    if (!impl) return 0.0f;
    for (size_t i = 0; i < impl->volume.desc.cell_count; ++i) {
        float value = fabsf(impl->volume.density[i]);
        if (value > max_abs) max_abs = value;
    }
    return max_abs;
}

static bool test_attached_object_box_emitter_writes_xyz_occupancy(void) {
    AppConfig cfg = {0};
    FluidScenePreset preset = {0};
    SimModeRoute route = {
        .backend_lane = SIM_BACKEND_CONTROLLED_3D,
        .requested_space_mode = SPACE_MODE_3D,
        .projection_space_mode = SPACE_MODE_2D,
    };
    PhysicsSimRuntimeVisualBootstrap visual = {0};
    SimRuntimeBackend *backend = NULL;
    SimRuntimeBackend3DScaffoldTestView impl = {0};
    SceneState scene = {0};
    size_t z_hits = 0;
    size_t center_idx = 0;
    size_t outside_idx = 0;

    cfg.quality_index = 1;
    cfg.grid_w = 128;
    cfg.grid_h = 128;
    cfg.emitter_density_multiplier = 1.0f;

    visual.scene_domain.enabled = true;
    visual.scene_domain_authored = true;
    visual.scene_domain.min.x = 0.0;
    visual.scene_domain.min.y = 0.0;
    visual.scene_domain.min.z = 0.0;
    visual.scene_domain.max.x = 1.0;
    visual.scene_domain.max.y = 1.0;
    visual.scene_domain.max.z = 1.0;

    preset.object_count = 1;
    preset.objects[0] = (PresetObject){
        .type = PRESET_OBJECT_BOX,
        .position_x = 0.5f,
        .position_y = 0.5f,
        .position_z = 0.5f,
        .size_x = 0.12f,
        .size_y = 0.07f,
        .size_z = 0.10f,
        .angle = 0.0f,
    };
    preset.emitter_count = 1;
    preset.emitters[0] = (FluidEmitter){
        .type = EMITTER_DENSITY_SOURCE,
        .position_x = 0.1f,
        .position_y = 0.1f,
        .position_z = 0.1f,
        .radius = 0.03f,
        .strength = 8.0f,
        .dir_x = 0.0f,
        .dir_y = 0.0f,
        .dir_z = 1.0f,
        .attached_object = 0,
        .attached_import = -1,
    };

    backend = sim_runtime_backend_create(&cfg, &preset, &route, &visual);
    if (!backend) return false;

    scene.backend = backend;
    scene.preset = &preset;
    scene.config = &cfg;
    scene.emitters_enabled = true;

    sim_runtime_backend_apply_emitters(backend, &scene, 0.1);
    if (!sim_runtime_backend_3d_test_view_refresh(backend, &impl)) return false;

    for (int z = 0; z < impl.volume.desc.grid_d; ++z) {
        size_t idx = sim_runtime_3d_volume_index(&impl.volume.desc,
                                                 impl.volume.desc.grid_w / 2,
                                                 impl.volume.desc.grid_h / 2,
                                                 z);
        if (impl.volume.density[idx] > 0.0f) ++z_hits;
    }
    if (z_hits < 2) return false;

    center_idx = sim_runtime_3d_volume_index(&impl.volume.desc,
                                             impl.volume.desc.grid_w / 2,
                                             impl.volume.desc.grid_h / 2,
                                             impl.volume.desc.grid_d / 2);
    if (impl.volume.density[center_idx] <= 0.0f) return false;

    outside_idx = sim_runtime_3d_volume_index(&impl.volume.desc,
                                              impl.volume.desc.grid_w / 2,
                                              impl.volume.desc.grid_h / 2,
                                              impl.volume.desc.grid_d / 2 + 24);
    if (outside_idx < impl.volume.desc.cell_count && impl.volume.density[outside_idx] > 0.0f) return false;

    sim_runtime_backend_destroy(backend);
    return true;
}

static bool test_attached_import_emitter_rotates_direction_and_updates_compat_slice(void) {
    AppConfig cfg = {0};
    FluidScenePreset preset = {0};
    SimModeRoute route = {
        .backend_lane = SIM_BACKEND_CONTROLLED_3D,
        .requested_space_mode = SPACE_MODE_3D,
        .projection_space_mode = SPACE_MODE_2D,
    };
    PhysicsSimRuntimeVisualBootstrap visual = {0};
    SimRuntimeBackend *backend = NULL;
    SimRuntimeBackend3DScaffoldTestView impl = {0};
    SceneState scene = {0};
    SceneFluidFieldView2D fluid = {0};
    SimRuntimeBackendReport report = {0};
    size_t center_idx = 0;
    size_t slice_idx = 0;
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

    preset.import_shape_count = 1;
    preset.import_shapes[0] = (ImportedShape){
        .enabled = true,
        .position_x = 0.5f,
        .position_y = 0.5f,
        .position_z = 0.5f,
        .scale = 1.0f,
        .rotation_deg = 90.0f,
    };
    preset.emitter_count = 1;
    preset.emitters[0] = (FluidEmitter){
        .type = EMITTER_VELOCITY_JET,
        .position_x = 0.15f,
        .position_y = 0.15f,
        .position_z = 0.15f,
        .radius = 0.04f,
        .strength = 10.0f,
        .dir_x = 1.0f,
        .dir_y = 0.0f,
        .dir_z = 0.0f,
        .attached_object = -1,
        .attached_import = 0,
    };

    backend = sim_runtime_backend_create(&cfg, &preset, &route, &visual);
    if (!backend) return false;

    scene.backend = backend;
    scene.preset = &preset;
    scene.config = &cfg;
    scene.emitters_enabled = true;
    scene.import_shape_count = 1;
    scene.import_shapes[0] = preset.import_shapes[0];

    sim_runtime_backend_apply_emitters(backend, &scene, 0.1);
    if (!sim_runtime_backend_3d_test_view_refresh(backend, &impl)) return false;

    center_idx = sim_runtime_3d_volume_index(&impl.volume.desc,
                                             impl.volume.desc.grid_w / 2,
                                             impl.volume.desc.grid_h / 2,
                                             impl.volume.desc.grid_d / 2);
    if (impl.volume.velocity_y[center_idx] <= 0.0f) return false;
    if (fabsf(impl.volume.velocity_x[center_idx]) > 0.0001f) return false;

    if (!sim_runtime_backend_get_fluid_view_2d(backend, &fluid)) return false;
    slice_idx = (size_t)(fluid.height / 2) * (size_t)fluid.width + (size_t)(fluid.width / 2);
    if (fluid.density[slice_idx] <= 0.0f) return false;
    if (fluid.velocity_y[slice_idx] <= 0.0f) return false;

    for (size_t i = 0; i < impl.volume.desc.cell_count; ++i) {
        if (impl.volume.density[i] > 0.0f ||
            impl.volume.velocity_x[i] != 0.0f ||
            impl.volume.velocity_y[i] != 0.0f ||
            impl.volume.velocity_z[i] != 0.0f) {
            ++written_cells;
        }
    }
    if (!sim_runtime_backend_get_report(backend, &report)) return false;
    if (report.emitter_step_emitters_applied != 1) return false;
    if (report.emitter_step_free_emitters_applied != 0) return false;
    if (report.emitter_step_attached_emitters_applied != 1) return false;
    if (report.emitter_step_affected_cells == 0) return false;
    if (report.emitter_step_affected_cells != written_cells) return false;
    if (report.emitter_step_last_footprint_cells != written_cells) return false;
    if (report.emitter_step_density_delta <= 0.0f) return false;
    if (report.emitter_step_velocity_magnitude_delta <= 0.0f) return false;

    sim_runtime_backend_destroy(backend);
    return true;
}

static bool test_surface_patch_object_emitter_targets_top_face_band(void) {
    AppConfig cfg = {0};
    FluidScenePreset preset = {0};
    SimModeRoute route = {
        .backend_lane = SIM_BACKEND_CONTROLLED_3D,
        .requested_space_mode = SPACE_MODE_3D,
        .projection_space_mode = SPACE_MODE_2D,
    };
    PhysicsSimRuntimeVisualBootstrap visual = {0};
    SimRuntimeBackend *backend = NULL;
    SimRuntimeBackend3DScaffoldTestView impl = {0};
    SceneState scene = {0};
    int min_hit_z = 0;
    int max_hit_z = 0;
    size_t z_hits = 0;
    size_t center_idx = 0;
    size_t top_idx = 0;

    cfg.quality_index = 1;
    cfg.grid_w = 128;
    cfg.grid_h = 128;
    cfg.emitter_density_multiplier = 1.0f;

    visual.scene_domain.enabled = true;
    visual.scene_domain_authored = true;
    visual.scene_domain.min.x = 0.0;
    visual.scene_domain.min.y = 0.0;
    visual.scene_domain.min.z = 0.0;
    visual.scene_domain.max.x = 1.0;
    visual.scene_domain.max.y = 1.0;
    visual.scene_domain.max.z = 1.0;

    preset.object_count = 1;
    preset.objects[0] = (PresetObject){
        .type = PRESET_OBJECT_BOX,
        .position_x = 0.5f,
        .position_y = 0.5f,
        .position_z = 0.5f,
        .size_x = 0.12f,
        .size_y = 0.07f,
        .size_z = 0.10f,
        .angle = 0.0f,
    };
    preset.emitter_count = 1;
    preset.emitters[0] = (FluidEmitter){
        .type = EMITTER_DENSITY_SOURCE,
        .position_x = 0.1f,
        .position_y = 0.1f,
        .position_z = 0.1f,
        .radius = 0.03f,
        .strength = 8.0f,
        .dir_x = 0.0f,
        .dir_y = 0.0f,
        .dir_z = 1.0f,
        .attached_object = 0,
        .attached_import = -1,
        .source_mode_3d = EMITTER_3D_SOURCE_MODE_SURFACE_PATCH,
        .surface_3d = EMITTER_3D_SURFACE_TOP,
        .obstacle_mode_3d = EMITTER_3D_OBSTACLE_MODE_RETAIN_ATTACHED,
    };

    backend = sim_runtime_backend_create(&cfg, &preset, &route, &visual);
    if (!backend) return false;

    scene.backend = backend;
    scene.preset = &preset;
    scene.config = &cfg;
    scene.emitters_enabled = true;

    sim_runtime_backend_apply_emitters(backend, &scene, 0.1);
    if (!sim_runtime_backend_3d_test_view_refresh(backend, &impl)) return false;
    if (!collect_center_column_density_band(&impl, &min_hit_z, &max_hit_z, &z_hits)) return false;

    center_idx = sim_runtime_3d_volume_index(&impl.volume.desc,
                                             impl.volume.desc.grid_w / 2,
                                             impl.volume.desc.grid_h / 2,
                                             impl.volume.desc.grid_d / 2);
    top_idx = sim_runtime_3d_volume_index(&impl.volume.desc,
                                          impl.volume.desc.grid_w / 2,
                                          impl.volume.desc.grid_h / 2,
                                          max_hit_z);
    if (z_hits == 0 || z_hits > 4) return false;
    if (min_hit_z <= (impl.volume.desc.grid_d / 2)) return false;
    if (impl.volume.density[center_idx] > 0.0f) return false;
    if (impl.volume.density[top_idx] <= 0.0f) return false;

    sim_runtime_backend_destroy(backend);
    return true;
}

static bool test_surface_patch_density_launches_harder_than_legacy_volume_fill(void) {
    FluidScenePreset legacy_preset = {0};
    FluidScenePreset surface_preset = {0};
    SimModeRoute route = {
        .backend_lane = SIM_BACKEND_CONTROLLED_3D,
        .requested_space_mode = SPACE_MODE_3D,
        .projection_space_mode = SPACE_MODE_2D,
    };
    PhysicsSimRuntimeVisualBootstrap visual = {0};
    AppConfig cfg = {0};
    SceneState legacy_scene = {0};
    SceneState surface_scene = {0};
    SimRuntimeBackend *legacy_backend = NULL;
    SimRuntimeBackend *surface_backend = NULL;
    SimRuntimeBackend3DScaffoldTestView legacy_impl = {0};
    SimRuntimeBackend3DScaffoldTestView surface_impl = {0};
    int min_hit_z = 0;
    int max_hit_z = 0;
    size_t z_hits = 0;
    size_t compare_idx = 0;

    cfg.quality_index = 1;
    cfg.grid_w = 128;
    cfg.grid_h = 128;
    cfg.emitter_density_multiplier = 1.0f;

    visual.scene_domain.enabled = true;
    visual.scene_domain_authored = true;
    visual.scene_domain.min.x = 0.0;
    visual.scene_domain.min.y = 0.0;
    visual.scene_domain.min.z = 0.0;
    visual.scene_domain.max.x = 1.0;
    visual.scene_domain.max.y = 1.0;
    visual.scene_domain.max.z = 1.0;

    legacy_preset.object_count = 1;
    legacy_preset.objects[0] = (PresetObject){
        .type = PRESET_OBJECT_BOX,
        .position_x = 0.5f,
        .position_y = 0.5f,
        .position_z = 0.5f,
        .size_x = 0.12f,
        .size_y = 0.07f,
        .size_z = 0.10f,
        .angle = 0.0f,
    };
    legacy_preset.emitter_count = 1;
    legacy_preset.emitters[0] = (FluidEmitter){
        .type = EMITTER_DENSITY_SOURCE,
        .position_x = 0.1f,
        .position_y = 0.1f,
        .position_z = 0.1f,
        .radius = 0.03f,
        .strength = 8.0f,
        .dir_x = 0.0f,
        .dir_y = 0.0f,
        .dir_z = 1.0f,
        .attached_object = 0,
        .attached_import = -1,
        .source_mode_3d = EMITTER_3D_SOURCE_MODE_LEGACY_COMPAT,
    };

    surface_preset = legacy_preset;
    surface_preset.emitters[0].source_mode_3d = EMITTER_3D_SOURCE_MODE_SURFACE_PATCH;
    surface_preset.emitters[0].surface_3d = EMITTER_3D_SURFACE_TOP;
    surface_preset.emitters[0].obstacle_mode_3d = EMITTER_3D_OBSTACLE_MODE_RETAIN_ATTACHED;

    legacy_backend = sim_runtime_backend_create(&cfg, &legacy_preset, &route, &visual);
    surface_backend = sim_runtime_backend_create(&cfg, &surface_preset, &route, &visual);
    if (!legacy_backend || !surface_backend) goto fail;

    legacy_scene.backend = legacy_backend;
    legacy_scene.preset = &legacy_preset;
    legacy_scene.config = &cfg;
    legacy_scene.emitters_enabled = true;

    surface_scene.backend = surface_backend;
    surface_scene.preset = &surface_preset;
    surface_scene.config = &cfg;
    surface_scene.emitters_enabled = true;

    sim_runtime_backend_apply_emitters(legacy_backend, &legacy_scene, 0.1);
    sim_runtime_backend_apply_emitters(surface_backend, &surface_scene, 0.1);
    if (!sim_runtime_backend_3d_test_view_refresh(legacy_backend, &legacy_impl) ||
        !sim_runtime_backend_3d_test_view_refresh(surface_backend, &surface_impl)) {
        goto fail;
    }
    if (!collect_center_column_density_band(&surface_impl, &min_hit_z, &max_hit_z, &z_hits)) goto fail;
    compare_idx = sim_runtime_3d_volume_index(&surface_impl.volume.desc,
                                              surface_impl.volume.desc.grid_w / 2,
                                              surface_impl.volume.desc.grid_h / 2,
                                              max_hit_z);

    sim_runtime_backend_destroy(legacy_backend);
    sim_runtime_backend_destroy(surface_backend);
    return surface_impl.volume.velocity_z[compare_idx] >
           legacy_impl.volume.velocity_z[compare_idx];

fail:
    if (legacy_backend) sim_runtime_backend_destroy(legacy_backend);
    if (surface_backend) sim_runtime_backend_destroy(surface_backend);
    return false;
}

static bool test_surface_shell_all_faces_avoids_center_fill(void) {
    AppConfig cfg = {0};
    FluidScenePreset preset = {0};
    SimModeRoute route = {
        .backend_lane = SIM_BACKEND_CONTROLLED_3D,
        .requested_space_mode = SPACE_MODE_3D,
        .projection_space_mode = SPACE_MODE_2D,
    };
    PhysicsSimRuntimeVisualBootstrap visual = {0};
    SimRuntimeBackend *backend = NULL;
    SimRuntimeBackend3DScaffoldTestView impl = {0};
    SceneState scene = {0};
    double min_x = 0.0;
    double max_x = 0.0;
    double min_y = 0.0;
    double max_y = 0.0;
    double min_z = 0.0;
    double max_z = 0.0;
    size_t active_cells = 0;
    size_t center_idx = 0;

    cfg.quality_index = 1;
    cfg.grid_w = 128;
    cfg.grid_h = 128;
    cfg.emitter_density_multiplier = 1.0f;

    visual.scene_domain.enabled = true;
    visual.scene_domain_authored = true;
    visual.scene_domain.min.x = 0.0;
    visual.scene_domain.min.y = 0.0;
    visual.scene_domain.min.z = 0.0;
    visual.scene_domain.max.x = 1.0;
    visual.scene_domain.max.y = 1.0;
    visual.scene_domain.max.z = 1.0;

    preset.object_count = 1;
    preset.objects[0] = (PresetObject){
        .type = PRESET_OBJECT_BOX,
        .position_x = 0.5f,
        .position_y = 0.5f,
        .position_z = 0.5f,
        .size_x = 0.12f,
        .size_y = 0.12f,
        .size_z = 0.12f,
        .angle = 0.0f,
    };
    preset.emitter_count = 1;
    preset.emitters[0] = (FluidEmitter){
        .type = EMITTER_DENSITY_SOURCE,
        .position_x = 0.1f,
        .position_y = 0.1f,
        .position_z = 0.1f,
        .radius = 0.03f,
        .strength = 8.0f,
        .dir_x = 0.0f,
        .dir_y = 0.0f,
        .dir_z = 1.0f,
        .attached_object = 0,
        .attached_import = -1,
        .source_mode_3d = EMITTER_3D_SOURCE_MODE_SURFACE_SHELL,
        .surface_3d = EMITTER_3D_SURFACE_ALL_FACES,
        .obstacle_mode_3d = EMITTER_3D_OBSTACLE_MODE_RETAIN_ATTACHED,
    };

    backend = sim_runtime_backend_create(&cfg, &preset, &route, &visual);
    if (!backend) return false;

    scene.backend = backend;
    scene.preset = &preset;
    scene.config = &cfg;
    scene.emitters_enabled = true;

    sim_runtime_backend_apply_emitters(backend, &scene, 0.1);
    if (!sim_runtime_backend_3d_test_view_refresh(backend, &impl)) return false;
    if (!collect_density_world_bounds(&impl,
                                      &min_x,
                                      &max_x,
                                      &min_y,
                                      &max_y,
                                      &min_z,
                                      &max_z,
                                      &active_cells)) {
        return false;
    }

    center_idx = sim_runtime_3d_volume_index(&impl.volume.desc,
                                             impl.volume.desc.grid_w / 2,
                                             impl.volume.desc.grid_h / 2,
                                             impl.volume.desc.grid_d / 2);

    sim_runtime_backend_destroy(backend);
    return impl.volume.density[center_idx] <= 0.0f &&
           active_cells > 0 &&
           min_x < 0.5 && max_x > 0.5 &&
           min_z < 0.5 && max_z > 0.5;
}

static bool test_heated_obstacle_uses_top_biased_shell_release(void) {
    AppConfig cfg = {0};
    FluidScenePreset preset = {0};
    SimModeRoute route = {
        .backend_lane = SIM_BACKEND_CONTROLLED_3D,
        .requested_space_mode = SPACE_MODE_3D,
        .projection_space_mode = SPACE_MODE_2D,
    };
    PhysicsSimRuntimeVisualBootstrap visual = {0};
    SimRuntimeBackend *backend = NULL;
    SimRuntimeBackend3DScaffoldTestView impl = {0};
    SimRuntimeBackendReport report = {0};
    SceneState scene = {0};
    size_t center_idx = 0;
    double centroid_z = 0.0;

    cfg.quality_index = 1;
    cfg.grid_w = 128;
    cfg.grid_h = 128;
    cfg.emitter_density_multiplier = 1.0f;

    visual.scene_domain.enabled = true;
    visual.scene_domain_authored = true;
    visual.scene_domain.min.x = 0.0;
    visual.scene_domain.min.y = 0.0;
    visual.scene_domain.min.z = 0.0;
    visual.scene_domain.max.x = 1.0;
    visual.scene_domain.max.y = 1.0;
    visual.scene_domain.max.z = 1.0;
    visual.scene_up.valid = true;
    visual.scene_up.direction = (CoreObjectVec3){0.0, 0.0, 1.0};
    visual.scene_up.source = PHYSICS_SIM_RUNTIME_SCENE_UP_FALLBACK_POSITIVE_Z;

    preset.dimension_mode = SCENE_DIMENSION_MODE_3D;
    preset.object_count = 1;
    preset.objects[0] = (PresetObject){
        .type = PRESET_OBJECT_BOX,
        .position_x = 0.5f,
        .position_y = 0.5f,
        .position_z = 0.5f,
        .size_x = 0.12f,
        .size_y = 0.12f,
        .size_z = 0.12f,
        .angle = 0.0f,
        .is_static = true,
    };
    preset.emitter_count = 1;
    preset.emitters[0] = (FluidEmitter){
        .type = EMITTER_DENSITY_SOURCE,
        .position_x = 0.1f,
        .position_y = 0.1f,
        .position_z = 0.1f,
        .radius = 0.03f,
        .strength = 8.0f,
        .attached_object = 0,
        .attached_import = -1,
        .source_mode_3d = EMITTER_3D_SOURCE_MODE_HEATED_OBSTACLE,
        .surface_3d = EMITTER_3D_SURFACE_ALL_FACES,
        .obstacle_mode_3d = EMITTER_3D_OBSTACLE_MODE_RETAIN_ATTACHED,
        .thermal_buoyancy_3d = 6.0f,
    };

    backend = sim_runtime_backend_create(&cfg, &preset, &route, &visual);
    if (!backend) return false;

    scene.backend = backend;
    scene.preset = &preset;
    scene.config = &cfg;
    scene.emitters_enabled = true;
    scene.runtime_visual = visual;

    sim_runtime_backend_apply_emitters(backend, &scene, 0.1);
    if (!sim_runtime_backend_3d_test_view_refresh(backend, &impl)) return false;
    if (!sim_runtime_backend_get_report(backend, &report)) return false;
    centroid_z = density_center_z(&impl);

    center_idx = sim_runtime_3d_volume_index(&impl.volume.desc,
                                             impl.volume.desc.grid_w / 2,
                                             impl.volume.desc.grid_h / 2,
                                             impl.volume.desc.grid_d / 2);

    sim_runtime_backend_destroy(backend);
    return !all_density_zero(&impl) &&
           any_velocity_non_zero(&impl) &&
           fabsf(impl.volume.velocity_z[center_idx]) <= 1e-6f &&
           report.emitter_step_density_delta > 0.0f &&
           report.emitter_step_velocity_magnitude_delta > report.emitter_step_density_delta &&
           centroid_z > 0.5;
}

static bool test_heated_obstacle_external_shell_persists_across_runtime_steps(void) {
    AppConfig cfg = {0};
    FluidScenePreset preset = {0};
    SimModeRoute route = {
        .backend_lane = SIM_BACKEND_CONTROLLED_3D,
        .requested_space_mode = SPACE_MODE_3D,
        .projection_space_mode = SPACE_MODE_2D,
    };
    PhysicsSimRuntimeVisualBootstrap visual = {0};
    SimRuntimeBackend *backend = NULL;
    SimRuntimeBackend3DScaffoldTestView impl = {0};
    SceneState scene = {0};
    double centroid_z = 0.0;

    configure_tiny3d_domain(&cfg, &visual, 4.0, 2.0, 2.0);
    cfg.fluid_solver_iterations = 8;
    cfg.velocity_damping = 0.000006f;
    cfg.density_diffusion = 0.0f;
    cfg.density_decay = 0.0f;
    cfg.fluid_buoyancy_force = 0.0f;
    cfg.emitter_density_multiplier = 1.0f;

    visual.scene_up.valid = true;
    visual.scene_up.direction = (CoreObjectVec3){0.0, 0.0, 1.0};
    visual.scene_up.source = PHYSICS_SIM_RUNTIME_SCENE_UP_FALLBACK_POSITIVE_Z;

    preset.dimension_mode = SCENE_DIMENSION_MODE_3D;
    preset.object_count = 1;
    preset.objects[0] = (PresetObject){
        .type = PRESET_OBJECT_BOX,
        .position_x = 0.5f,
        .position_y = 0.5f,
        .position_z = 0.5f,
        .size_x = 0.12f,
        .size_y = 0.12f,
        .size_z = 0.12f,
        .angle = 0.0f,
        .is_static = true,
    };
    preset.emitter_count = 1;
    preset.emitters[0] = (FluidEmitter){
        .type = EMITTER_DENSITY_SOURCE,
        .position_x = 0.1f,
        .position_y = 0.1f,
        .position_z = 0.1f,
        .radius = 0.03f,
        .strength = 8.0f,
        .attached_object = 0,
        .attached_import = -1,
        .source_mode_3d = EMITTER_3D_SOURCE_MODE_HEATED_OBSTACLE,
        .surface_3d = EMITTER_3D_SURFACE_ALL_FACES,
        .obstacle_mode_3d = EMITTER_3D_OBSTACLE_MODE_RETAIN_ATTACHED,
        .thermal_buoyancy_3d = 6.0f,
    };

    backend = sim_runtime_backend_create(&cfg, &preset, &route, &visual);
    if (!backend) return false;

    scene.backend = backend;
    scene.preset = &preset;
    scene.config = &cfg;
    scene.emitters_enabled = true;
    scene.runtime_visual = visual;

    for (int i = 0; i < 4; ++i) {
        sim_runtime_backend_apply_emitters(backend, &scene, 0.1);
        sim_runtime_backend_step(backend, &scene, &cfg, 0.25);
    }
    if (!sim_runtime_backend_3d_test_view_refresh(backend, &impl)) return false;
    centroid_z = density_center_z(&impl);

    {
        bool density_live = !all_density_zero(&impl);
        bool velocity_live = any_velocity_non_zero(&impl);
        float max_density = max_abs_density_value(&impl);
        bool pass = density_live && velocity_live && centroid_z > 0.7;
        sim_runtime_backend_destroy(backend);
        if (!pass) {
            fprintf(stderr,
                    "heated obstacle external-shell diag: density_live=%d velocity_live=%d max_density=%.9f centroid_z=%.9f\n",
                    density_live ? 1 : 0,
                    velocity_live ? 1 : 0,
                    (double)max_density,
                    centroid_z);
        }
        return pass;
    }
}

static bool test_surface_patch_thermal_buoyancy_adds_scene_up_lift(void) {
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

    configure_tiny3d_domain(&cfg, &visual, 4.0, 2.0, 2.0);
    cfg.fluid_solver_iterations = 8;
    cfg.velocity_damping = 0.000006f;
    cfg.density_diffusion = 0.0f;
    cfg.density_decay = 0.0f;
    cfg.fluid_buoyancy_force = 0.0f;
    cfg.emitter_density_multiplier = 1.0f;

    visual.scene_up.valid = true;
    visual.scene_up.direction = (CoreObjectVec3){0.0, 0.0, 1.0};
    visual.scene_up.source = PHYSICS_SIM_RUNTIME_SCENE_UP_FALLBACK_POSITIVE_Z;

    preset.dimension_mode = SCENE_DIMENSION_MODE_3D;
    preset.object_count = 1;
    preset.objects[0] = (PresetObject){
        .type = PRESET_OBJECT_BOX,
        .position_x = 0.5f,
        .position_y = 0.5f,
        .position_z = 0.5f,
        .size_x = 0.12f,
        .size_y = 0.07f,
        .size_z = 0.10f,
        .angle = 0.0f,
        .is_static = true,
    };
    preset.emitter_count = 1;
    preset.emitters[0] = (FluidEmitter){
        .type = EMITTER_DENSITY_SOURCE,
        .position_x = 0.1f,
        .position_y = 0.1f,
        .position_z = 0.1f,
        .radius = 0.03f,
        .strength = 8.0f,
        .dir_x = 0.0f,
        .dir_y = 0.0f,
        .dir_z = 0.0f,
        .attached_object = 0,
        .attached_import = -1,
        .source_mode_3d = EMITTER_3D_SOURCE_MODE_SURFACE_PATCH,
        .surface_3d = EMITTER_3D_SURFACE_TOP,
        .obstacle_mode_3d = EMITTER_3D_OBSTACLE_MODE_RETAIN_ATTACHED,
        .thermal_buoyancy_3d = 6.0f,
    };

    backend = sim_runtime_backend_create(&cfg, &preset, &route, &visual);
    if (!backend) return false;

    scene.backend = backend;
    scene.preset = &preset;
    scene.config = &cfg;
    scene.emitters_enabled = true;
    scene.runtime_visual = visual;

    sim_runtime_backend_apply_emitters(backend, &scene, 0.1);
    if (!sim_runtime_backend_get_report(backend, &report)) return false;
    if (report.emitter_step_velocity_magnitude_delta <= 0.0f) return false;

    sim_runtime_backend_step(backend, &scene, &cfg, 0.25);
    if (!sim_runtime_backend_get_report(backend, &report)) return false;

    sim_runtime_backend_destroy(backend);
    return report.debug_volume_scene_up_velocity_valid &&
           report.debug_volume_scene_up_velocity_avg > 0.0f &&
           report.debug_volume_scene_up_velocity_peak > 0.0f;
}

static bool test_attached_import_world_footprint_stays_stable_across_quality_levels(void) {
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
    SimRuntimeBackend3DScaffoldTestView coarse_impl = {0};
    SimRuntimeBackend3DScaffoldTestView dense_impl = {0};
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
    dense_cfg.grid_w = 384;
    dense_cfg.grid_h = 384;

    visual.scene_domain.enabled = true;
    visual.scene_domain_authored = true;
    visual.scene_domain.min.x = -5.0;
    visual.scene_domain.min.y = -2.0;
    visual.scene_domain.min.z = -1.0;
    visual.scene_domain.max.x = 5.0;
    visual.scene_domain.max.y = 2.0;
    visual.scene_domain.max.z = 1.0;

    preset.import_shape_count = 1;
    preset.import_shapes[0] = (ImportedShape){
        .enabled = true,
        .position_x = 0.5f,
        .position_y = 0.5f,
        .position_z = 0.5f,
        .scale = 1.0f,
        .rotation_deg = 0.0f,
    };
    preset.emitter_count = 1;
    preset.emitters[0] = (FluidEmitter){
        .type = EMITTER_VELOCITY_JET,
        .position_x = 0.1f,
        .position_y = 0.1f,
        .position_z = 0.1f,
        .radius = 0.01f,
        .strength = 10.0f,
        .dir_x = 0.0f,
        .dir_y = 1.0f,
        .dir_z = 0.0f,
        .attached_object = -1,
        .attached_import = 0,
    };

    coarse_backend = sim_runtime_backend_create(&coarse_cfg, &preset, &route, &visual);
    dense_backend = sim_runtime_backend_create(&dense_cfg, &preset, &route, &visual);
    if (!coarse_backend || !dense_backend) goto fail;

    coarse_scene.backend = coarse_backend;
    coarse_scene.preset = &preset;
    coarse_scene.config = &coarse_cfg;
    coarse_scene.emitters_enabled = true;
    coarse_scene.import_shape_count = 1;
    coarse_scene.import_shapes[0] = preset.import_shapes[0];
    dense_scene.backend = dense_backend;
    dense_scene.preset = &preset;
    dense_scene.config = &dense_cfg;
    dense_scene.emitters_enabled = true;
    dense_scene.import_shape_count = 1;
    dense_scene.import_shapes[0] = preset.import_shapes[0];

    sim_runtime_backend_apply_emitters(coarse_backend, &coarse_scene, 0.1);
    sim_runtime_backend_apply_emitters(dense_backend, &dense_scene, 0.1);
    if (!sim_runtime_backend_3d_test_view_refresh(coarse_backend, &coarse_impl) ||
        !sim_runtime_backend_3d_test_view_refresh(dense_backend, &dense_impl)) {
        goto fail;
    }

    if (!collect_density_world_bounds(&coarse_impl,
                                      &coarse_min_x,
                                      &coarse_max_x,
                                      &coarse_min_y,
                                      &coarse_max_y,
                                      &coarse_min_z,
                                      &coarse_max_z,
                                      &coarse_cells)) {
        goto fail;
    }
    if (!collect_density_world_bounds(&dense_impl,
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

    tolerance = coarse_impl.volume.desc.voxel_size + dense_impl.volume.desc.voxel_size;
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

static bool test_attached_tilted_object_box_uses_full_3d_orientation(void) {
    AppConfig cfg = {0};
    FluidScenePreset preset = {0};
    SimModeRoute route = {
        .backend_lane = SIM_BACKEND_CONTROLLED_3D,
        .requested_space_mode = SPACE_MODE_3D,
        .projection_space_mode = SPACE_MODE_2D,
    };
    PhysicsSimRuntimeVisualBootstrap visual = {0};
    SimRuntimeBackend *backend = NULL;
    SimRuntimeBackend3DScaffoldTestView impl = {0};
    SceneState scene = {0};
    size_t along_normal_idx = 0;
    size_t along_legacy_z_idx = 0;

    cfg.quality_index = 1;
    cfg.grid_w = 128;
    cfg.grid_h = 128;
    cfg.emitter_density_multiplier = 1.0f;

    visual.scene_domain.enabled = true;
    visual.scene_domain_authored = true;
    visual.scene_domain.min.x = 0.0;
    visual.scene_domain.min.y = 0.0;
    visual.scene_domain.min.z = 0.0;
    visual.scene_domain.max.x = 1.0;
    visual.scene_domain.max.y = 1.0;
    visual.scene_domain.max.z = 1.0;

    preset.object_count = 1;
    preset.objects[0] = (PresetObject){
        .type = PRESET_OBJECT_BOX,
        .position_x = 0.5f,
        .position_y = 0.5f,
        .position_z = 0.5f,
        .size_x = 0.10f,
        .size_y = 0.20f,
        .size_z = 0.30f,
        .orientation_basis_valid = true,
        .orientation_u_x = 0.0f,
        .orientation_u_y = 1.0f,
        .orientation_u_z = 0.0f,
        .orientation_v_x = 0.0f,
        .orientation_v_y = 0.0f,
        .orientation_v_z = 1.0f,
        .orientation_w_x = 1.0f,
        .orientation_w_y = 0.0f,
        .orientation_w_z = 0.0f,
    };
    preset.emitter_count = 1;
    preset.emitters[0] = (FluidEmitter){
        .type = EMITTER_DENSITY_SOURCE,
        .position_x = 0.1f,
        .position_y = 0.1f,
        .position_z = 0.1f,
        .radius = 0.03f,
        .strength = 8.0f,
        .dir_x = 0.0f,
        .dir_y = 0.0f,
        .dir_z = 1.0f,
        .attached_object = 0,
        .attached_import = -1,
    };

    backend = sim_runtime_backend_create(&cfg, &preset, &route, &visual);
    if (!backend) return false;

    scene.backend = backend;
    scene.preset = &preset;
    scene.config = &cfg;
    scene.emitters_enabled = true;

    sim_runtime_backend_apply_emitters(backend, &scene, 0.1);
    if (!sim_runtime_backend_3d_test_view_refresh(backend, &impl)) return false;

    along_normal_idx = sim_runtime_3d_volume_index(&impl.volume.desc,
                                                   (impl.volume.desc.grid_w * 3) / 4,
                                                   impl.volume.desc.grid_h / 2,
                                                   impl.volume.desc.grid_d / 2);
    along_legacy_z_idx = sim_runtime_3d_volume_index(&impl.volume.desc,
                                                     impl.volume.desc.grid_w / 2,
                                                     impl.volume.desc.grid_h / 2,
                                                     (impl.volume.desc.grid_d * 3) / 4);
    if (impl.volume.density[along_normal_idx] <= 0.0f) return false;
    if (impl.volume.density[along_legacy_z_idx] > 0.0f) return false;

    sim_runtime_backend_destroy(backend);
    return true;
}

static bool test_tiny3d_attached_import_advects_along_rotated_axis(void) {
    AppConfig cfg = {0};
    FluidScenePreset preset = {0};
    SimModeRoute route = {
        .backend_lane = SIM_BACKEND_CONTROLLED_3D,
        .requested_space_mode = SPACE_MODE_3D,
        .projection_space_mode = SPACE_MODE_2D,
    };
    PhysicsSimRuntimeVisualBootstrap visual = {0};
    SimRuntimeBackend *backend = NULL;
    SimRuntimeBackend3DScaffoldTestView impl = {0};
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

    preset.import_shape_count = 1;
    preset.import_shapes[0] = (ImportedShape){
        .enabled = true,
        .position_x = 0.50f,
        .position_y = 0.50f,
        .position_z = 0.50f,
        .scale = 1.0f,
        .rotation_deg = 90.0f,
    };
    preset.emitter_count = 1;
    preset.emitters[0] = (FluidEmitter){
        .type = EMITTER_VELOCITY_JET,
        .position_x = 0.12f,
        .position_y = 0.12f,
        .position_z = 0.12f,
        .radius = 0.05f,
        .strength = 10.0f,
        .dir_x = 1.0f,
        .dir_y = 0.0f,
        .dir_z = 0.0f,
        .attached_object = -1,
        .attached_import = 0,
    };

    backend = sim_runtime_backend_create(&cfg, &preset, &route, &visual);
    if (!backend) return false;
    if (!sim_runtime_backend_3d_test_view_refresh(backend, &impl)) return false;
    if (impl.volume.desc.grid_w != 64 || impl.volume.desc.grid_h != 32 || impl.volume.desc.grid_d != 32) {
        return false;
    }

    scene.backend = backend;
    scene.preset = &preset;
    scene.config = &cfg;
    scene.emitters_enabled = true;
    scene.import_shape_count = 1;
    scene.import_shapes[0] = preset.import_shapes[0];

    sim_runtime_backend_apply_emitters(backend, &scene, 0.1);
    if (!sim_runtime_backend_3d_test_view_refresh(backend, &impl)) return false;
    center_before = density_center_y(&impl);

    for (int i = 0; i < 8; ++i) {
        sim_runtime_backend_step(backend, &scene, &cfg, 0.25);
    }
    if (!sim_runtime_backend_3d_test_view_refresh(backend, &impl)) return false;
    center_after = density_center_y(&impl);

    sim_runtime_backend_destroy(backend);
    return center_after > center_before;
}

int main(void) {
    if (!test_attached_object_box_emitter_writes_xyz_occupancy()) {
        fprintf(stderr,
                "sim_runtime_backend_3d_attached_emitter_contract_test: attached object occupancy failed\n");
        return 1;
    }
    if (!test_attached_import_emitter_rotates_direction_and_updates_compat_slice()) {
        fprintf(stderr,
                "sim_runtime_backend_3d_attached_emitter_contract_test: attached import occupancy failed\n");
        return 1;
    }
    if (!test_surface_patch_object_emitter_targets_top_face_band()) {
        fprintf(stderr,
                "sim_runtime_backend_3d_attached_emitter_contract_test: surface patch face band failed\n");
        return 1;
    }
    if (!test_surface_patch_density_launches_harder_than_legacy_volume_fill()) {
        fprintf(stderr,
                "sim_runtime_backend_3d_attached_emitter_contract_test: surface patch launch energy failed\n");
        return 1;
    }
    if (!test_surface_shell_all_faces_avoids_center_fill()) {
        fprintf(stderr,
                "sim_runtime_backend_3d_attached_emitter_contract_test: surface shell footprint failed\n");
        return 1;
    }
    if (!test_heated_obstacle_uses_top_biased_shell_release()) {
        fprintf(stderr,
                "sim_runtime_backend_3d_attached_emitter_contract_test: heated obstacle release failed\n");
        return 1;
    }
    if (!test_heated_obstacle_external_shell_persists_across_runtime_steps()) {
        fprintf(stderr,
                "sim_runtime_backend_3d_attached_emitter_contract_test: heated obstacle external shell persistence failed\n");
        return 1;
    }
    if (!test_surface_patch_thermal_buoyancy_adds_scene_up_lift()) {
        fprintf(stderr,
                "sim_runtime_backend_3d_attached_emitter_contract_test: surface patch thermal lift failed\n");
        return 1;
    }
    if (!test_tiny3d_attached_import_advects_along_rotated_axis()) {
        fprintf(stderr,
                "sim_runtime_backend_3d_attached_emitter_contract_test: tiny attached transport failed\n");
        return 1;
    }
    if (!test_attached_tilted_object_box_uses_full_3d_orientation()) {
        fprintf(stderr,
                "sim_runtime_backend_3d_attached_emitter_contract_test: retained 3d emitter orientation failed\n");
        return 1;
    }
    if (!test_attached_import_world_footprint_stays_stable_across_quality_levels()) {
        fprintf(stderr,
                "sim_runtime_backend_3d_attached_emitter_contract_test: world footprint stability failed\n");
        return 1;
    }
    fprintf(stdout, "sim_runtime_backend_3d_attached_emitter_contract_test: success\n");
    return 0;
}
