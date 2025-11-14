#ifndef SCENE_MENU_H
#define SCENE_MENU_H

#include <stdbool.h>

#include "app/app_config.h"
#include "app/scene_presets.h"

typedef struct SceneMenuResult {
    const FluidScenePreset *preset;
    AppConfig              config_overrides;
} SceneMenuResult;

// Runs the SDL scene editor menu. Returns true if the user pressed Start.
bool scene_menu_run(AppConfig *cfg,
                    FluidScenePreset *in_out_preset,
                    int *in_out_preset_index);

#endif // SCENE_MENU_H
