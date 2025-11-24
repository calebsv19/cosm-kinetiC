#ifndef SCENE_EDITOR_CANVAS_H
#define SCENE_EDITOR_CANVAS_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>

#include "app/scene_presets.h"
#include "ui/text_input.h"

#define SCENE_EDITOR_SELECT_HIGHLIGHT_FACTOR 0.35f
#define SCENE_EDITOR_OBJECT_MIN_RADIUS_PX 6
#define SCENE_EDITOR_OBJECT_MIN_HALF_PX 4
#define SCENE_EDITOR_OBJECT_HANDLE_MARGIN_PX 10
#define SCENE_EDITOR_OBJECT_HANDLE_HIT_RADIUS_PX 12
#define SCENE_EDITOR_BOUNDARY_HIT_MARGIN 16

typedef enum EditorDragMode {
    DRAG_NONE = 0,
    DRAG_POSITION,
    DRAG_DIRECTION
} EditorDragMode;

void scene_editor_canvas_project(int canvas_x,
                                 int canvas_y,
                                 int canvas_w,
                                 int canvas_h,
                                 float px,
                                 float py,
                                 int *out_x,
                                 int *out_y);

void scene_editor_canvas_to_normalized(int canvas_x,
                                       int canvas_y,
                                       int canvas_w,
                                       int canvas_h,
                                       int sx,
                                       int sy,
                                       float *out_x,
                                       float *out_y);

void scene_editor_canvas_draw_background(SDL_Renderer *renderer,
                                         int canvas_x,
                                         int canvas_y,
                                         int canvas_w,
                                         int canvas_h);

int scene_editor_canvas_hit_test(const FluidScenePreset *preset,
                                 int canvas_x,
                                 int canvas_y,
                                 int canvas_w,
                                 int canvas_h,
                                 int px,
                                 int py,
                                 EditorDragMode *mode);
int scene_editor_canvas_hit_object(const FluidScenePreset *preset,
                                   int canvas_x,
                                   int canvas_y,
                                   int canvas_w,
                                   int canvas_h,
                                   int px,
                                   int py);
int scene_editor_canvas_hit_object_handle(const FluidScenePreset *preset,
                                          int canvas_x,
                                          int canvas_y,
                                          int canvas_w,
                                          int canvas_h,
                                          int px,
                                          int py);
bool scene_editor_canvas_object_handle_point(const FluidScenePreset *preset,
                                             int canvas_x,
                                             int canvas_y,
                                             int canvas_w,
                                             int canvas_h,
                                             int object_index,
                                             int *out_x,
                                             int *out_y);
float scene_editor_canvas_object_visual_radius_px(const PresetObject *obj, int canvas_w);
void scene_editor_canvas_object_visual_half_sizes_px(const PresetObject *obj,
                                                     int canvas_w,
                                                     int canvas_h,
                                                     float *out_half_w_px,
                                                     float *out_half_h_px);
float scene_editor_canvas_object_handle_length_px(const PresetObject *obj,
                                                  int canvas_w,
                                                  int canvas_h);

int scene_editor_canvas_hit_edge(int canvas_x,
                                 int canvas_y,
                                 int canvas_w,
                                 int canvas_h,
                                 int px,
                                 int py);

void scene_editor_canvas_draw_boundary_flows(SDL_Renderer *renderer,
                                             int canvas_x,
                                             int canvas_y,
                                             int canvas_w,
                                             int canvas_h,
                                             const BoundaryFlow flows[BOUNDARY_EDGE_COUNT],
                                             int hover_edge,
                                             int selected_edge,
                                             bool edit_mode);

void scene_editor_canvas_draw_tooltip(SDL_Renderer *renderer,
                                      TTF_Font *font,
                                      int x,
                                      int y,
                                      const char *lines[],
                                      int line_count);

void scene_editor_canvas_draw_name(SDL_Renderer *renderer,
                                   int canvas_x,
                                   int canvas_y,
                                   int canvas_w,
                                   int canvas_h,
                                   TTF_Font *font_main,
                                   TTF_Font *font_small,
                                   const char *name,
                                   bool renaming,
                                   const TextInputField *input);

void scene_editor_canvas_draw_emitters(SDL_Renderer *renderer,
                                       int canvas_x,
                                       int canvas_y,
                                       int canvas_w,
                                       int canvas_h,
                                       const FluidScenePreset *preset,
                                       int selected_emitter,
                                       int hover_emitter,
                                       TTF_Font *font_small);
void scene_editor_canvas_draw_objects(SDL_Renderer *renderer,
                                      int canvas_x,
                                      int canvas_y,
                                      int canvas_w,
                                      int canvas_h,
                                      const FluidScenePreset *preset,
                                      int selected_object,
                                      int hover_object);

#endif // SCENE_EDITOR_CANVAS_H
