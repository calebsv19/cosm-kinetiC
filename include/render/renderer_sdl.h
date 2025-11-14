#ifndef RENDERER_SDL_H
#define RENDERER_SDL_H

#include <stdbool.h>

#include "app/scene_state.h"

bool renderer_sdl_init(int windowW, int windowH, int gridW, int gridH);
void renderer_sdl_shutdown(void);
void renderer_sdl_draw(const SceneState *scene);

#endif // RENDERER_SDL_H
