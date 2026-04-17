#ifndef SCENE_STATE_H
#define SCENE_STATE_H

#include <stdbool.h>
#include <stdint.h>

#include "app/app_config.h"
#include "app/editor/scene_editor_viewport.h"
#include "app/scene_presets.h"
#include "app/sim_mode.h"
#include "app/sim_runtime_backend.h"
#include "command/command_bus.h"
#include "geo/shape_library.h"
#include "import/runtime_scene_bridge.h"
#include "input/input.h"
#include "input/stroke_buffer.h"
#include "physics/objects/object_manager.h"

typedef struct SceneState {
    double time;
    double dt;
    bool paused;
    bool emitters_enabled;
    SimModeRoute mode_route;

    SimRuntimeBackend *backend;
    const FluidScenePreset *preset;

    const AppConfig *config; // non-owning pointer
    ObjectManager objects;
    bool objects_gravity_enabled;
    bool objects_elastic;

    // Imported ShapeLib assets baked into backend-owned masks or runtime bodies.
    size_t import_shape_count;
    ImportedShape import_shapes[MAX_IMPORTED_SHAPES];
    int import_body_map[MAX_IMPORTED_SHAPES]; // index into objects, -1 if none

    // Shared ShapeAsset library (non-owning pointer).
    const ShapeAssetLibrary *shape_library;

    // Stored initial transforms for imports (normalized positions, degrees rotation).
    float import_start_pos_x[MAX_IMPORTED_SHAPES];
    float import_start_pos_y[MAX_IMPORTED_SHAPES];
    float import_start_rot_deg[MAX_IMPORTED_SHAPES];

    PhysicsSimRuntimeVisualBootstrap runtime_visual;
    SceneEditorViewportState runtime_viewport;
} SceneState;

SceneState scene_create(const AppConfig *cfg,
                        const FluidScenePreset *preset,
                        const ShapeAssetLibrary *shape_library,
                        const SimModeRoute *mode_route);
void scene_destroy(SceneState *scene);

void scene_apply_input(SceneState *scene, const InputCommands *cmds);
bool scene_handle_command(SceneState *scene, const Command *cmd);
bool scene_apply_brush_sample(SceneState *scene, const StrokeSample *sample);
void scene_apply_emitters(SceneState *scene, double dt);
void scene_apply_boundary_flows(SceneState *scene, double dt);
void scene_enforce_boundary_flows(SceneState *scene);
void scene_set_emitters_enabled(SceneState *scene, bool enabled);
void scene_enforce_obstacles(SceneState *scene);
void scene_rasterize_dynamic_obstacles(SceneState *scene);
bool scene_load_runtime_visual_bootstrap(SceneState *scene,
                                         const char *runtime_scene_path);

static inline bool scene_backend_fluid_view_2d(const SceneState *scene,
                                               SceneFluidFieldView2D *out_view) {
    return scene && scene->backend && scene->backend->ops &&
           scene->backend->ops->get_fluid_view_2d &&
           scene->backend->ops->get_fluid_view_2d(scene->backend, out_view);
}

static inline bool scene_backend_obstacle_view_2d(const SceneState *scene,
                                                  SceneObstacleFieldView2D *out_view) {
    return scene && scene->backend && scene->backend->ops &&
           scene->backend->ops->get_obstacle_view_2d &&
           scene->backend->ops->get_obstacle_view_2d(scene->backend, out_view);
}

static inline bool scene_backend_report(const SceneState *scene,
                                        SimRuntimeBackendReport *out_report) {
    return scene && scene->backend && scene->backend->ops &&
           scene->backend->ops->get_report &&
           scene->backend->ops->get_report(scene->backend, out_report);
}

static inline bool scene_backend_step_compatibility_slice(SceneState *scene, int delta_z) {
    return scene && scene->backend &&
           sim_runtime_backend_step_compatibility_slice(scene->backend, delta_z);
}

static inline bool scene_backend_compatibility_slice_activity(const SceneState *scene,
                                                              int slice_z,
                                                              bool *out_has_fluid,
                                                              bool *out_has_obstacles) {
    return scene && scene->backend &&
           sim_runtime_backend_get_compatibility_slice_activity(
               scene->backend, slice_z, out_has_fluid, out_has_obstacles);
}

static inline void scene_backend_mark_obstacles_dirty(SceneState *scene) {
    if (!scene || !scene->backend || !scene->backend->ops ||
        !scene->backend->ops->mark_obstacles_dirty) {
        return;
    }
    scene->backend->ops->mark_obstacles_dirty(scene->backend);
}

// Snapshot export (Phase 1 basic implementation)
bool scene_export_snapshot(const SceneState *scene, const char *path);

#endif // SCENE_STATE_H
