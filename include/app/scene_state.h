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
#include "physics/objects/object_manager.h"

typedef struct SceneState {
    double time;
    double dt;
    bool   paused;
    bool   emitters_enabled;

    Fluid2D *smoke;
    const FluidScenePreset *preset;

    const AppConfig *config; // non-owning pointer
    ObjectManager objects;
    uint8_t *static_mask;
    int     wind_ramp_steps;
} SceneState;

SceneState scene_create(const AppConfig *cfg, const FluidScenePreset *preset);
void       scene_destroy(SceneState *scene);

void scene_apply_input(SceneState *scene, const InputCommands *cmds);
bool scene_handle_command(SceneState *scene, const Command *cmd);
bool scene_apply_brush_sample(SceneState *scene, const StrokeSample *sample);
void scene_apply_emitters(SceneState *scene, double dt);
void scene_apply_boundary_flows(SceneState *scene, double dt);
void scene_enforce_boundary_flows(SceneState *scene);
void scene_set_emitters_enabled(SceneState *scene, bool enabled);
void scene_enforce_obstacles(SceneState *scene);

// Snapshot export (Phase 1 basic implementation)
bool scene_export_snapshot(const SceneState *scene, const char *path);

#endif // SCENE_STATE_H
