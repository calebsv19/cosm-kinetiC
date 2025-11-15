#ifndef SCENE_CONTROLLER_H
#define SCENE_CONTROLLER_H

#include "app/app_config.h"
#include "app/scene_presets.h"

typedef struct HeadlessOptions {
    bool enabled;
    int  frame_limit;
    bool skip_present;
    bool ignore_input;
    bool preserve_input;
    bool preserve_sdl_state;
} HeadlessOptions;

// Runs the lifetime of the application: initializes SDL subsystems via the
// renderer, owns the SceneState, pumps input, drains the command bus, and
// steps physics/rendering until the user quits.
int scene_controller_run(const AppConfig *initial_cfg,
                         const FluidScenePreset *preset,
                         const char *snapshot_dir,
                         const HeadlessOptions *headless);

#endif // SCENE_CONTROLLER_H
