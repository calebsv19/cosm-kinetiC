#include "app/scene_state.h"
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

static SimRuntimeBackend *create_backend(AppConfig *cfg, PhysicsSimRuntimeVisualBootstrap *visual) {
    SimModeRoute route = {
        .backend_lane = SIM_BACKEND_CONTROLLED_3D,
        .requested_space_mode = SPACE_MODE_3D,
        .projection_space_mode = SPACE_MODE_2D,
    };
    return sim_runtime_backend_create(cfg, NULL, &route, visual);
}

static bool test_static_preset_object_marks_volumetric_occupancy(void) {
    AppConfig cfg = {0};
    PhysicsSimRuntimeVisualBootstrap visual = {0};
    SimRuntimeBackend *backend = NULL;
    SimRuntimeBackend3DScaffold *impl = NULL;
    FluidScenePreset preset = {0};
    SceneState scene = {0};
    size_t center_idx = 0;
    size_t wall_idx = 0;

    cfg.quality_index = 1;
    cfg.grid_w = 128;
    cfg.grid_h = 128;
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
        .size_y = 0.08f,
        .size_z = 0.12f,
        .is_static = true,
    };

    backend = create_backend(&cfg, &visual);
    if (!backend) return false;
    impl = (SimRuntimeBackend3DScaffold *)backend->impl;
    if (!impl) return false;

    scene.backend = backend;
    scene.preset = &preset;
    scene.config = &cfg;

    sim_runtime_backend_build_static_obstacles(backend, &scene);

    center_idx = sim_runtime_3d_volume_index(&impl->volume.desc,
                                             impl->volume.desc.grid_w / 2,
                                             impl->volume.desc.grid_h / 2,
                                             impl->volume.desc.grid_d / 2);
    wall_idx = sim_runtime_3d_volume_index(&impl->volume.desc,
                                           impl->volume.desc.grid_w / 2,
                                           impl->volume.desc.grid_h / 2,
                                           0);
    if (!impl->obstacle_occupancy[center_idx]) return false;
    if (!impl->obstacle_occupancy[wall_idx]) return false;

    sim_runtime_backend_destroy(backend);
    return true;
}

static bool test_attached_object_is_skipped_from_obstacle_occupancy(void) {
    AppConfig cfg = {0};
    PhysicsSimRuntimeVisualBootstrap visual = {0};
    SimRuntimeBackend *backend = NULL;
    SimRuntimeBackend3DScaffold *impl = NULL;
    FluidScenePreset preset = {0};
    SceneState scene = {0};
    size_t center_idx = 0;

    cfg.quality_index = 1;
    cfg.grid_w = 128;
    cfg.grid_h = 128;
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
        .size_y = 0.08f,
        .size_z = 0.12f,
        .is_static = true,
    };
    preset.emitter_count = 1;
    preset.emitters[0] = (FluidEmitter){
        .type = EMITTER_DENSITY_SOURCE,
        .attached_object = 0,
        .attached_import = -1,
    };

    backend = create_backend(&cfg, &visual);
    if (!backend) return false;
    impl = (SimRuntimeBackend3DScaffold *)backend->impl;
    if (!impl) return false;

    scene.backend = backend;
    scene.preset = &preset;
    scene.config = &cfg;

    sim_runtime_backend_build_static_obstacles(backend, &scene);

    center_idx = sim_runtime_3d_volume_index(&impl->volume.desc,
                                             impl->volume.desc.grid_w / 2,
                                             impl->volume.desc.grid_h / 2,
                                             impl->volume.desc.grid_d / 2);
    if (impl->obstacle_occupancy[center_idx]) return false;

    sim_runtime_backend_destroy(backend);
    return true;
}

static bool test_import_occupancy_tracks_live_transform_updates(void) {
    AppConfig cfg = {0};
    PhysicsSimRuntimeVisualBootstrap visual = {0};
    SimRuntimeBackend *backend = NULL;
    SimRuntimeBackend3DScaffold *impl = NULL;
    FluidScenePreset preset = {0};
    SceneState scene = {0};
    int y = 0;
    int z = 0;
    size_t left_idx = 0;
    size_t right_idx = 0;

    cfg.quality_index = 1;
    cfg.grid_w = 128;
    cfg.grid_h = 128;
    visual.scene_domain.enabled = true;
    visual.scene_domain_authored = true;
    visual.scene_domain.min.x = 0.0;
    visual.scene_domain.min.y = 0.0;
    visual.scene_domain.min.z = 0.0;
    visual.scene_domain.max.x = 1.0;
    visual.scene_domain.max.y = 1.0;
    visual.scene_domain.max.z = 1.0;

    backend = create_backend(&cfg, &visual);
    if (!backend) return false;
    impl = (SimRuntimeBackend3DScaffold *)backend->impl;
    if (!impl) return false;

    preset.import_shape_count = 1;
    scene.backend = backend;
    scene.preset = &preset;
    scene.config = &cfg;
    scene.import_shape_count = 1;
    scene.import_shapes[0] = (ImportedShape){
        .enabled = true,
        .is_static = false,
        .gravity_enabled = true,
        .position_x = 0.25f,
        .position_y = 0.5f,
        .position_z = 0.5f,
        .rotation_deg = 0.0f,
        .scale = 1.0f,
    };

    sim_runtime_backend_build_obstacles(backend, &scene);

    y = impl->volume.desc.grid_h / 2;
    z = impl->volume.desc.grid_d / 2;
    left_idx = sim_runtime_3d_volume_index(&impl->volume.desc,
                                           impl->volume.desc.grid_w / 4,
                                           y,
                                           z);
    right_idx = sim_runtime_3d_volume_index(&impl->volume.desc,
                                            (impl->volume.desc.grid_w * 3) / 4,
                                            y,
                                            z);
    if (!impl->obstacle_occupancy[left_idx]) return false;
    if (impl->obstacle_occupancy[right_idx]) return false;

    scene.import_shapes[0].position_x = 0.75f;
    scene.import_shapes[0].rotation_deg = 35.0f;
    sim_runtime_backend_rasterize_dynamic_obstacles(backend, &scene);

    if (impl->obstacle_occupancy[left_idx]) return false;
    if (!impl->obstacle_occupancy[right_idx]) return false;

    sim_runtime_backend_destroy(backend);
    return true;
}

