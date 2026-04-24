#include "app/scene_state.h"

#include <stdio.h>
#include <string.h>

#include "app/scene_imports.h"
#include "app/scene_masks.h"
#include "app/scene_objects.h"

static bool scene_recreate_backend(SceneState *scene,
                                   const PhysicsSimRuntimeVisualBootstrap *runtime_visual) {
    SimRuntimeBackend *next = NULL;
    if (!scene) return false;
    next = sim_runtime_backend_create(scene->config,
                                      scene->preset,
                                      &scene->mode_route,
                                      runtime_visual);
    if (!next) return false;
    sim_runtime_backend_destroy(scene->backend);
    scene->backend = next;
    return true;
}

SceneState scene_create(const AppConfig *cfg,
                        const FluidScenePreset *preset,
                        const ShapeAssetLibrary *shape_library,
                        const SimModeRoute *mode_route) {
    SceneState s;
    memset(&s, 0, sizeof(s));

    s.time = 0.0;
    s.dt = 0.0;
    s.paused = false;
    s.emitters_enabled = true;
    if (mode_route) {
        s.mode_route = *mode_route;
    } else if (cfg) {
        s.mode_route = sim_mode_resolve_route(cfg->sim_mode, cfg->space_mode);
    } else {
        s.mode_route = sim_mode_resolve_route(SIM_MODE_BOX, SPACE_MODE_2D);
    }
    s.config = cfg;
    s.preset = preset;
    s.runtime_slice_overlay_enabled = true;
    memset(&s.runtime_visual, 0, sizeof(s.runtime_visual));
    scene_editor_viewport_init(&s.runtime_viewport, SPACE_MODE_3D, SPACE_MODE_3D);
    (void)scene_recreate_backend(&s, NULL);

    scene_objects_init(&s);
    s.shape_library = shape_library;

    if (preset) {
        size_t copy_count = (preset->import_shape_count < MAX_IMPORTED_SHAPES)
                                ? preset->import_shape_count
                                : MAX_IMPORTED_SHAPES;
        for (size_t i = 0; i < copy_count; ++i) {
            s.import_shapes[s.import_shape_count] = preset->import_shapes[i];
            s.import_start_pos_x[s.import_shape_count] = preset->import_shapes[i].position_x;
            s.import_start_pos_y[s.import_shape_count] = preset->import_shapes[i].position_y;
            s.import_start_rot_deg[s.import_shape_count] = preset->import_shapes[i].rotation_deg;
            s.import_shape_count++;
        }
        scene_imports_resolve(&s);
        scene_imports_rebuild_bodies(&s);
        scene_objects_add_presets(&s);
        scene_masks_build_static(&s);
        scene_masks_build_emitter(&s);
    }

    return s;
}

void scene_destroy(SceneState *scene) {
    if (!scene) return;
    sim_runtime_backend_destroy(scene->backend);
    scene->backend = NULL;
    scene_objects_shutdown(scene);
}

void scene_apply_input(SceneState *scene, const InputCommands *cmds) {
    if (!scene || !cmds) return;
    (void)cmds;
}

bool scene_handle_command(SceneState *scene, const Command *cmd) {
    if (!scene || !cmd) return false;

    switch (cmd->type) {
    case COMMAND_TOGGLE_PAUSE:
        scene->paused = !scene->paused;
        return true;
    case COMMAND_CLEAR_SMOKE:
        sim_runtime_backend_clear(scene->backend);
        return true;
    case COMMAND_TOGGLE_OBJECT_GRAVITY:
        scene_objects_reset_gravity(scene);
        return true;
    case COMMAND_TOGGLE_ELASTIC_COLLISIONS:
        scene_objects_set_elastic(scene, !scene->objects_elastic);
        return true;
    default:
        return false;
    }
}

bool scene_apply_brush_sample(SceneState *scene, const StrokeSample *sample) {
    if (!scene || !sample) return false;
    return sim_runtime_backend_apply_brush_sample(scene->backend, scene->config, sample);
}

void scene_set_emitters_enabled(SceneState *scene, bool enabled) {
    if (!scene) return;
    scene->emitters_enabled = enabled;
}

void scene_rasterize_dynamic_obstacles(SceneState *scene) {
    if (!scene) return;
    sim_runtime_backend_rasterize_dynamic_obstacles(scene->backend, scene);
}

bool scene_load_runtime_visual_bootstrap(SceneState *scene,
                                         const char *runtime_scene_path) {
    PhysicsSimRuntimeVisualBootstrap bootstrap = {0};
    char diagnostics[256];
    if (!scene || !runtime_scene_path || !runtime_scene_path[0]) return false;
    if (!runtime_scene_bridge_load_visual_bootstrap_file(runtime_scene_path,
                                                         &bootstrap,
                                                         diagnostics,
                                                         sizeof(diagnostics))) {
        fprintf(stderr, "[scene] Retained runtime bootstrap failed: %s\n", diagnostics);
        return false;
    }
    if (!bootstrap.valid || !bootstrap.retained_scene.valid_contract) {
        fprintf(stderr, "[scene] Retained runtime bootstrap invalid.\n");
        return false;
    }
    scene->runtime_visual = bootstrap;
    if (scene->mode_route.backend_lane == SIM_BACKEND_CONTROLLED_3D) {
        if (!scene_recreate_backend(scene, &scene->runtime_visual)) {
            fprintf(stderr, "[scene] Failed to recreate 3D backend from retained runtime bootstrap.\n");
            return false;
        }
        scene_masks_build_static(scene);
        scene_masks_build_emitter(scene);
    }
    return true;
}

bool scene_export_snapshot(const SceneState *scene, const char *path) {
    if (!scene) return false;
    return sim_runtime_backend_export_snapshot(scene->backend, scene->time, path);
}
