#ifndef SCENE_EDITOR_H
#define SCENE_EDITOR_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>

#include "app/app_config.h"
#include "app/scene_presets.h"

typedef struct SceneEditorResult {
    bool applied;
} SceneEditorResult;

bool scene_editor_run(SDL_Window *window,
                      SDL_Renderer *renderer,
                      TTF_Font *font_main,
                      TTF_Font *font_small,
                      const AppConfig *cfg,
                      FluidScenePreset *preset);

#endif // SCENE_EDITOR_H
