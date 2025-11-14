#ifndef SCENE_CONTROLLER_H
#define SCENE_CONTROLLER_H

#include "app/app_config.h"
#include "app/scene_presets.h"

// Runs the lifetime of the application: initializes SDL subsystems via the
// renderer, owns the SceneState, pumps input, drains the command bus, and
// steps physics/rendering until the user quits.
int scene_controller_run(const AppConfig *initial_cfg,
                         const FluidScenePreset *preset,
                         const char *snapshot_dir);

#endif // SCENE_CONTROLLER_H
