#ifndef SCENE_EDITOR_CANVAS_RETAINED_H
#define SCENE_EDITOR_CANVAS_RETAINED_H

#include <SDL2/SDL.h>

#include "app/editor/scene_editor_canvas.h"
#include "app/wind_tunnel_3d.h"

typedef struct PhysicsSimDomainOverlay PhysicsSimDomainOverlay;
typedef struct CoreSceneObjectContract CoreSceneObjectContract;
typedef struct PhysicsSimRuntimeMeshPreviewSet PhysicsSimRuntimeMeshPreviewSet;

void scene_editor_canvas_draw_retained_origin_axes(SDL_Renderer *renderer,
                                                   const SceneEditorState *state);
void scene_editor_canvas_draw_retained_domain_box(SDL_Renderer *renderer,
                                                  const SceneEditorState *state,
                                                  const PhysicsSimDomainOverlay *domain);
void scene_editor_canvas_draw_retained_wind_faces(SDL_Renderer *renderer,
                                                  const SceneEditorState *state,
                                                  const PhysicsSimDomainOverlay *domain);
bool scene_editor_canvas_retained_wind_face_at(const SceneEditorState *state,
                                               const PhysicsSimDomainOverlay *domain,
                                               int x,
                                               int y,
                                               WindTunnel3DFace *out_face);
void scene_editor_canvas_draw_retained_mesh_previews(SDL_Renderer *renderer,
                                                     const SceneEditorState *state,
                                                     const PhysicsSimRuntimeMeshPreviewSet *previews);
void scene_editor_canvas_draw_retained_object_overlay(SDL_Renderer *renderer,
                                                      const SceneEditorState *state,
                                                      const CoreSceneObjectContract *object,
                                                      SDL_Color color);

#endif /* SCENE_EDITOR_CANVAS_RETAINED_H */
