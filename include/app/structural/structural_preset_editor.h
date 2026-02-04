#ifndef STRUCTURAL_PRESET_EDITOR_H
#define STRUCTURAL_PRESET_EDITOR_H

#include <stdbool.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "app/app_config.h"
#include "input/input_context.h"

bool structural_preset_editor_run(SDL_Window *window,
                                  SDL_Renderer *renderer,
                                  TTF_Font *font_main,
                                  TTF_Font *font_small,
                                  const AppConfig *cfg,
                                  const char *preset_path,
                                  InputContextManager *ctx_mgr);

#endif // STRUCTURAL_PRESET_EDITOR_H
