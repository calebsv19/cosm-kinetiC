#ifndef SCENE_EDITOR_PRECISION_H
#define SCENE_EDITOR_PRECISION_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include "app/app_config.h"
#include "app/scene_presets.h"

// Launches a full-size precision editor window at the simulation window size.
// Mutates `working` and `selected_object` if the user applies changes.
// Returns true if applied, false if canceled. Sets *dirty_out when changes occurred.
bool scene_editor_run_precision(const AppConfig *cfg,
                                FluidScenePreset *working,
                                int *selected_object,
                                TTF_Font *font_small,
                                TTF_Font *font_main,
                                bool *dirty_out);

#endif // SCENE_EDITOR_PRECISION_H
