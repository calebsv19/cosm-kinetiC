#ifndef PHYSICS_SIM_RETAINED_RUNTIME_SCENE_OVERLAY_H
#define PHYSICS_SIM_RETAINED_RUNTIME_SCENE_OVERLAY_H

#include <SDL2/SDL.h>

#include "app/scene_state.h"

bool retained_runtime_scene_overlay_active(const SceneState *scene);
bool retained_runtime_scene_overlay_slice_debug_enabled(const SceneState *scene);
bool retained_runtime_scene_overlay_frame_view(SceneState *scene,
                                               int window_w,
                                               int window_h);
void retained_runtime_scene_overlay_draw(const SceneState *scene,
                                         SDL_Renderer *renderer,
                                         int window_w,
                                         int window_h);

#endif
