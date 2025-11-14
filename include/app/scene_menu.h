#ifndef SCENE_MENU_H
#define SCENE_MENU_H

#include <stdbool.h>

#include "app/app_config.h"
#include "app/scene_presets.h"
#include "app/preset_io.h"

typedef struct SceneMenuResult {
    const FluidScenePreset *preset;
    AppConfig              config_overrides;
} SceneMenuResult;

typedef struct SceneMenuSelection {
    int  custom_slot_index;
} SceneMenuSelection;

// Runs the SDL scene editor menu. Returns true if the user pressed Start.
bool scene_menu_run(AppConfig *cfg,
                    FluidScenePreset *in_out_preset,
                    SceneMenuSelection *selection,
                    CustomPresetLibrary *library);

#endif // SCENE_MENU_H
