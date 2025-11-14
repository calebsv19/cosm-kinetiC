#ifndef RENDERER_SDL_H
#define RENDERER_SDL_H

#include <stdbool.h>

#include "app/scene_state.h"

typedef struct RendererHudInfo {
    const char *preset_name;
    bool        preset_is_custom;
    int         grid_w;
    int         grid_h;
    size_t      emitter_count;
    size_t      stroke_samples;
    bool        paused;
} RendererHudInfo;

bool renderer_sdl_init(int windowW, int windowH, int gridW, int gridH);
void renderer_sdl_shutdown(void);
void renderer_sdl_draw(const SceneState *scene,
                       const RendererHudInfo *hud);

#endif // RENDERER_SDL_H
