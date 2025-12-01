#ifndef PARTICLE_OVERLAY_H
#define PARTICLE_OVERLAY_H

#include <stdbool.h>
#include <SDL2/SDL.h>

#include "app/scene_state.h"

bool particle_overlay_init(int grid_w, int grid_h);
void particle_overlay_shutdown(void);
void particle_overlay_reset(void);
void particle_overlay_update(const SceneState *scene, double dt, bool spawn_enabled);
void particle_overlay_draw(const SceneState *scene,
                           SDL_Renderer *renderer,
                           int window_w,
                           int window_h);

#endif // PARTICLE_OVERLAY_H
