#ifndef SCENE_STATE_H
#define SCENE_STATE_H

#include <stdbool.h>

#include "app/app_config.h"
#include "command/command_bus.h"
#include "physics/fluid2d/fluid2d.h"
#include "input/input.h"
#include "input/stroke_buffer.h"
#include "app/scene_presets.h"

typedef struct SceneState {
    double time;
    double dt;
    bool   paused;

    Fluid2D *smoke;
    const FluidScenePreset *preset;

    const AppConfig *config; // non-owning pointer
} SceneState;

SceneState scene_create(const AppConfig *cfg, const FluidScenePreset *preset);
void       scene_destroy(SceneState *scene);

void scene_apply_input(SceneState *scene, const InputCommands *cmds);
bool scene_handle_command(SceneState *scene, const Command *cmd);
bool scene_apply_brush_sample(SceneState *scene, const StrokeSample *sample);
void scene_apply_emitters(SceneState *scene, double dt);

// Snapshot export (Phase 1 basic implementation)
bool scene_export_snapshot(const SceneState *scene, const char *path);

#endif // SCENE_STATE_H
