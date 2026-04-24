#ifndef SCENE_EDITOR_CANVAS_RETAINED_H
#define SCENE_EDITOR_CANVAS_RETAINED_H

#include <SDL2/SDL.h>

#include "app/editor/scene_editor_canvas.h"

typedef struct PhysicsSimDomainOverlay PhysicsSimDomainOverlay;
typedef struct CoreSceneObjectContract CoreSceneObjectContract;

void scene_editor_canvas_draw_retained_origin_axes(SDL_Renderer *renderer,
                                                   const SceneEditorState *state);
void scene_editor_canvas_draw_retained_domain_box(SDL_Renderer *renderer,
                                                  const SceneEditorState *state,
                                                  const PhysicsSimDomainOverlay *domain);
void scene_editor_canvas_draw_retained_object_overlay(SDL_Renderer *renderer,
                                                      const SceneEditorState *state,
                                                      const CoreSceneObjectContract *object,
                                                      SDL_Color color);

#endif /* SCENE_EDITOR_CANVAS_RETAINED_H */
