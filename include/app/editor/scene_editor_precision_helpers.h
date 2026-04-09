#ifndef SCENE_EDITOR_PRECISION_HELPERS_H
#define SCENE_EDITOR_PRECISION_HELPERS_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>

#include "app/scene_presets.h"
#include "geo/shape_library.h"

int scene_editor_precision_local_emitter_index_for_object(const FluidScenePreset *preset, int obj_index);
int scene_editor_precision_local_emitter_index_for_import(const FluidScenePreset *preset, int import_index);

void scene_editor_precision_screen_to_normalized(int w,
                                                 int h,
                                                 int sx,
                                                 int sy,
                                                 float *out_x,
                                                 float *out_y);

void scene_editor_precision_draw_import_outline(SDL_Renderer *renderer,
                                                const ImportedShape *imp,
                                                const ShapeAssetLibrary *lib,
                                                int win_w,
                                                int win_h,
                                                bool selected,
                                                bool hovered,
                                                const SDL_Color *tint_override);

SDL_Color scene_editor_precision_emitter_color(const FluidEmitter *em);

void scene_editor_precision_draw_import_overlays(SDL_Renderer *renderer,
                                                 const FluidScenePreset *scene,
                                                 const ShapeAssetLibrary *lib,
                                                 int win_w,
                                                 int win_h,
                                                 int selected_import,
                                                 int hover_import);

void scene_editor_precision_draw_hover_tooltip(SDL_Renderer *renderer,
                                               TTF_Font *font_small,
                                               const FluidScenePreset *scene,
                                               int pointer_x,
                                               int pointer_y,
                                               int hover_object,
                                               int hover_import,
                                               int hover_emitter,
                                               int hover_edge);

#endif
