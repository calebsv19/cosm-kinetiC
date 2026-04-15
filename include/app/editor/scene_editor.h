#ifndef SCENE_EDITOR_H
#define SCENE_EDITOR_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>

#include "app/app_config.h"
#include "app/editor/scene_editor_session.h"
#include "app/scene_presets.h"
#include "app/preset_io.h"
#include "input/input_context.h"
#include "geo/shape_library.h"

typedef struct SceneEditorResult {
    bool applied;
    bool has_retained_scene;
    char retained_runtime_scene_path[512];
} SceneEditorResult;

bool scene_editor_run(SDL_Window *window,
                      SDL_Renderer *renderer,
                      TTF_Font *font_main,
                      TTF_Font *font_small,
                      AppConfig *cfg,
                      FluidScenePreset *preset,
                      const SceneEditorBootstrap *bootstrap,
                      SceneEditorResult *result,
                      InputContextManager *ctx_mgr,
                      const ShapeAssetLibrary *shape_library,
                      char *name_buffer,
                      size_t name_capacity);

#endif // SCENE_EDITOR_H
