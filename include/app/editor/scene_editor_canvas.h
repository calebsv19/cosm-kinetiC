#ifndef SCENE_EDITOR_CANVAS_H
#define SCENE_EDITOR_CANVAS_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>

#include "app/scene_presets.h"
#include "ui/text_input.h"

#define SCENE_EDITOR_SELECT_HIGHLIGHT_FACTOR 0.35f
#define SCENE_EDITOR_OBJECT_HANDLE_MARGIN 0.05f
#define SCENE_EDITOR_OBJECT_HANDLE_MIN 0.02f
#define SCENE_EDITOR_OBJECT_HANDLE_MAX 0.6f

typedef enum EditorDragMode {
    DRAG_NONE = 0,
    DRAG_POSITION,
    DRAG_DIRECTION
} EditorDragMode;

void scene_editor_canvas_project(int canvas_x,
                                 int canvas_y,
                                 int canvas_size,
                                 float px,
                                 float py,
                                 int *out_x,
                                 int *out_y);

void scene_editor_canvas_to_normalized(int canvas_x,
                                       int canvas_y,
                                       int canvas_size,
                                       int sx,
                                       int sy,
                                       float *out_x,
                                       float *out_y);

void scene_editor_canvas_draw_background(SDL_Renderer *renderer,
                                         int canvas_x,
                                         int canvas_y,
                                         int canvas_size);

int scene_editor_canvas_hit_test(const FluidScenePreset *preset,
                                 int canvas_x,
                                 int canvas_y,
                                 int canvas_size,
                                 int px,
                                 int py,
                                 EditorDragMode *mode);
int scene_editor_canvas_hit_object(const FluidScenePreset *preset,
                                   int canvas_x,
                                   int canvas_y,
                                   int canvas_size,
                                   int px,
                                   int py);
int scene_editor_canvas_hit_object_handle(const FluidScenePreset *preset,
                                          int canvas_x,
                                          int canvas_y,
                                          int canvas_size,
                                          int px,
                                          int py);
bool scene_editor_canvas_object_handle_point(const FluidScenePreset *preset,
                                             int canvas_x,
                                             int canvas_y,
                                             int canvas_size,
                                             int object_index,
                                             int *out_x,
                                             int *out_y);

void scene_editor_canvas_draw_name(SDL_Renderer *renderer,
                                   int canvas_x,
                                   int canvas_y,
                                   int canvas_size,
                                   TTF_Font *font_main,
                                   TTF_Font *font_small,
                                   const char *name,
                                   bool renaming,
                                   const TextInputField *input);

void scene_editor_canvas_draw_emitters(SDL_Renderer *renderer,
                                       int canvas_x,
                                       int canvas_y,
                                       int canvas_size,
                                       const FluidScenePreset *preset,
                                       int selected_emitter,
                                       int hover_emitter,
                                       TTF_Font *font_small);
void scene_editor_canvas_draw_objects(SDL_Renderer *renderer,
                                      int canvas_x,
                                      int canvas_y,
                                      int canvas_size,
                                      const FluidScenePreset *preset,
                                      int selected_object,
                                      int hover_object);

#endif // SCENE_EDITOR_CANVAS_H
