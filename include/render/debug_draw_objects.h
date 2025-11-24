#ifndef DEBUG_DRAW_OBJECTS_H
#define DEBUG_DRAW_OBJECTS_H

#include <SDL2/SDL.h>

#include "app/scene_state.h"

// Draws debug outlines for scene objects (circles/boxes) onto the renderer.
void debug_draw_object_borders(const SceneState *scene,
                               SDL_Renderer *renderer,
                               int window_w,
                               int window_h);

#endif // DEBUG_DRAW_OBJECTS_H
