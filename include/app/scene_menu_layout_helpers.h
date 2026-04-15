#ifndef SCENE_MENU_LAYOUT_HELPERS_H
#define SCENE_MENU_LAYOUT_HELPERS_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stddef.h>

#include "app/menu/menu_types.h"

int scene_menu_font_height(SDL_Renderer *renderer, TTF_Font *font, int fallback);

void scene_menu_fit_text_to_width(SDL_Renderer *renderer,
                                  TTF_Font *font,
                                  const char *text,
                                  int max_width,
                                  char *out,
                                  size_t out_size);

void scene_menu_update_dynamic_layout(SceneMenuInteraction *ctx,
                                      int win_w,
                                      int win_h);

#endif // SCENE_MENU_LAYOUT_HELPERS_H