static bool test_tilted_retained_object_uses_full_3d_orientation_for_occupancy(void) {
    AppConfig cfg = {0};
    PhysicsSimRuntimeVisualBootstrap visual = {0};
    SimRuntimeBackend *backend = NULL;
    SimRuntimeBackend3DScaffold *impl = NULL;
    FluidScenePreset preset = {0};
    SceneState scene = {0};
    size_t along_normal_idx = 0;
    size_t along_legacy_z_idx = 0;

    cfg.quality_index = 1;
    cfg.grid_w = 128;
    cfg.grid_h = 128;
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
        .is_static = true,
    };

    backend = create_backend(&cfg, &visual);
    if (!backend) return false;
    impl = (SimRuntimeBackend3DScaffold *)backend->impl;
    if (!impl) return false;

    scene.backend = backend;
    scene.preset = &preset;
    scene.config = &cfg;

    sim_runtime_backend_build_static_obstacles(backend, &scene);

    along_normal_idx = sim_runtime_3d_volume_index(&impl->volume.desc,
                                                   (impl->volume.desc.grid_w * 3) / 4,
                                                   impl->volume.desc.grid_h / 2,
                                                   impl->volume.desc.grid_d / 2);
    along_legacy_z_idx = sim_runtime_3d_volume_index(&impl->volume.desc,
                                                     impl->volume.desc.grid_w / 2,
                                                     impl->volume.desc.grid_h / 2,
                                                     (impl->volume.desc.grid_d * 3) / 4);
    if (!impl->obstacle_occupancy[along_normal_idx]) return false;
    if (impl->obstacle_occupancy[along_legacy_z_idx]) return false;

    sim_runtime_backend_destroy(backend);
    return true;
}

static bool test_dynamic_rasterize_keeps_obstacle_volume_fresh(void) {
    AppConfig cfg = {0};
    PhysicsSimRuntimeVisualBootstrap visual = {0};
    SimRuntimeBackend *backend = NULL;
    SimRuntimeBackend3DScaffold *impl = NULL;
    FluidScenePreset preset = {0};
    SceneState scene = {0};
    SceneObstacleFieldView2D obstacle = {0};

    cfg.quality_index = 1;
    cfg.grid_w = 128;
    cfg.grid_h = 128;
    visual.scene_domain.enabled = true;
    visual.scene_domain_authored = true;
    visual.scene_domain.min.x = 0.0;
    visual.scene_domain.min.y = 0.0;
    visual.scene_domain.min.z = 0.0;
    visual.scene_domain.max.x = 1.0;
    visual.scene_domain.max.y = 1.0;
    visual.scene_domain.max.z = 1.0;

    backend = create_backend(&cfg, &visual);
    if (!backend) return false;
    impl = (SimRuntimeBackend3DScaffold *)backend->impl;
    if (!impl) return false;

    preset.import_shape_count = 1;
    scene.backend = backend;
    scene.preset = &preset;
    scene.config = &cfg;
    scene.import_shape_count = 1;
    scene.import_shapes[0] = (ImportedShape){
        .enabled = true,
        .is_static = false,
        .gravity_enabled = true,
        .position_x = 0.5f,
        .position_y = 0.5f,
        .position_z = 0.5f,
        .rotation_deg = 15.0f,
        .scale = 1.0f,
    };

    sim_runtime_backend_rasterize_dynamic_obstacles(backend, &scene);
    if (impl->obstacle_volume_dirty) return false;
    if (impl->obstacle_slice_dirty) return false;
    if (!sim_runtime_backend_get_obstacle_view_2d(backend, &obstacle)) return false;
    if (!obstacle.solid_mask) return false;
    if (impl->obstacle_volume_dirty) return false;
    if (impl->obstacle_slice_dirty) return false;

    sim_runtime_backend_destroy(backend);
    return true;
}

int main(void) {
    if (!test_static_preset_object_marks_volumetric_occupancy()) {
        fprintf(stderr,
                "sim_runtime_backend_3d_retained_obstacle_contract_test: static preset occupancy failed\n");
        return 1;
    }
    if (!test_attached_object_is_skipped_from_obstacle_occupancy()) {
        fprintf(stderr,
                "sim_runtime_backend_3d_retained_obstacle_contract_test: attached object skip failed\n");
        return 1;
    }
    if (!test_import_occupancy_tracks_live_transform_updates()) {
        fprintf(stderr,
                "sim_runtime_backend_3d_retained_obstacle_contract_test: import transform tracking failed\n");
        return 1;
    }
    if (!test_tilted_retained_object_uses_full_3d_orientation_for_occupancy()) {
        fprintf(stderr,
                "sim_runtime_backend_3d_retained_obstacle_contract_test: retained 3d orientation failed\n");
        return 1;
    }
    if (!test_dynamic_rasterize_keeps_obstacle_volume_fresh()) {
        fprintf(stderr,
                "sim_runtime_backend_3d_retained_obstacle_contract_test: obstacle freshness failed\n");
        return 1;
    }
    fprintf(stdout, "sim_runtime_backend_3d_retained_obstacle_contract_test: success\n");
    return 0;
}
