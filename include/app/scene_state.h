#ifndef SCENE_STATE_H
#define SCENE_STATE_H

#include <stdbool.h>
#include <stdint.h>

#include "app/app_config.h"
#include "command/command_bus.h"
#include "physics/fluid2d/fluid2d.h"
#include "input/input.h"
#include "input/stroke_buffer.h"
#include "app/scene_presets.h"
#include "app/sim_mode.h"
#include "physics/objects/object_manager.h"
#include "geo/shape_library.h"
#include "import/runtime_scene_bridge.h"
#include "app/editor/scene_editor_viewport.h"

typedef struct SceneState {
    double time;
    double dt;
    bool   paused;
    bool   emitters_enabled;
    SimModeRoute mode_route;

    Fluid2D *smoke;
    const FluidScenePreset *preset;

    const AppConfig *config; // non-owning pointer
    ObjectManager objects;
    bool objects_gravity_enabled;
    bool objects_elastic;
    uint8_t *static_mask;
    uint8_t *obstacle_mask;
    float   *obstacle_velX;
    float   *obstacle_velY;
    float   *obstacle_distance;
    bool     obstacle_mask_dirty;
    int      wind_ramp_steps;

    // Imported ShapeLib assets baked into the static mask.
    size_t import_shape_count;
    ImportedShape import_shapes[MAX_IMPORTED_SHAPES];
    int import_body_map[MAX_IMPORTED_SHAPES]; // index into objects, -1 if none

    // Shared ShapeAsset library (non-owning pointer).
    const ShapeAssetLibrary *shape_library;

    // Rasterized emitter masks for emitters attached to imports (grid-sized masks).
    struct {
        uint8_t *mask;
        int min_x, max_x, min_y, max_y;
    } emitter_masks[MAX_FLUID_EMITTERS];
    bool emitter_masks_dirty;

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
void       scene_destroy(SceneState *scene);

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

// Snapshot export (Phase 1 basic implementation)
bool scene_export_snapshot(const SceneState *scene, const char *path);

#endif // SCENE_STATE_H
